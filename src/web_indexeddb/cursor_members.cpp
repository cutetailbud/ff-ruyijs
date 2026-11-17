// IDBCursor / IDBCursorWithValue members (docs/owners/INDEXED_DB.md, 2026-10-21):
// iteration requests re-arm the cursor's one request; update/delete are store
// write jobs whose request source is the cursor.
#include "web_indexeddb/cursor.h"
#include "js/Conversions.h"
#include "js/Object.h"
#include "js/PropertyAndElement.h"
#include "js/Wrapper.h"
#include <cmath>

namespace ruyijs::web_indexeddb {
namespace {
enum Member { MAdvance, MContinue, MContinuePrimaryKey, MUpdate, MDelete };
enum Field { FSource, FDirection, FKey, FPrimaryKey, FRequest, FValue };
const char* const kMethods[] = {"advance", "continue", "continuePrimaryKey", "update", "delete"};
State* Self(JSContext* cx, const JS::CallArgs& args, bool needValue, JS::MutableHandleObject out) {
  JS::HandleValue value = args.thisv();
  out.set(value.isObject() ? js::CheckedUnwrapStatic(&value.toObject()) : nullptr);
  const JSClass* clasp = out ? JS::GetClass(out) : nullptr;
  if (!out || (clasp != &kCursorValueClass && (needValue || clasp != &kCursorClass))) {
    WebKit::FailBrand(cx, args, needValue ? "IDBCursorWithValue" : "IDBCursor");
    return nullptr;
  }
  return static_cast<State*>(JS::GetReservedSlot(out, CursorOwner).toPrivate());
}
// The cursor's object store (its source, or its index's store) and transaction.
bool CursorStore(JSContext* cx, JS::HandleObject cursor, JS::MutableHandleObject store, JS::MutableHandleObject tx) {
  JS::RootedObject source(cx, &JS::GetReservedSlot(cursor, CursorSource).toObject());
  store.set(JS::GetClass(source) == &kIndexClass ? &JS::GetReservedSlot(source, IndexStore).toObject() : source.get());
  JS::RootedObject request(cx, &JS::GetReservedSlot(cursor, CursorRequest).toObject());
  tx.set(&JS::GetReservedSlot(request, TransactionSlot).toObject());
  return true;
}
bool Write(State& s, JS::HandleObject cursor, JS::HandleObject store, JS::HandleObject tx, int member,
           JS::HandleValue input, JS::MutableHandleValue result) {
  JSContext* cx = s.cx;
  NativeCursor& native = *CursorData(cursor);
  std::u16string mode;
  JS::RootedValue modeValue(cx, JS::GetReservedSlot(tx, TxMode));
  if (!Text(cx, modeValue, &mode)) return false;
  if (mode != u"readwrite" && mode != u"versionchange") return Throw(s, "ReadOnlyError");
  if (!native.gotValue || native.keyOnly) return Throw(s, "InvalidStateError");
  JS::RootedObject none(cx);
  auto job = std::make_unique<Job>(&s, cx, none, Key{}, 0, false);
  if (member == MUpdate) {
    Key database;
    JS::RootedObject ignored(cx);
    JS::RootedValue clone(cx), path(cx, JS::GetReservedSlot(store, StoreKeyPath)), name(cx, JS::GetReservedSlot(store, StoreName));
    std::u16string storeName;
    if (!CloneWrite(s, input, &job->bytes) || !CloneRead(s, job->bytes, &clone)) return false;
    if (path.isString()) {  // an in-line key must still be the cursor's primary key
      IndexInfo info;
      std::vector<std::u16string> keys;
      if (!Text(cx, path, &storeName)) return false;
      info.paths.push_back(storeName);
      if (!IndexKeysOf(s, info, clone, &keys)) return false;
      if (keys.empty() || keys.front() != native.position.primary) return Throw(s, "DataError");
    }
    if (!StoreConnection(s, store, &database, &ignored) || !Text(cx, name, &storeName) ||
        !RecordIndexKeys(s, database, storeName, clone, &job->indexKeys)) return false;
    job->kind = Put;
    job->recordKey = native.position.primary;
  } else {
    job->kind = Delete;
    job->query.range.lower = job->query.range.upper = native.position.primary;
  }
  JSAutoRealm realm(cx, cursor);
  JS::RootedObject request(cx, NewRequest(s, cursor, tx));
  if (!request) return false;
  job->request = request;
  if (!EnqueueJob(s, std::move(job), tx)) return false;
  result.setObject(*request);
  return true;
}
bool BadKey(const NativeCursor& native, const std::u16string& key, const std::u16string* primary) {
  const bool forward = native.direction == Direction::Next || native.direction == Direction::NextUnique;
  if (!primary) return forward ? key <= native.position.key : key >= native.position.key;
  const auto at = std::make_pair(native.position.key, native.position.primary), to = std::make_pair(key, *primary);
  return forward ? to <= at : to >= at;
}
}  // namespace

bool CursorGet(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  const int field = WebKit::Slot(args);
  JS::RootedObject self(cx);
  if (!Self(cx, args, field == FValue, &self)) return false;
  const uint32_t slots[] = {CursorSource, CursorDirection, CursorKey, CursorPrimary, CursorRequest, CursorValue};
  args.rval().set(JS::GetReservedSlot(self, slots[field]));
  return JS_WrapValue(cx, args.rval());
}
bool CursorMethod(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  const int member = WebKit::Slot(args);
  JS::RootedObject self(cx), store(cx), tx(cx);
  State* s = Self(cx, args, false, &self);
  if (!s) return false;
  const unsigned required = member == MContinuePrimaryKey ? 2 : member == MAdvance || member == MUpdate ? 1 : 0;
  if (argc < required)
    return WebKit::Fail(cx, (std::string("IDBCursor.") + kMethods[member] + ": At least " + std::to_string(required) +
                             " argument" + (required == 1 ? "" : "s") + " required, but only " + std::to_string(argc) +
                             " passed").c_str());
  NativeCursor& native = *CursorData(self);
  CursorStep step;
  if (member == MAdvance) {  // [EnforceRange] unsigned long count
    double count = 0;
    if (!JS::ToNumber(cx, args[0], &count)) return false;
    if (!std::isfinite(count) || std::trunc(count) < 0 || std::trunc(count) > 4294967295.0)
      return WebKit::Fail(cx, "IDBCursor.advance: Value is out of range for unsigned long.");
    if (std::trunc(count) == 0) return WebKit::Fail(cx, "IDBCursor.advance: 0 (Zero) is not a valid advance count.");
    step.count = uint32_t(std::trunc(count));
  }
  CursorStore(cx, self, &store, &tx);
  if (JS::GetReservedSlot(tx, TxActive).toInt32()) return Throw(*s, "TransactionInactiveError");
  if (member == MUpdate || member == MDelete) return Write(*s, self, store, tx, member, args.get(0), args.rval());
  if (member == MContinuePrimaryKey &&
      (!native.hasIndex || native.direction == Direction::NextUnique || native.direction == Direction::PrevUnique))
    return Throw(*s, "InvalidAccessError");
  if (!native.gotValue) return Throw(*s, "InvalidStateError");
  if (member != MAdvance && !args.get(0).isUndefined()) {
    std::u16string key, primary;
    if (!EncodeKey(*s, args[0], &key)) return false;
    if (member == MContinuePrimaryKey && !EncodeKey(*s, args[1], &primary)) return false;
    if (BadKey(native, key, member == MContinuePrimaryKey ? &primary : nullptr)) return Throw(*s, "DataError");
    step.key = key;
    if (member == MContinuePrimaryKey) step.primary = primary;
  }
  args.rval().setUndefined();
  return ScheduleStep(*s, self, step);
}
}  // namespace ruyijs::web_indexeddb

// IDBObjectStore members added with indexes and cursors (docs/owners/INDEXED_DB.md,
// 2026-10-21): add/delete/clear request jobs, createIndex/deleteIndex/index,
// indexNames, autoIncrement, openCursor/openKeyCursor.
#include "web_indexeddb/keys.h"
#include "js/Conversions.h"
#include "js/MapAndSet.h"
#include "js/Object.h"
#include "js/PropertyAndElement.h"
#include "js/Wrapper.h"

namespace ruyijs::web_indexeddb {
namespace {
enum Member { MDelete = 21, MClear, MCreateIndex, MIndex, MDeleteIndex, MOpenCursor, MOpenKeyCursor, MIndexNames,
              MAutoIncrement };
State* Self(JSContext* cx, const JS::CallArgs& args, JS::MutableHandleObject out) {
  JS::HandleValue value = args.thisv();
  out.set(value.isObject() ? js::CheckedUnwrapStatic(&value.toObject()) : nullptr);
  if (!out || JS::GetClass(out) != &kStoreClass) { WebKit::FailBrand(cx, args, "IDBObjectStore"); return nullptr; }
  return static_cast<State*>(JS::GetReservedSlot(out, StoreOwner).toPrivate());
}
bool ReadStoreName(JSContext* cx, JS::HandleObject store, std::u16string* name) {
  JS::RootedValue value(cx, JS::GetReservedSlot(store, StoreName));
  return Text(cx, value, name);
}
// The versionchange (upgrade) state of the store's connection.
bool Upgrading(JS::HandleObject store) {
  return JS::GetReservedSlot(store, StoreTransaction).isNull() &&
         JS::GetReservedSlot(&JS::GetReservedSlot(store, StoreDatabase).toObject(), 8).toBoolean();
}
JSObject* IndexCache(State& s, JS::HandleObject store) {
  JS::Value slot = JS::GetReservedSlot(store, StoreIndexes);
  if (slot.isObject()) return &slot.toObject();
  JSObject* map = JS::NewMapObject(s.cx);
  if (map) JS::SetReservedSlot(store, StoreIndexes, JS::ObjectValue(*map));
  return map;
}
bool CreateIndex(State& s, JS::HandleObject store, const JS::CallArgs& args) {
  JSContext* cx = s.cx;
  if (args.length() < 2)
    return WebKit::Fail(cx, ("IDBObjectStore.createIndex: At least 2 arguments required, but only " +
                             std::to_string(args.length()) + " passed").c_str());
  IndexInfo info;
  bool valid = false;
  if (!Text(cx, args[0], &info.name) || !ValidKeyPath(cx, args[1], &info, &valid)) return false;
  if (!args.get(2).isNullOrUndefined()) {
    if (!args[2].isObject()) return WebKit::Fail(cx, "IDBObjectStore.createIndex: Argument 3 can't be converted to a dictionary.");
    JS::RootedObject options(cx, &args[2].toObject());
    JS::RootedValue v(cx);
    if (!JS_GetProperty(cx, options, "locale", &v)) return false;
    if (!JS_GetProperty(cx, options, "multiEntry", &v)) return false;
    info.multiEntry = JS::ToBoolean(v);
    if (!JS_GetProperty(cx, options, "unique", &v)) return false;
    info.unique = JS::ToBoolean(v);
  }
  if (!Upgrading(store)) return Throw(s, "InvalidStateError");
  Key database;
  JS::RootedObject tx(cx);
  std::u16string storeName;
  std::vector<IndexInfo> existing;
  if (!StoreConnection(s, store, &database, &tx) || !ReadStoreName(cx, store, &storeName) ||
      !s.store->model.Indexes(database, storeName, &existing)) return Throw(s, "InvalidStateError");
  for (const auto& index : existing) if (index.name == info.name) return Throw(s, "ConstraintError");
  if (!valid) return Throw(s, "SyntaxError");
  if (info.array && info.multiEntry) return Throw(s, "InvalidAccessError");
  // Existing records populate the new index (a unique violation is reported here).
  std::vector<Record> records;
  std::vector<std::pair<std::u16string, std::u16string>> entries;
  if (!s.store->model.Records(database, storeName, &records)) return Throw(s, "UnknownError");
  for (const auto& record : records) {
    JS::RootedValue clone(cx);
    std::vector<std::u16string> keys;
    if (!CloneRead(s, record.second, &clone) || !IndexKeysOf(s, info, clone, &keys)) return false;
    for (auto& key : keys) {
      if (info.unique)
        for (const auto& entry : entries) if (entry.first == key) return Throw(s, "ConstraintError");
      entries.push_back({std::move(key), record.first});
    }
  }
  if (!s.store->model.CreateIndex(database, storeName, info, entries)) return Throw(s, "ConstraintError");
  JS::RootedObject index(cx, IndexObject(s, store, info.name));
  if (!index) return false;
  args.rval().setObject(*index);
  return true;
}
}  // namespace

bool StoreConnection(State& s, JS::HandleObject store, Key* database, JS::MutableHandleObject tx) {
  JS::RootedObject db(s.cx, &JS::GetReservedSlot(store, StoreDatabase).toObject());
  Connection* connection = FindConnection(s, db);
  if (!connection) return false;
  *database = connection->key;
  // An upgrade-created store (null StoreTransaction) runs in its connection's
  // versionchange transaction.
  JS::Value value = JS::GetReservedSlot(store, StoreTransaction);
  if (!value.isObject()) value = JS::GetReservedSlot(db, DbUpgrade);
  tx.set(value.isObject() ? &value.toObject() : nullptr);
  return true;
}
// The store's one IDBIndex object per name (store.index(n) === store.index(n)).
JSObject* IndexObject(State& s, JS::HandleObject store, const std::u16string& name) {
  JSContext* cx = s.cx;
  JS::RootedObject cache(cx, IndexCache(s, store));
  JS::RootedValue key(cx), found(cx);
  if (!cache || !String(cx, name, &key) || !JS::MapGet(cx, cache, key, &found)) return nullptr;
  if (found.isObject()) return &found.toObject();
  JS::RootedObject index(cx, s.kit.New(&kIndexClass, s.indexFace.prototype));
  if (!index) return nullptr;
  JS::SetReservedSlot(index, IndexOwner, JS::PrivateValue(&s));
  JS::SetReservedSlot(index, IndexStore, JS::ObjectValue(*store));
  JS::SetReservedSlot(index, IndexName, key);
  JS::SetReservedSlot(index, IndexKeyPath, JS::UndefinedValue());
  found.setObject(*index);
  return JS::MapSet(cx, cache, key, found) ? index.get() : nullptr;
}
bool StoreMember(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  JS::RootedObject self(cx);
  State* s = Self(cx, args, &self);
  if (!s) return false;
  const int member = WebKit::Slot(args);
  Key database;
  JS::RootedObject tx(cx);
  std::u16string storeName, name;
  if (!StoreConnection(*s, self, &database, &tx) || !ReadStoreName(cx, self, &storeName)) return Throw(*s, "InvalidStateError");
  std::vector<IndexInfo> indexes;
  switch (member) {
    case MAutoIncrement: {
      bool value = false;
      if (!s->store->model.AutoIncrement(database, storeName, &value)) return Throw(*s, "InvalidStateError");
      args.rval().setBoolean(value); return true;
    }
    case MIndexNames: {
      if (!s->store->model.Indexes(database, storeName, &indexes)) return Throw(*s, "InvalidStateError");
      JS::RootedObject array(cx, JS::NewArrayObject(cx, 0));
      JS::RootedValue value(cx);
      if (!array) return false;
      for (uint32_t i = 0; i < indexes.size(); ++i)
        if (!String(cx, indexes[i].name, &value) || !JS_SetElement(cx, array, i, value)) return false;
      JS::RootedObject list(cx, s->environment.StringList(array));
      if (!list) return false;
      args.rval().setObject(*list);
      return true;
    }
    case MCreateIndex: return CreateIndex(*s, self, args);
    case MIndex:
    case MDeleteIndex: {
      if (!argc) return WebKit::Fail(cx, member == MIndex ? "IDBObjectStore.index: At least 1 argument required, but only 0 passed"
                                                          : "IDBObjectStore.deleteIndex: At least 1 argument required, but only 0 passed");
      if (!Text(cx, args[0], &name)) return false;
      if (member == MDeleteIndex && !Upgrading(self)) return Throw(*s, "InvalidStateError");
      if (!s->store->model.Indexes(database, storeName, &indexes)) return Throw(*s, "InvalidStateError");
      bool exists = false;
      for (const auto& index : indexes) exists = exists || index.name == name;
      if (!exists) return Throw(*s, "NotFoundError");
      if (member == MIndex) {
        JS::RootedObject index(cx, IndexObject(*s, self, name));
        if (!index) return false;
        args.rval().setObject(*index);
        return true;
      }
      JS::RootedObject cache(cx, IndexCache(*s, self));
      JS::RootedValue key(cx);
      bool removed = false;
      if (!cache || !String(cx, name, &key) || !JS::MapDelete(cx, cache, key, &removed)) return false;
      if (!s->store->model.DeleteIndex(database, storeName, name)) return Throw(*s, "NotFoundError");
      args.rval().setUndefined();
      return true;
    }
    case MOpenCursor:
    case MOpenKeyCursor:
      if (!tx) return Throw(*s, "TransactionInactiveError");
      return OpenCursor(*s, self, tx, nullptr, args.get(0), args.get(1), member == MOpenKeyCursor, args.rval());
    default: break;
  }
  // delete(query) / clear(): readwrite request jobs.
  if (member == MDelete && !argc) return WebKit::Fail(cx, "IDBObjectStore.delete: At least 1 argument required, but only 0 passed");
  if (!tx || JS::GetReservedSlot(tx, TxActive).toInt32()) return Throw(*s, "TransactionInactiveError");
  std::u16string mode;
  JS::RootedValue modeValue(cx, JS::GetReservedSlot(tx, TxMode));
  if (!Text(cx, modeValue, &mode)) return false;
  if (mode != u"readwrite" && mode != u"versionchange") return Throw(*s, "ReadOnlyError");
  Query query;
  if (member == MDelete && !ReadRange(*s, args[0], &query.range, false)) return false;
  {
    JSAutoRealm realm(cx, self);
    JS::RootedObject request(cx, NewRequest(*s, self, tx));
    if (!request) return false;
    auto job = std::make_unique<Job>(s, cx, request, Key{}, 0, false);
    job->kind = member == MDelete ? Delete : Clear;
    job->query = std::move(query);
    if (!EnqueueJob(*s, std::move(job), tx)) return false;
    args.rval().setObject(*request);
  }
  return JS_WrapValue(cx, args.rval());
}
}  // namespace ruyijs::web_indexeddb

// The one production installer for the `console` namespace object. It is a
// plain object whose prototype is Object.prototype (donor `ProtoObjectHack`),
// branded with Symbol.toStringTag and installed once per Window realm.
#include "web_console/internal.h"
#include "runtime/page_helpers.h"
#include "jsapi.h"
#include "js/PropertyAndElement.h"
#include "js/Realm.h"
#include "js/String.h"
#include "js/Symbol.h"

namespace ruyijs::web_console {
namespace {
bool Tag(JSContext* cx, JS::HandleObject object, const char* text) {
  JS::RootedId id(cx, JS::PropertyKey::Symbol(
      JS::GetWellKnownSymbol(cx, JS::SymbolCode::toStringTag)));
  JS::RootedString value(cx, JS_NewStringCopyZ(cx, text));
  return value && JS_DefinePropertyById(cx, object, id, value, JSPROP_READONLY);
}
}

State::State(JSContext* cx, JS::HandleObject page, PageHelpers& helpers, Log* shared)
    : cx(cx), page(cx, page), console(cx), helpers(&helpers), log(shared) {
  if (!log) {
    owned = std::make_unique<Log>();
    log = owned.get();
  }
  realm = log->NewRealm();
}

Realm::Realm(JSContext* cx, JS::HandleObject page, PageHelpers& helpers, Log* shared)
    : state_(std::make_unique<State>(cx, page, helpers, shared)), log_(state_->log) {}
Realm::~Realm() = default;
void Realm::Report(Record record) {
  record.realm = state_->realm;
  record.indent = static_cast<int>(state_->groups.size());
  log_->Append(std::move(record));
}

void Realm::SetClockPort(ClockPort clock, void* context) {
  state_->clock = clock;
  state_->clockContext = context;
}

void Realm::CopyClockFrom(const Realm& other) {
  state_->clock = other.state_->clock;
  state_->clockContext = other.state_->clockContext;
}

bool Realm::Install() {
  State& s = *state_;
  JSAutoRealm realm(s.cx, s.page);
  JS::RootedObject object(s.cx, JS_NewPlainObject(s.cx));
  if (!object) return false;
  for (size_t index = 0; index < MethodCount(); ++index) {
    JS::RootedObject member(s.cx, Method(s, static_cast<uint32_t>(index)));
    if (!member ||
        !JS_DefineProperty(s.cx, object, MethodName(index), member, JSPROP_ENUMERATE) ||
        !s.helpers->Member(member, "console", MethodName(index), "call")) return false;
  }
  if (!Tag(s.cx, object, "Console") ||
      !JS_DefineProperty(s.cx, s.page, "console", object, 0)) return false;
  s.console = object;
  return s.helpers->Object("console", object);
}
}

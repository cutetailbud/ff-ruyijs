#include "browser_environment/state.h"
#include "browser_environment/browser_chrome.h"
#include "runtime/runtime.h"
#include "js/String.h"
#include <utility>
#include <cstring>
#include <iterator>

namespace ruyijs::browser_environment {
namespace {
State* Parent(State& state) {
  JS::RootedObject parent(state.cx, state.parentWindow), prototype(state.cx);
  return WindowGlobalOwner(state.cx, parent, &prototype);
}
bool InheritsOrigin(const State& state) {
  return state.shared && (state.frameUrl.empty() || state.frameUrl == "about:blank" ||
                          state.frameUrl == "about:srcdoc");
}
}
bool State::ReadOriginValue(OriginValue* result) {
  if (worker) {
    if (Model::CopyOriginValue(workerContext.origin, result)) return true;
    JS_ReportOutOfMemory(cx); return false;
  }
  if (InheritsOrigin(*this)) {
    State* parent = Parent(*this);
    return parent && parent->ReadOriginValue(result);
  }
  const bool read = shared ? Model::ReadUrlOrigin(frameUrl, result) : model.ReadOriginValue(result);
  if (!read) JS_ReportOutOfMemory(cx);
  return read;
}
bool State::ReadOrigin(JS::MutableHandleValue result) {
  OriginValue origin;
  if (!ReadOriginValue(&origin)) return false;
  const auto source = origin.text();
  JS::RootedString text(cx, JS_NewStringCopyUTF8N(cx,
      JS::UTF8Chars(source.data(), source.size())));
  if (!text) return false;
  result.setString(text); return true;
}
bool Environment::ReadSourceSnapshot(SourceSnapshot* result) {
  if (!state_) return false;
  JSAutoRealm realm(state_->cx, state_->page);
  SourceSnapshot snapshot;
  if (!state_->Baseline() || !state_->ReadOriginValue(&snapshot.origin) ||
      !state_->ReadSecureContext(&snapshot.secure)) return false;
  *result = std::move(snapshot); return true;
}
bool Environment::ReadDevicePixelRatio(double* result) {
  *result = 1;
  if (!state_ || state_->shared) return true;
  JSAutoRealm realm(state_->cx, state_->page);
  if (!state_->Baseline()) return false;
  JS::RootedValue value(state_->cx);
  if (!state_->Read(Kind::Viewport, kViewportDevicePixelRatio, ValueType::Number, &value)) return false;
  if (value.isNumber() && value.toNumber() > 0) *result = value.toNumber();
  return true;
}
bool Environment::ReadOsLocale(std::string* result) {
  result->clear();
  if (!state_) return false;
  JSAutoRealm realm(state_->cx, state_->page);
  if (!state_->Baseline()) return false;
  JS::RootedValue value(state_->cx);
  if (!state_->Read(Kind::Locale, 1, ValueType::String, &value)) return false;
  if (!value.isString()) return true;
  JS::UniqueChars text = JS_EncodeStringToUTF8(state_->cx, JS::RootedString(state_->cx, value.toString()));
  if (!text) return false;
  *result = text.get();
  return true;
}
bool Environment::ReadLanguage(std::string* result) { return ReadNavigatorText("language", result); }
bool Environment::ReadPlatform(std::string* result) { return ReadNavigatorText("platform", result); }
bool Environment::ReadNavigatorText(const char* name, std::string* result) {
  result->clear();
  if (!state_) return false;
  JSAutoRealm realm(state_->cx, state_->page);
  if (!state_->Baseline()) return false;
  uint32_t field = 0;
  while (field < std::size(kNavigatorFields) && std::strcmp(kNavigatorFields[field].name, name)) ++field;
  JS::RootedValue value(state_->cx);
  if (field == std::size(kNavigatorFields) ||
      !state_->Read(Kind::Navigator, field, ValueType::String, &value)) return false;
  if (!value.isString()) return true;
  JS::UniqueChars text = JS_EncodeStringToUTF8(state_->cx, JS::RootedString(state_->cx, value.toString()));
  if (!text) return false;
  *result = text.get();
  return true;
}
bool Environment::SameOrigin(Environment& other, bool* result) {
  State* left = state_;
  State* right = other.state_;
  // Inherited opaque origins share their original owner identity. Two unrelated
  // serialized "null" origins must never be treated as same origin.
  while (InheritsOrigin(*left)) { left = Parent(*left); if (!left) return false; }
  while (InheritsOrigin(*right)) { right = Parent(*right); if (!right) return false; }
  if (left == right) { *result = true; return true; }
  JSContext* cx = state_->cx;
  JS::RootedValue a(cx), b(cx);
  if (!left->ReadOrigin(&a) || !right->ReadOrigin(&b)) return false;
  JS::RootedString av(cx, a.toString()), bv(cx, b.toString());
  bool opaque = false;
  if (!JS_StringEqualsAscii(cx, av, "null", &opaque)) return false;
  if (opaque) { *result = false; return true; }
  int32_t comparison = 0;
  if (!JS_CompareStrings(cx, av, bv, &comparison)) return false;
  *result = comparison == 0;
  return true;
}
bool State::ReadSecureContext(bool* result) {
  if (worker) { *result = workerContext.secure; return true; }
  if (shared) {
    State* parent = Parent(*this);
    if (!parent || !parent->ReadSecureContext(result)) return false;
    if (!*result || InheritsOrigin(*this)) return true;
  }
  JS::RootedValue value(cx);
  if (!Read(Kind::Location, 0, ValueType::String, &value)) return false;
  std::string address;
  JS::RootedString text(cx, value.toString());
  if (!ToUTF8(cx, text, &address)) return false;
  *result = ruyijs::domain::browser_environment::TrustworthyContextUrl(
      ruyijs::domain::browser_environment::UrlValue(address));
  return true;
}
bool State::IsCrossOriginIsolated() const {
  // Window policy isolation is not supported by the original document owner.
  // Workers transport the creator's actual capability, never infer it from HTTPS.
  return worker && workerContext.isolated;
}
}

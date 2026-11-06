// Shared helpers for the Styling / CSSOM owner: install-time state lookup,
// receiver checks, ASCII string conversion and the property-block codec the
// read-only declaration views use.
#ifndef RUYIJS_WEB_STYLING_INTERNAL_H
#define RUYIJS_WEB_STYLING_INTERNAL_H
#include <string>
#include "js/Conversions.h"
#include "js/Object.h"
#include "js/PropertyAndElement.h"
#include "js/String.h"
#include "jsapi.h"
#include "web_styling/state.h"

namespace ruyijs::web_styling {
struct ComputedStyle;
bool ReadComputed(State&, JS::HandleObject, ComputedStyle*);
bool CascadeStamp(State&, JS::HandleObject, uint64_t*);
bool InstallComputed(State& state);
bool InstallSheets(State& state);
bool InstallSheetClass(State& state);
bool InstallRules(State& state);
bool InstallRuleList(State& state);
bool InstallCSS(State& state);
bool InstallMedia(State& state);
}

namespace ruyijs::web_styling::detail {
// Reads the install-time state from the function's reserved slot. Never call
// this after writing args.rval(): Owner() reads the callee.
inline State* Self(const JS::CallArgs& args) { return static_cast<State*>(WebKit::Owner(args)); }

inline bool CheckReceiver(JSContext* cx, JS::HandleValue self, const JSClass* clasp,
                          JS::MutableHandleObject out, const char* message) {
  return WebKit::Check(cx, self, clasp, out, message);
}

bool AsciiEquals(JSContext* cx, JS::HandleString text, const char* ascii, bool* match);
bool AsciiEqualsCI(JSContext* cx, JS::HandleString text, const char* ascii, bool* match);
bool ToAscii(JSContext* cx, JS::HandleValue value, std::string* out);
bool StringToAscii(JSContext* cx, JS::HandleString text, std::string* out);

// Registers a member on a prototype another owner created (the read-only
// CSSStyleDeclaration view prototype derives from the Document Model one). The
// adding domain owns exactly the descriptors it adds. The prototype is a handle:
// every registration allocates, so a minor GC may move a nursery prototype and a
// raw JSObject* held across the calls would dangle.
inline bool MethodOn(WebKit& kit, JS::HandleObject prototype, const char* iface, const char* name,
                     JSNative native, unsigned length, int32_t slot) {
  WebFace face(kit.cx);
  face.prototype.set(prototype);
  return kit.Method(face, iface, name, native, length, slot);
}
inline bool AccessorOn(WebKit& kit, JS::HandleObject prototype, const char* iface, const char* name,
                       JSNative getter, JSNative setter, int32_t slot) {
  WebFace face(kit.cx);
  face.prototype.set(prototype);
  return kit.Accessor(face, iface, name, getter, setter, slot);
}

// ---- declaration-block codec -------------------------------------------------
// The read-only views store one serialized "name: value;" block. These helpers
// are the single codec for it, so computed and rule styles share one parser.
std::string Trim(const std::string& text);
std::string Lower(const std::string& text);
std::string Dashed(const std::string& idl_name);  // camelCase -> dashed
bool Lookup(const std::string& block, const std::string& name, std::string* value,
            bool* important);
size_t CountProperties(const std::string& block);
bool PropertyNameAt(const std::string& block, size_t index, std::string* name);
std::string Serialize(const std::string& name, const std::string& value, bool important);

bool ReadViewBlock(JSContext*, JS::HandleObject, std::string*);
bool WriteRuleBlock(State&, JS::HandleObject, JS::HandleValue);
bool SetRuleProperty(State&, JS::HandleObject, const std::string&, JS::HandleValue,
                      JS::HandleValue priority, bool remove, JS::MutableHandleValue);

// rule_factory.cpp: the unique stable CSSStyleRule cached by its sheet.
JSObject* NewCssRule(State& state, JS::HandleObject container, size_t index,
                       JS::MutableHandleObject out);
}  // namespace ruyijs::web_styling::detail
#endif

#include "runtime/runtime.h"
#include "runtime/page_reference.h"
#include "runtime/exception_capture.h"
#include "execution_scheduling/realm.h"
#include "browser_environment/cross_origin.h"
#include "jsapi.h"
#include "js/CharacterEncoding.h"
#include "js/CompilationAndEvaluation.h"
#include "js/Exception.h"
#include <algorithm>
#include "js/GlobalObject.h"
#include "js/PropertyAndElement.h"
#include "jsapi.h"
#include "js/Initialization.h"
#include "js/Object.h"
#include "js/Realm.h"
#include "js/RealmOptions.h"
#include "js/SourceText.h"
#include "js/StableStringChars.h"
#include "js/String.h"

namespace ruyijs {
namespace {
PageFrames* FramesOf(JSObject* page) {
  const JS::Value value = JS::GetReservedSlot(page, JSCLASS_GLOBAL_SLOT_COUNT + unsigned(PageReference::Frames));
  return value.isUndefined() ? nullptr : static_cast<PageFrames*>(value.toPrivate());
}
// Firefox release builds resolve three legacy names on a content window
// (nsGlobalWindowInner::DoResolve with USE_CONTROLLERS_SHIM, and
// dom.use_components_shim): `controllers` and `Controllers` as empty objects
// of the XULControllers and Controllers classes (bug 1010577: sites sniff
// Firefox by them), and `Components` as {interfaces: {nsIDOMNode: Node, ...}}.
// Each becomes an own writable, enumerable, configurable data property when
// first looked up; no own-key listing names it before that.
const JSClass kXULControllersShim = {"XULControllers", 0};
const JSClass kControllersShim = {"Controllers", 0};
constexpr struct { const char* gecko; const char* dom; } kInterfaceShims[] = {
    {"nsIXMLHttpRequest", "XMLHttpRequest"}, {"nsIDOMDOMException", "DOMException"},
    {"nsIDOMNode", "Node"}, {"nsIDOMCSSRule", "CSSRule"}, {"nsIDOMEvent", "Event"},
    {"nsIDOMNSEvent", "Event"}, {"nsIDOMKeyEvent", "KeyEvent"}, {"nsIDOMMouseEvent", "MouseEvent"},
    {"nsIDOMMouseScrollEvent", "MouseScrollEvent"}, {"nsIDOMMutationEvent", "MutationEvent"},
    {"nsIDOMUIEvent", "UIEvent"}, {"nsIDOMHTMLMediaElement", "HTMLMediaElement"},
    {"nsIDOMRange", "Range"}, {"nsIDOMNodeFilter", "NodeFilter"}, {"nsIDOMXPathResult", "XPathResult"}};
enum class WindowShim { None, Controllers, ControllersClass, Components };
WindowShim ShimFor(jsid id) {
  if (!id.isString()) return WindowShim::None;
  JSLinearString* name = id.toLinearString();
  if (JS_LinearStringEqualsAscii(name, "controllers")) return WindowShim::Controllers;
  if (JS_LinearStringEqualsAscii(name, "Controllers")) return WindowShim::ControllersClass;
  if (JS_LinearStringEqualsAscii(name, "Components")) return WindowShim::Components;
  return WindowShim::None;
}
bool ResolveShim(JSContext* cx, JS::HandleObject page, JS::HandleId id, WindowShim shim, bool* resolved) {
  JS::RootedObject value(cx);
  if (shim == WindowShim::Components) {
    JS::RootedObject interfaces(cx, JS_NewPlainObject(cx));
    value = JS_NewPlainObject(cx);
    const unsigned fixed = JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_READONLY;
    if (!value || !interfaces || !JS_DefineProperty(cx, value, "interfaces", interfaces, fixed)) return false;
    JS::RootedValue face(cx);
    for (const auto& entry : kInterfaceShims) {
      if (!JS_GetProperty(cx, page, entry.dom, &face)) return false;
      if (face.isObject() && !JS_DefineProperty(cx, interfaces, entry.gecko, face, fixed)) return false;
    }
  } else {
    value = JS_NewObject(cx, shim == WindowShim::Controllers ? &kXULControllersShim : &kControllersShim);
    if (!value) return false;
  }
  if (!JS_DefinePropertyById(cx, page, id, value, JSPROP_ENUMERATE)) return false;
  *resolved = true;
  return true;
}
// Window indexed properties: the standard-class lazy resolution first, then an
// array index below the child count resolves to that child's WindowProxy
// (enumerable, configurable, read-only, like the donor's WindowProxy).
bool PageResolve(JSContext* cx, JS::HandleObject page, JS::HandleId id, bool* resolved) {
  if (!JS_ResolveStandardClass(cx, page, id, resolved)) return false;
  if (*resolved) return true;
  if (const WindowShim shim = ShimFor(id); shim != WindowShim::None)
    return ResolveShim(cx, page, id, shim, resolved);
  if (!id.isInt() || id.toInt() < 0) return true;
  PageFrames* frames = FramesOf(page);
  const size_t index = size_t(id.toInt());
  if (!frames || !frames->count || index >= frames->count(frames->context)) return true;
  JS::RootedValue child(cx);
  if (!frames->at(frames->context, index, &child)) return false;
  if (child.isUndefined()) return true;
  if (!JS_WrapValue(cx, &child) || !JS_DefinePropertyById(cx, page, id, child, JSPROP_ENUMERATE | JSPROP_READONLY))
    return false;
  frames->resolved = std::max(frames->resolved, index + 1);
  *resolved = true;
  return true;
}
bool PageMayResolve(const JSAtomState& names, jsid id, JSObject* maybe) {
  return id.isInt() || ShimFor(id) != WindowShim::None || JS_MayResolveStandardClass(names, id, maybe);
}
// Own keys list the child indices first (Firefox: indexed properties precede
// the named ones), then the standard classes.
bool PageEnumerate(JSContext* cx, JS::HandleObject page, JS::MutableHandleIdVector properties, bool enumerableOnly) {
  if (PageFrames* frames = FramesOf(page); frames && frames->count) {
    const size_t count = frames->count(frames->context);
    // Resolving each index defines it as an own property, which the native
    // enumeration then lists.
    JS::RootedId index(cx);
    for (size_t i = 0; i < count; ++i) {
      bool found = false;
      index = JS::PropertyKey::Int(int32_t(i));
      if (!JS_HasOwnPropertyById(cx, page, index, &found)) return false;
    }
  }
  return JS_NewEnumerateStandardClasses(cx, page, properties, enumerableOnly);
}
const JSClassOps kPageOps = {nullptr, nullptr, nullptr, PageEnumerate, PageResolve, PageMayResolve,
                             nullptr, nullptr, nullptr, JS_GlobalObjectTraceHook};
}  // namespace
static const JSClass kPageClass = {"ruyijsPage", JSCLASS_GLOBAL_FLAGS_WITH_SLOTS(unsigned(PageReference::Count)),
                                   &kPageOps};
void SetPageFrames(JS::HandleObject page, PageFrames* frames) {
  if (!page || JS::GetClass(page) != &kPageClass) return;
  JS::SetReservedSlot(page, JSCLASS_GLOBAL_SLOT_COUNT + unsigned(PageReference::Frames),
                      frames ? JS::PrivateValue(frames) : JS::UndefinedValue());
}
bool InvalidatePageFrames(JSContext* cx, JS::HandleObject page) {
  if (!page || JS::GetClass(page) != &kPageClass) return true;
  PageFrames* frames = FramesOf(page);
  if (!frames || !frames->resolved) return true;
  JSAutoRealm realm(cx, page);
  for (size_t i = 0; i < frames->resolved; ++i) {
    JS::ObjectOpResult removed;
    if (!JS_DeleteElement(cx, page, uint32_t(i), removed)) return false;
  }
  frames->resolved = 0;
  return true;
}

bool ReadPageReference(JS::HandleObject page, PageReference slot, JS::MutableHandleValue value) {
  if (!page || JS::GetClass(page) != &kPageClass || slot >= PageReference::Count) return false;
  value.set(JS::GetReservedSlot(page, JSCLASS_GLOBAL_SLOT_COUNT + unsigned(slot)));
  return true;
}
bool WritePageReference(JS::HandleObject page, PageReference slot, JS::HandleValue value) {
  if (!page || JS::GetClass(page) != &kPageClass || slot >= PageReference::Count) return false;
  JS::SetReservedSlot(page, JSCLASS_GLOBAL_SLOT_COUNT + unsigned(slot), value);
  return true;
}

JSObject* CreatePage(JSContext* cx, JS::HandleObject sameOrigin) {
  JS::RealmOptions options;
  if (sameOrigin) options.creationOptions().setExistingCompartment(sameOrigin);
  else options.creationOptions().setNewCompartmentAndZone();
  // As the donor's window globals (XPCJSContext SetPrefableRealmOptions with
  // javascript.options.shared_memory on; nsGlobalWindowOuter without
  // cross-origin isolation): Atomics exists, the SharedArrayBuffer global
  // constructor is not defined.
  options.creationOptions().setSharedMemoryAndAtomicsEnabled(true)
      .setDefineSharedArrayBufferConstructor(false);
  options.behaviors().setReduceTimerPrecisionCallerType({0});
  JS::RootedObject page(cx, JS_NewGlobalObject(cx, &kPageClass, nullptr,
      JS::DontFireOnNewGlobalHook, options));
  if (!page) return nullptr;
  {
    JSAutoRealm realm(cx, page);
    if (!JS::InitRealmStandardClasses(cx)) return nullptr;
    JS_FireOnNewGlobalObject(cx, page);
  }
  return page;
}

bool ToUTF8(JSContext* cx, JS::HandleString input, std::string* output) {
  JSLinearString* linear = JS_EnsureLinearString(cx, input);
  if (!linear) return false;
  const size_t size = JS::GetDeflatedUTF8StringLength(linear);
  JS::UniqueChars encoded = JS_EncodeStringToUTF8(cx, input);
  if (!encoded) return false;
  output->assign(encoded.get(), size);
  return true;
}

bool Evaluate(JSContext* cx, JS::HandleObject page, JS::HandleString code,
              const char* filename, Checkpoint checkpoint,
              JS::MutableHandleValue value, bool* success, const EvalOptions& evalOptions,
              const char* introduction, bool host, bool fullParse) {
  execution_scheduling::BeginPageOperation(cx, page);
  JS::AutoStableStringChars characters(cx);
  if (!characters.initTwoByte(cx, code)) return false;
  {
    JSAutoRealm realm(cx, page);
    JS::CompileOptions options(cx);
    // A script run synchronously for a page script (a DOM-inserted inline
    // script) names that caller, as the donor ScriptLoader does: the stack
    // then reads "<url> line <n> > injectedScript".
    JS::RootedScript introducer(cx);
    if (introduction) options.setIntroductionInfoToCaller(cx, introduction, &introducer);
    options.setFileAndLine(filename, 1);
    // A page script the document's ScriptLoader compiles in full
    // (ScriptLoader::ApplyDelazifyStrategy): every inner function is parsed
    // with the top level, which is also what names them (Error.stack reads
    // "639/OD/<", a lazily parsed one only "OD/<").
    if (fullParse) options.setEagerDelazificationStrategy(JS::DelazificationOption::ParseEverythingEagerly);
    // Public ruyijs applies offsets only when BOTH values are nonnegative.
    if (evalOptions.line() >= 0 && evalOptions.col() >= 0) {
      options.setFileAndLine(filename, uint32_t(evalOptions.line()) + 1);
      options.setColumn(JS::ColumnNumberOneOrigin(uint32_t(evalOptions.col()) + 1));
    }
    JS::SourceText<char16_t> source;
    if (!source.init(cx, characters.twoByteChars(), characters.length(),
                     JS::SourceOwnership::Borrowed))
      return false;
    JS::RootedScript script(cx, JS::Compile(cx, options, source));
    if (script && host) browser_environment::MarkHostScript(script);
    *success = script && JS_ExecuteScript(cx, script, value);
    if (!*success) {
      CaptureOperationFailure(cx);
      value.setUndefined();
    }
    checkpoint(cx);
    if (JS_IsExceptionPending(cx)) {
      *success = false;
      CaptureOperationFailure(cx);
    }
  }
  return JS_WrapValue(cx, value);
}
bool EvaluateHostExpression(JSContext* cx, const char* filename, const char16_t* chars, size_t length,
                            JS::MutableHandleValue value) {
  JS::CompileOptions options(cx);
  options.setFileAndLine(filename, 1);
  JS::SourceText<char16_t> source;
  if (!source.init(cx, chars, length, JS::SourceOwnership::Borrowed)) return false;
  JS::RootedScript script(cx, JS::Compile(cx, options, source));
  if (!script) return false;
  browser_environment::MarkHostScript(script);
  return JS_ExecuteScript(cx, script, value);
}
}

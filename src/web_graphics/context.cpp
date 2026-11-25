// WebGLRenderingContext: one context object per canvas, created by the canvas
// getContext port and owned by the realm that installed the WebGL interfaces.
#include "web_graphics/internal.h"

#include "js/PropertyAndElement.h"
#include "js/Realm.h"
#include "js/Conversions.h"
#include "js/String.h"
#include "runtime/runtime.h"
#include <string>
#include "runtime/native_constructor_trace.h"
#include "runtime/page_helpers.h"

namespace ruyijs::web_graphics {
const JSClass kContextClass = {"WebGLRenderingContext",
                               JSCLASS_HAS_RESERVED_SLOTS(1)};
const JSClass kContext2Class = {"WebGL2RenderingContext", JSCLASS_HAS_RESERVED_SLOTS(1)};

namespace {
struct Method {
  const char* name;
  JSNative native;
  unsigned length;
};

const Method kMethods[] = {
    {"getParameter", GetParameter, 1},
    {"getSupportedExtensions", GetSupportedExtensions, 0},
    {"getExtension", GetExtension, 1},
    {"getError", GetError, 0},
    {"createBuffer", CreateBuffer, 0},
    {"createTexture", CreateTexture, 0},
    {"createFramebuffer", CreateFramebuffer, 0},
    {"createRenderbuffer", CreateRenderbuffer, 0},
    {"createProgram", CreateProgram, 0},
    {"createShader", CreateShader, 1},
    {"deleteBuffer", DeleteBuffer, 1},
    {"deleteTexture", DeleteTexture, 1},
    {"deleteFramebuffer", DeleteFramebuffer, 1},
    {"deleteRenderbuffer", DeleteRenderbuffer, 1},
    {"deleteProgram", DeleteProgram, 1},
    {"deleteShader", DeleteShader, 1},
    {"isBuffer", IsBuffer, 1},
    {"isTexture", IsTexture, 1},
    {"isFramebuffer", IsFramebuffer, 1},
    {"isRenderbuffer", IsRenderbuffer, 1},
    {"isProgram", IsProgram, 1},
    {"isShader", IsShader, 1},
    {"bindBuffer", BindBuffer, 2},
    {"getUniformLocation", GetUniformLocation, 2},
};

JSObject* NewContext(State& state, Context& entry, JS::MutableHandleObject out) {
  JSContext* cx = state.cx;
  JS::RootedObject prototype(cx, entry.version == 2 ? state.context2.prototype.get() : state.context.prototype.get());
  JS::RootedObject wrapper(cx, state.kit.New(entry.version == 2 ? &kContext2Class : &kContextClass, prototype));
  if (!wrapper) return nullptr;
  JS::SetReservedSlot(wrapper, 0, JS::PrivateValue(&entry));
  out.set(wrapper);
  return wrapper;
}
}  // namespace

bool ContextCtor(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  NativeConstructorTrace trace(cx, args);
  // WebGLRenderingContext has no constructor in the specification.
  return trace.Finish(WebKit::Fail(cx, "Illegal constructor."));
}

// HTML getContext step 1 and the WebGLContextAttributes dictionary init the
// donor's CanvasRenderingContextOptions::Init runs on every getContext call:
// a non-object is null (all defaults); members are read in WebIDL (sorted)
// order, booleans by ToBoolean, powerPreference against its enumeration.
// forceSoftwareRendering ([Func] system principal) and xrCompatible
// (dom.vr.webxr.enabled off) are not members for a content page.
namespace {
constexpr const char* kRequestNames[kAttributeCount] = {
    "alpha", "antialias", "depth", "failIfMajorPerformanceCaveat",
    "powerPreference", "premultipliedAlpha", "preserveDrawingBuffer", "stencil"};
struct RequestedAttributes {
  bool passed[kAttributeCount] = {};
  bool flags[kAttributeCount] = {};
  std::string powerPreference;
};
bool ReadAttributes(JSContext* cx, JS::HandleValue options, RequestedAttributes* out) {
  if (!options.isObject()) return true;
  JS::RootedObject object(cx, &options.toObject());
  JS::RootedValue field(cx);
  for (int32_t index = 0; index < kAttributeCount; ++index) {
    if (!JS_GetProperty(cx, object, kRequestNames[index], &field)) return false;
    if (field.isUndefined()) continue;
    out->passed[index] = true;
    if (index != kPowerPreference) { out->flags[index] = JS::ToBoolean(field); continue; }
    JS::RootedString text(cx, JS::ToString(cx, field));
    if (!text || !ToUTF8(cx, text, &out->powerPreference)) return false;
    if (out->powerPreference != "default" && out->powerPreference != "low-power" &&
        out->powerPreference != "high-performance")
      return WebKit::FailKind(cx, ("'" + out->powerPreference + "' (value of 'powerPreference' member of "
                                   "WebGLContextAttributes) is not a valid value for enumeration "
                                   "WebGLPowerPreference.").c_str(), JSEXN_TYPEERR);
  }
  return true;
}
// The attributes a new context reports: the requested members over the
// profile's answer for a context created without options. A requested
// failIfMajorPerformanceCaveat reads back false: Firefox clears it before
// creating the context (webgl.disable-fail-if-major-performance-caveat).
Attributes Effective(const Attributes& profile, const RequestedAttributes& requested) {
  Attributes result = profile;
  bool* members[] = {&result.alpha, &result.antialias, &result.depth,
                     &result.fail_if_major_performance_caveat, nullptr, &result.premultiplied_alpha,
                     &result.preserve_drawing_buffer, &result.stencil};
  for (int32_t index = 0; index < kAttributeCount; ++index) {
    if (!requested.passed[index]) continue;
    if (index == kPowerPreference) result.power_preference = requested.powerPreference;
    else *members[index] = requested.flags[index];
  }
  if (requested.passed[kFailIfMajorPerformanceCaveat]) result.fail_if_major_performance_caveat = false;
  return result;
}
}  // namespace

bool ContextPort(void* context, JS::HandleObject canvas, JS::HandleString kind,
                 JS::HandleValue options, JS::MutableHandleValue out) {
  State& s = *static_cast<State*>(context);
  bool webgl = false, experimental = false, webgl2 = false;
  if (!detail::AsciiEquals(s.cx, kind, "webgl", &webgl) ||
      !detail::AsciiEquals(s.cx, kind, "experimental-webgl", &experimental) ||
      !detail::AsciiEquals(s.cx, kind, "webgl2", &webgl2))
    return false;
  // Any other context kind stays with the Canvas owner, which returns null.
  if (!webgl && !experimental && !webgl2) {
    out.setNull();
    return true;
  }
  RequestedAttributes requested;
  if (!ReadAttributes(s.cx, options, &requested)) return false;
  Context* entry = s.FindOrCreate(canvas, webgl2 ? 2 : 1);
  if (!entry->wrapper) {
    entry->attributes = Effective(s.model.values().attributes, requested);
    JS::RootedObject wrapper(s.cx);
    if (!NewContext(s, *entry, &wrapper)) return false;
    entry->wrapper = wrapper;
    if (!s.kit.helpers->Object(webgl2 ? "WebGL2RenderingContext" : "WebGLRenderingContext", wrapper)) return false;
  }
  out.setObject(*entry->wrapper);
  return true;
}

bool InstallContext(State& state) {
  WebKit& kit = state.kit;
  JSContext* cx = state.cx;
  JS::RootedObject objectProto(cx, JS::GetRealmObjectPrototype(cx));
  if (!kit.Define("WebGLRenderingContext", &kContextClass, objectProto, ContextCtor,
                  &state.context)) return false;
  if (!kit.Define("WebGL2RenderingContext", &kContext2Class, objectProto,
                  ContextCtor, &state.context2)) return false;
  for (int version = 1; version <= 2; ++version)
    for (const Method& method : kMethods)
      if (!kit.Method(version == 1 ? state.context : state.context2,
          version == 1 ? "WebGLRenderingContext" : "WebGL2RenderingContext",
          method.name, method.native, method.length, version << 16)) return false;
  return true;
}
}  // namespace ruyijs::web_graphics

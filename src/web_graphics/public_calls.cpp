// The public wheel's no-backend calls. No shader compilation or raster output.
#include "web_graphics/internal.h"
#include "web_graphics/public_calls.h"

namespace ruyijs::web_graphics {
bool PublicCall(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  int op = WebKit::Slot(args) & 0xffff;
  State& s = *detail::Self(args);
  JS::RootedObject self(cx);
  if (!detail::ContextReceiver(cx, args, &self)) return false;
  switch (op) {
    case kNull: args.rval().setNull(); return true;
    case kZero: args.rval().setInt32(0); return true;
    case kMinusOne: args.rval().setInt32(-1); return true;
    case kInvalidIndex: args.rval().setNumber(4294967295.0); return true;
    case kFalse: args.rval().setBoolean(false); return true;
    case kFramebufferComplete: args.rval().setInt32(36053); return true;
    case kWaitComplete: args.rval().setInt32(37148); return true;
    case kEmptyString: {
      JS::RootedString text(cx, JS_NewStringCopyZ(cx, ""));
      if (!text) return false;
      args.rval().setString(text); return true;
    }
    case kEmptyArray: {
      JS::RootedObject result(cx, JS::NewArrayObject(cx, 0));
      if (!result) return false;
      args.rval().setObject(*result); return true;
    }
    case kAttributes: {
      // WebGLContextAttributes dictionary members in WebIDL order: what the
      // creating getContext call asked for over the "webgl" profile group.
      const Attributes& attributes = detail::ContextOf(self)->attributes;
      JS::RootedObject result(cx, JS_NewPlainObject(cx));
      if (!result) return false;
      const char* names[] = {"alpha","antialias","depth","failIfMajorPerformanceCaveat",
        "powerPreference","premultipliedAlpha","preserveDrawingBuffer","stencil"};
      const bool members[] = {attributes.alpha, attributes.antialias, attributes.depth,
        attributes.fail_if_major_performance_caveat, false,
        attributes.premultiplied_alpha, attributes.preserve_drawing_buffer,
        attributes.stencil};
      for (int i = 0; i < 8; ++i) {
        JS::RootedValue value(cx, JS::BooleanValue(members[i]));
        if (i == 4) {
          JS::RootedString pref(cx, JS_NewStringCopyN(cx,
              attributes.power_preference.data(), attributes.power_preference.size()));
          if (!pref) return false;
          value.setString(pref);
        }
        if (!JS_DefineProperty(cx, result, names[i], value, JSPROP_ENUMERATE)) return false;
      }
      args.rval().setObject(*result); return true;
    }
    default: args.rval().setUndefined(); return true;
  }
}
bool ContextAttribute(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  int field = WebKit::Slot(args) & 0xffff;
  JS::RootedObject self(cx);
  if (!detail::ContextReceiver(cx, args, &self)) return false;
  Context* context = detail::ContextOf(self);
  if (field == 0) { args.rval().setObject(*context->canvas); return true; }
  if (field >= 3) {
    int color = field == 3 ? context->drawingColorSpace : context->unpackColorSpace;
    JS::RootedString text(cx, JS_NewStringCopyZ(cx, color ? "display-p3" : "srgb"));
    if (!text) return false;
    args.rval().setString(text); return true;
  }
  return JS_GetProperty(cx, context->canvas, field == 1 ? "width" : "height", args.rval());
}
bool SetContextColorSpace(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  int field = WebKit::Slot(args) & 0xffff;
  JS::RootedObject self(cx);
  if (!detail::ContextReceiver(cx, args, &self)) return false;
  JS::RootedString value(cx, JS::ToString(cx, args.get(0)));
  bool srgb = false, p3 = false;
  if (!value || !detail::AsciiEquals(cx, value, "srgb", &srgb) ||
      !detail::AsciiEquals(cx, value, "display-p3", &p3)) return false;
  if (!srgb && !p3) return WebKit::Fail(cx, "Invalid PredefinedColorSpace value");
  auto* context = detail::ContextOf(self);
  (field == 3 ? context->drawingColorSpace : context->unpackColorSpace) = p3 ? 1 : 0;
  args.rval().setUndefined(); return true;
}
}

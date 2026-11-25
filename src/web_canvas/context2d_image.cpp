// getImageData and putImageData. Both read and write the one RGBA store the
// canvas binding owns, so the pixels toDataURL encodes are the same bytes.
#include "web_canvas/internal.h"
#include "web_canvas/realm.h"

#include <cstring>

#include "js/PropertyAndElement.h"
#include "runtime/page_helpers.h"

namespace ruyijs::web_canvas {
namespace {
bool ImageArgument(State& state, JS::HandleValue value, JS::MutableHandleObject out) {
  if (!value.isObject() || JS::GetClass(&value.toObject()) != &kImageDataClass)
    return WebKit::Fail(state.cx, "an ImageData argument is required");
  out.set(&value.toObject());
  return true;
}

// The bounding rectangle of a putImageData dirty region, clipped to the image.
void ClipRect(int64_t x, int64_t y, int64_t w, int64_t h, int64_t limitX, int64_t limitY,
              int64_t* left, int64_t* top, int64_t* right, int64_t* bottom) {
  *left = x < 0 ? 0 : x;
  *top = y < 0 ? 0 : y;
  *right = x + w > limitX ? limitX : x + w;
  *bottom = y + h > limitY ? limitY : y + h;
}
}  // namespace

bool GetImageData(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  State& state = *detail::Self(args);
  JS::RootedObject self(cx);
  if (!detail::ContextReceiver(cx, args, &self)) return false;
  int32_t sx = 0, sy = 0, sw = 0, sh = 0;
  if (!detail::ToLong(cx, args.get(0), &sx) || !detail::ToLong(cx, args.get(1), &sy) ||
      !detail::ToLong(cx, args.get(2), &sw) || !detail::ToLong(cx, args.get(3), &sh))
    return false;
  if (sw == 0 || sh == 0)
    return ThrowDOMException(state, "IndexSizeError", "The source width or height is zero.");
  if (sw < 0 || sh < 0)
    return WebKit::Fail(cx, "getImageData dimensions must be non-negative");
  if (uint64_t(sw) * uint64_t(sh) > kCanvasAreaLimit)
    return ThrowDOMException(state, "IndexSizeError", "The image is too large.");
  JS::RootedObject image(cx);
  if (!NewImageData(state, sw, sh, &image)) {
    if (JS_IsExceptionPending(cx)) return false;
    return ThrowDOMException(state, "IndexSizeError", "The image is too large.");
  }
  JS::RootedObject canvas(cx, detail::ContextCanvas(state, self));
  CanvasBinding* binding = nullptr;
  if (canvas && !EnsureBitmap(state, canvas, &binding)) return false;
  JSObject* bitmap = detail::BitmapOf(binding);
  {
    uint8_t* destination = detail::ViewBytes(detail::ImagePixels(image));
    uint8_t* source = bitmap ? detail::ViewBytes(bitmap) : nullptr;
    if (destination && source && binding) {
      const size_t dstStride = size_t(sw) * 4u;
      const size_t srcStride = size_t(binding->width) * 4u;
      for (int64_t row = 0; row < sh; ++row) {
        const int64_t srcRow = int64_t(sy) + row;
        if (srcRow < 0 || srcRow >= int64_t(binding->height)) continue;
        for (int64_t column = 0; column < sw; ++column) {
          const int64_t srcColumn = int64_t(sx) + column;
          if (srcColumn < 0 || srcColumn >= int64_t(binding->width)) continue;
          // The store is premultiplied; ImageData is not (gfx's reciprocal
          // table unpremultiply, as getImageData uses).
          const uint8_t* pixel = source + size_t(srcRow) * srcStride + size_t(srcColumn) * 4u;
          uint8_t* out = destination + size_t(row) * dstStride + size_t(column) * 4u;
          const unsigned alpha = pixel[3];
          if (alpha == 255 || alpha == 0) {
            std::memcpy(out, pixel, 4u);
            if (alpha == 0) out[0] = out[1] = out[2] = 0;
            continue;
          }
          const uint32_t reciprocal = 0xFF00FFu / alpha;
          for (int channel = 0; channel < 3; ++channel) {
            // UnpremultiplyChunkFallback: the 8.16 product is rounded, so every
            // valid channel reads (c * 255 + alpha / 2) / alpha.
            out[channel] = uint8_t((uint32_t(pixel[channel]) * reciprocal + 0x8000u) >> 16);
          }
          out[3] = uint8_t(alpha);
        }
      }
    }
  }
  // CanvasRenderingContext2D PerturbImageDataArray: the whole result, at the
  // requested origin.
  if (uint8_t* data = detail::ViewBytes(detail::ImagePixels(image)))
    PerturbExportedPixels(data, uint32_t(sw), uint32_t(sh), sw * 4, sx, sy, 1);
  if (!state.kit.helpers->Object("ImageData", image)) return false;
  args.rval().setObject(*image);
  return true;
}

bool PutImageData(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  State& state = *detail::Self(args);
  JS::RootedObject self(cx);
  if (!detail::ContextReceiver(cx, args, &self)) return false;
  args.rval().setUndefined();
  JS::RootedObject image(cx);
  if (!ImageArgument(state, args.get(0), &image)) return false;
  int32_t dx = 0, dy = 0;
  if (!detail::ToLong(cx, args.get(1), &dx) || !detail::ToLong(cx, args.get(2), &dy))
    return false;
  const int32_t width = JS::GetReservedSlot(image, kImageWidthSlot).toInt32();
  const int32_t height = JS::GetReservedSlot(image, kImageHeightSlot).toInt32();
  int32_t dirtyX = 0, dirtyY = 0, dirtyWidth = width, dirtyHeight = height;
  if (args.length() > 3 && !args.get(3).isUndefined()) {
    if (!detail::ToLong(cx, args.get(3), &dirtyX) || !detail::ToLong(cx, args.get(4), &dirtyY) ||
        !detail::ToLong(cx, args.get(5), &dirtyWidth) ||
        !detail::ToLong(cx, args.get(6), &dirtyHeight))
      return false;
  }
  int64_t left = 0, top = 0, right = 0, bottom = 0;
  ClipRect(dirtyX, dirtyY, dirtyWidth, dirtyHeight, width, height, &left, &top, &right, &bottom);
  if (right <= left || bottom <= top) return true;
  JS::RootedObject canvas(cx, detail::ContextCanvas(state, self));
  if (!canvas) return true;
  CanvasBinding* binding = nullptr;
  if (!EnsureBitmap(state, canvas, &binding)) return false;
  JSObject* bitmap = detail::BitmapOf(binding);
  if (!bitmap) return true;
  {
    uint8_t* destination = detail::ViewBytes(bitmap);
    uint8_t* source = detail::ViewBytes(detail::ImagePixels(image));
    if (!destination || !source) return true;
    const size_t dstStride = size_t(binding->width) * 4u;
    const size_t srcStride = size_t(width) * 4u;
    for (int64_t row = top; row < bottom; ++row) {
      const int64_t targetRow = int64_t(dy) + row;
      if (targetRow < 0 || targetRow >= int64_t(binding->height)) continue;
      for (int64_t column = left; column < right; ++column) {
        const int64_t targetColumn = int64_t(dx) + column;
        if (targetColumn < 0 || targetColumn >= int64_t(binding->width)) continue;
        // ImageData is straight alpha, the store is premultiplied.
        const uint8_t* pixel = source + size_t(row) * srcStride + size_t(column) * 4u;
        uint8_t* out = destination + size_t(targetRow) * dstStride + size_t(targetColumn) * 4u;
        const unsigned alpha = pixel[3];
        for (int channel = 0; channel < 3; ++channel) {
          out[channel] = uint8_t((unsigned(pixel[channel]) * alpha + 127) / 255);
        }
        out[3] = uint8_t(alpha);
      }
    }
  }
  return true;
}

bool InstallImageMethods(State& state, WebFace& face) {
  WebKit& kit = state.kit;
  return detail::Method2D(state, face, "createImageData", CreateImageData,
                    1, 0) &&
         detail::Method2D(state, face, "getImageData", GetImageData, 4, 0) &&
         detail::Method2D(state, face, "putImageData", PutImageData, 3, 0);
}
}  // namespace ruyijs::web_canvas

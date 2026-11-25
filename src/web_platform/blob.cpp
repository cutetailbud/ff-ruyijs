// Blob. The bytes and the type live in the object's slots; the constructor
// collects its parts through the shared collector, size/type read them back and
// text/arrayBuffer/slice are the three methods the product exposes. The class is
// shared with File, so every member accepts a File receiver too.
#include "web_platform/state.h"
#include "web_platform/text_codec.h"

#include "runtime/native_constructor_trace.h"
#include "runtime/web_kit.h"
#include "jsapi.h"
#include "js/ArrayBuffer.h"
#include "js/experimental/TypedData.h"
#include "js/Conversions.h"
#include "js/Exception.h"
#include "js/Object.h"
#include "js/PropertyAndElement.h"
#include "js/String.h"
#include "js/experimental/TypedData.h"
#include <algorithm>
#include <cmath>
#include <cstring>

namespace ruyijs::web_platform {
namespace {
// Firefox's receiver error names the member: "'size' called on an object that ...".
bool Brand(JSContext* cx, JS::HandleValue self, JS::MutableHandleObject out, const char* member) {
  if (IsBlob(self)) {
    out.set(&self.toObject());
    return true;
  }
  const std::string message = std::string("'") + member + "' called on an object that does not implement interface Blob.";
  return WebKit::Fail(cx, message.c_str());
}
const char* const kMethodNames[] = {"text", "arrayBuffer", "slice", "bytes", "stream"};

bool ToInteger(JSContext* cx, JS::HandleValue value, double* result) {
  double number = 0;
  if (!JS::ToNumber(cx, value, &number)) return false;
  if (std::isnan(number)) number = 0;
  else if (!std::isinf(number)) number = std::trunc(number);
  *result = number;
  return true;
}

size_t Clamp(double index, size_t length) {
  if (index < 0) return size_t(std::max(double(length) + index, 0.0));
  return size_t(std::min(index, double(length)));
}

bool BytesOf(JSContext* cx, JS::HandleObject self, std::string* bytes) {
  const uint8_t* data = nullptr;
  size_t length = 0;
  if (!BlobBytes(cx, self, &data, &length) || (!data && length)) return false;
  bytes->assign(reinterpret_cast<const char*>(data), length);
  return true;
}

bool Resolved(State& state, JS::HandleValue value, JS::MutableHandleValue result) {
  return state.kit.Resolve(value, result);
}
}  // namespace

bool BlobConstructor(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  State& state = *static_cast<State*>(WebKit::Owner(args));
  NativeConstructorTrace trace(cx, args);
  std::string bytes;
  std::vector<std::pair<size_t, size_t>> strings;
  if (!CollectBlobParts(cx, args.get(0), &bytes, &strings)) return false;
  if (!ApplyBlobEndings(cx, args.get(1), &bytes, strings)) return false;
  std::string type;
  if (!NormalizeTypeOption(cx, args.get(1), &type)) return false;
  JS::RootedObject prototype(cx);
  if (!state.kit.ConstructProto(args, state.blob.prototype, &prototype)) return false;
  JS::RootedObject object(cx, MakeBlob(state, bytes, type, &kBlobClass, prototype));
  if (!object) return false;
  args.rval().setObject(*object);
  return trace.Finish(true);
}

bool BlobGetter(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  JS::RootedObject self(cx);
  if (!Brand(cx, args.thisv(), &self, BlobField(WebKit::Slot(args)) == kBlobSizeField ? "get size" : "get type"))
    return false;
  if (BlobField(WebKit::Slot(args)) == kBlobSizeField) {
    const uint8_t* data = nullptr;
    size_t length = 0;
    if (!BlobBytes(cx, self, &data, &length)) return false;
    args.rval().setNumber(double(length));
    return true;
  }
  args.rval().set(JS::GetReservedSlot(self, kBlobTypeSlot));
  return true;
}

bool BlobMethod(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  State& state = *static_cast<State*>(WebKit::Owner(args));
  JS::RootedObject self(cx);
  if (!Brand(cx, args.thisv(), &self, kMethodNames[WebKit::Slot(args)])) {
    const auto kind = BlobMethodKind(WebKit::Slot(args));
    if (kind == kBlobSlice || kind == kBlobStream) return false;
    JS::RootedValue reason(cx);
    if (!JS_GetPendingException(cx, &reason)) return false;
    JS_ClearPendingException(cx);
    return state.kit.RejectWith(reason, args.rval());
  }
  std::string bytes;
  if (!BytesOf(cx, self, &bytes)) return false;
  switch (BlobMethodKind(WebKit::Slot(args))) {
    case kBlobText: {
      const DecodeResult decoded = DecodeBytes("utf-8", bytes);
      if (decoded.error) { JS_ReportOutOfMemory(cx); return false; }
      JS::RootedString text(cx, JS_NewUCStringCopyN(cx, decoded.text.data(),
                                                    decoded.text.size()));
      JS::RootedValue value(cx);
      if (!text) return false;
      value.setString(text);
      return Resolved(state, value, args.rval());
    }
    case kBlobStream: {
      JS::RootedObject stream(cx);
      if (!state.byteStream) { JS_ReportErrorASCII(cx, "Blob.stream: no stream owner in this realm"); return false; }
      if (!state.byteStream(state.byteStreamContext, bytes, &stream)) return false;
      args.rval().setObject(*stream);
      return JS_WrapValue(cx, args.rval());
    }
    case kBlobBytes:
    case kBlobArrayBuffer: {
      JS::RootedObject buffer(cx, JS::NewArrayBuffer(cx, bytes.size()));
      if (!buffer) return false;
      if (!bytes.empty()) {
        size_t length = 0;
        bool shared = false;
        uint8_t* data = nullptr;
        JS::GetArrayBufferLengthAndData(buffer, &length, &shared, &data);
        memcpy(data, bytes.data(), bytes.size());
      }
      JS::RootedValue value(cx, JS::ObjectValue(*buffer));
      if (BlobMethodKind(WebKit::Slot(args)) == kBlobBytes) {
        JS::RootedObject array(cx, JS_NewUint8ArrayWithBuffer(cx, buffer, 0, -1));
        if (!array) return false;
        value.setObject(*array);
      }
      return Resolved(state, value, args.rval());
    }
    case kBlobSlice: {
      double start = 0, end = double(bytes.size());
      if (argc >= 1 && !args.get(0).isUndefined() && !ToInteger(cx, args.get(0), &start))
        return false;
      if (argc >= 2 && !args.get(1).isUndefined() && !ToInteger(cx, args.get(1), &end))
        return false;
      const size_t begin = Clamp(start, bytes.size());
      const size_t finish = std::max(begin, Clamp(end, bytes.size()));
      std::string type;
      if (argc >= 3 && !args.get(2).isUndefined() && !NormalizeType(cx, args.get(2), &type))
        return false;
      std::string slice(bytes.data() + begin, finish - begin);
      JS::RootedObject object(
          cx, MakeBlob(state, slice, type, &kBlobClass, state.blob.prototype));
      if (!object) return false;
      args.rval().setObject(*object);
      return true;
    }
  }
  return WebKit::Fail(cx, "Unknown Blob member");
}
}  // namespace ruyijs::web_platform

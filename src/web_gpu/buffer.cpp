// GPUBuffer and its map state machine. The buffer owns no bytes; mapAsync
// settles at once and getMappedRange hands back a zero-filled ArrayBuffer, per
// deviation D39. Invalid transitions throw the page realm's OperationError.
#include "web_gpu/internal.h"

#include "js/ArrayBuffer.h"
#include "js/Realm.h"
#include <cmath>

namespace ruyijs::web_gpu {
namespace {

inline constexpr int32_t kUnmapped = 0;
inline constexpr int32_t kPending = 1;
inline constexpr int32_t kMapped = 2;

const char* MapStateName(int32_t state) {
  return state == kPending ? "pending" : state == kMapped ? "mapped" : "unmapped";
}

// Validates one map range against the buffer's size. A zero length or a
// misaligned offset/size is not representable (there are no bytes to map).
bool ValidRange(double size, double offset, double length) {
  return offset >= 0.0 && length > 0.0 && offset + length <= size &&
         std::fmod(offset, 4.0) == 0.0 && std::fmod(length, 4.0) == 0.0;
}

}  // namespace

JSObject* NewBuffer(State& state, JS::HandleObject device, double size, uint32_t usage,
                    JS::HandleString label) {
  (void)device;  // The device is the owner, not a data source for the buffer.
  JSContext* cx = state.cx;
  const JSAutoRealm realm(cx, state.kit.page);
  JS::RootedObject buffer(cx, state.kit.New(&kBufferClass, state.buffer.prototype));
  if (!buffer) return nullptr;
  JS::SetReservedSlot(buffer, kBufferSize, JS::DoubleValue(size));
  JS::SetReservedSlot(buffer, kBufferUsage, JS::NumberValue(usage));
  JS::SetReservedSlot(buffer, kBufferMapState, JS::Int32Value(kUnmapped));
  JS::SetReservedSlot(buffer, kBufferDestroyed, JS::BooleanValue(false));
  JS::SetReservedSlot(buffer, kBufferMapped, JS::NullValue());
  JS::SetReservedSlot(buffer, kBufferLabel, JS::StringValue(label));
  return buffer.get();
}

bool MappedRange(State& state, JS::HandleObject buffer, JS::MutableHandleObject out) {
  JSContext* cx = state.cx;
  JS::RootedValue cached(cx, JS::GetReservedSlot(buffer, kBufferMapped));
  if (cached.isObject()) {
    out.set(&cached.toObject());
    return true;
  }
  const double size = JS::GetReservedSlot(buffer, kBufferSize).toNumber();
  JS::RootedObject range(cx, JS::NewArrayBuffer(cx, static_cast<size_t>(size)));
  if (!range) return false;
  JS::SetReservedSlot(buffer, kBufferMapped, JS::ObjectValue(*range));
  out.set(range);
  return true;
}

bool BufferProperty(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  State* state = static_cast<State*>(WebKit::Owner(args));
  (void)state;
  JS::RootedObject self(cx);
  if (!CheckBuffer(cx, args.thisv(), &self)) return false;
  switch (WebKit::Slot(args)) {
    case kBufferSize:
      args.rval().setDouble(JS::GetReservedSlot(self, kBufferSize).toNumber());
      return true;
    case kBufferUsage:
      args.rval().setNumber(JS::GetReservedSlot(self, kBufferUsage).toNumber());
      return true;
    case kBufferLabel: {
      std::string text;
      if (!SlotString(cx, self, kBufferLabel, &text)) return false;
      return StringValue(cx, text, args.rval());
    }
    default: {
      // A null string must not be boxed: setString(nullptr) is still
      // string-tagged (#46).
      JSString* name = JS_NewStringCopyZ(
          cx, MapStateName(JS::GetReservedSlot(self, kBufferMapState).toInt32()));
      if (!name) return false;
      args.rval().setString(name);
      return true;
    }
  }
}

bool BufferMethod(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  State* state = static_cast<State*>(WebKit::Owner(args));
  JS::RootedObject self(cx);
  if (!CheckBuffer(cx, args.thisv(), &self)) return false;
  const double size = JS::GetReservedSlot(self, kBufferSize).toNumber();
  const bool destroyed = JS::GetReservedSlot(self, kBufferDestroyed).toBoolean();
  const int32_t mapState = JS::GetReservedSlot(self, kBufferMapState).toInt32();
  switch (WebKit::Slot(args)) {
    case kBufferMapAsync: {
      if (destroyed) return RejectOperation(*state, "the buffer is destroyed", args.rval());
      if (mapState != kUnmapped)
        return RejectOperation(*state, "the buffer is already mapped", args.rval());
      uint32_t mode = 0;
      if (!JS::ToUint32(cx, args.get(0), &mode)) return false;
      if (mode != static_cast<uint32_t>(kMapModeRead) &&
          mode != static_cast<uint32_t>(kMapModeWrite))
        return RejectOperation(*state, "mapAsync requires GPUMapMode.READ or WRITE",
                               args.rval());
      double offset = 0.0, length = size;
      if (!args.get(1).isUndefined() && !ReadDouble(cx, args.get(1), &offset))
        return RejectOperation(*state, "the map range must be numeric", args.rval());
      if (!args.get(2).isUndefined() && !ReadDouble(cx, args.get(2), &length))
        return RejectOperation(*state, "the map range must be numeric", args.rval());
      if (!ValidRange(size, offset, length))
        return RejectOperation(*state, "the map range is not valid", args.rval());
      JS::SetReservedSlot(self, kBufferMapState, JS::Int32Value(kMapped));
      return Resolve(*state, JS::UndefinedValue(), args.rval());
    }
    case kBufferGetMappedRange: {
      if (destroyed) return ThrowOperation(*state, "the buffer is destroyed");
      if (mapState != kMapped) return ThrowOperation(*state, "the buffer is not mapped");
      double offset = 0.0, length = size;
      if (!args.get(0).isUndefined() && !ReadDouble(cx, args.get(0), &offset))
        return ThrowOperation(*state, "the mapped range must be numeric");
      if (!args.get(1).isUndefined() && !ReadDouble(cx, args.get(1), &length))
        return ThrowOperation(*state, "the mapped range must be numeric");
      if (!ValidRange(size, offset, length))
        return ThrowOperation(*state, "the mapped range is not valid");
      if (offset == 0.0 && length == size) {
        JS::RootedObject range(cx);
        if (!MappedRange(*state, self, &range)) return false;
        args.rval().setObject(*range);
        return true;
      }
      JS::RootedObject range(cx, JS::NewArrayBuffer(cx, static_cast<size_t>(length)));
      if (!range) return false;
      args.rval().setObject(*range);
      return true;
    }
    case kBufferUnmap: {
      if (mapState != kMapped) return ThrowOperation(*state, "the buffer is not mapped");
      JS::SetReservedSlot(self, kBufferMapState, JS::Int32Value(kUnmapped));
      JS::SetReservedSlot(self, kBufferMapped, JS::NullValue());
      args.rval().setUndefined();
      return true;
    }
    default: {  // destroy
      JS::SetReservedSlot(self, kBufferDestroyed, JS::BooleanValue(true));
      JS::SetReservedSlot(self, kBufferMapState, JS::Int32Value(kUnmapped));
      JS::SetReservedSlot(self, kBufferMapped, JS::NullValue());
      args.rval().setUndefined();
      return true;
    }
  }
}

}  // namespace ruyijs::web_gpu

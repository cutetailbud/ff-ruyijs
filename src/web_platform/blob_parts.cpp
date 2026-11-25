// The shared Blob/File construction helpers: the receiver brand, the byte store
// in the reserved slot, the type normalization and the part collector. Blob and
// File therefore share one byte-accumulation rule and one UTF-8 path, and the
// kBlobByteLimit ceiling is enforced before any Uint8Array is built.
#include "web_platform/state.h"
#include "web_platform/text_codec.h"

#include "runtime/web_kit.h"
#include "jsapi.h"
#include "js/ArrayBuffer.h"
#include "js/Conversions.h"
#include "js/ErrorReport.h"
#include "js/ForOfIterator.h"
#include "js/Object.h"
#include "js/String.h"
#include "web_platform/realm.h"
#include "js/Wrapper.h"
#include "js/experimental/TypedData.h"
#include <cstring>

namespace ruyijs::web_platform {
bool IsBlob(JS::HandleValue value) {
  if (!value.isObject()) return false;
  const JSClass* clasp = JS::GetClass(&value.toObject());
  return clasp == &kBlobClass || clasp == &kFileClass;
}

// Like IsBlob, through a cross-compartment wrapper, for other owners' values.
bool IsBlobValue(JS::HandleValue value) {
  JSObject* object = value.isObject() ? js::CheckedUnwrapStatic(&value.toObject()) : nullptr;
  if (!object) return false;
  const JSClass* clasp = JS::GetClass(object);
  return clasp == &kBlobClass || clasp == &kFileClass;
}

namespace {
bool CopyInto(const uint8_t* data, size_t length, size_t limit, std::string* bytes,
              bool* overflow) {
  if (bytes->size() + length > limit) {
    *overflow = true;
    return true;
  }
  if (data) bytes->append(reinterpret_cast<const char*>(data), length);
  return true;
}

// Appends one Blob part: another blob copies its bytes, a buffer source copies
// its view, everything else is stringified and UTF-8 encoded.
bool AppendPart(JSContext* cx, JS::HandleValue part, std::string* bytes,
                bool* overflow, bool* string_part) {
  *string_part = false;
  if (part.isObject()) {
    JS::RootedObject object(cx, &part.toObject());
    const uint8_t* data = nullptr;
    size_t length = 0;
    bool buffer = false;
    if (IsBlob(part)) {
      JS::RootedValue stored(cx, JS::GetReservedSlot(object, kBlobBytesSlot));
      if (stored.isObject()) {
        JS::RootedObject view(cx, &stored.toObject());
        buffer = true;
        {
          JS::AutoCheckCannotGC nogc;
          bool shared = false;
          data = static_cast<const uint8_t*>(
              JS_GetArrayBufferViewData(view, &shared, nogc));
          length = JS_GetTypedArrayByteLength(view);
        }
      }
    } else if (JS::IsArrayBufferObject(object)) {
      buffer = true;
      JS::AutoCheckCannotGC nogc;
      bool shared = false;
      data = static_cast<const uint8_t*>(JS::GetArrayBufferData(object, &shared, nogc));
      length = JS::GetArrayBufferByteLength(object);
    } else if (JS_IsArrayBufferViewObject(object)) {
      buffer = true;
      JS::AutoCheckCannotGC nogc;
      bool shared = false;
      data = static_cast<const uint8_t*>(JS_GetArrayBufferViewData(object, &shared, nogc));
      length = JS_GetArrayBufferViewByteLength(object);
    }
    if (buffer) return CopyInto(data, length, kBlobByteLimit, bytes, overflow);
  }
  JS::RootedString text(cx, JS::ToString(cx, part));
  if (!text) return false;
  *string_part = true;
  const size_t length = JS_GetStringLength(text);
  std::u16string units(length, u'\0');
  if (length && !JS::CopyStringChars(cx, units.data(), text, length)) return false;
  std::string encoded;
  if (!AppendUtf8(units, &encoded)) {
    *overflow = true;
    return true;
  }
  return CopyInto(reinterpret_cast<const uint8_t*>(encoded.data()), encoded.size(),
                  kBlobByteLimit, bytes, overflow);
}
}  // namespace

bool NormalizeType(JSContext* cx, JS::HandleValue value, std::string* type) {
  type->clear();
  JS::RootedString text(cx, JS::ToString(cx, value));
  if (!text) return false;
  const size_t length = JS_GetStringLength(text);
  std::u16string units(length, u'\0');
  if (length && !JS::CopyStringChars(cx, units.data(), text, length)) return false;
  for (char16_t unit : units) {
    if (unit < 0x20 || unit > 0x7E) {
      type->clear();
      return true;
    }
  }
  for (char16_t unit : units) {
    if (unit >= 'A' && unit <= 'Z') unit = char16_t(unit - 'A' + 'a');
    type->push_back(char(unit));
  }
  return true;
}

bool NormalizeTypeOption(JSContext* cx, JS::HandleValue options, std::string* type) {
  type->clear();
  if (options.isUndefined() || options.isNull()) return true;
  JS::RootedObject object(cx, JS::ToObject(cx, options));
  JS::RootedValue value(cx);
  if (!object) return false;
  if (!JS_GetProperty(cx, object, "type", &value)) return false;
  if (value.isUndefined()) return true;
  return NormalizeType(cx, value, type);
}

bool CollectBlobParts(JSContext* cx, JS::HandleValue parts, std::string* bytes,
                      std::vector<std::pair<size_t, size_t>>* strings) {
  if (parts.isUndefined() || parts.isNull()) return true;
  JS::ForOfIterator iterate(cx);
  if (!iterate.init(parts, JS::ForOfIterator::ThrowOnNonIterable)) return false;
  JS::RootedValue part(cx);
  while (true) {
    bool done = false;
    if (!iterate.next(&part, &done)) return false;
    if (done) break;
    bool overflow = false, string_part = false;
    const size_t start = bytes->size();
    if (!AppendPart(cx, part, bytes, &overflow, &string_part)) return false;
    if (string_part && strings && bytes->size() > start) strings->emplace_back(start, bytes->size());
    if (overflow)
      return WebKit::FailKind(cx, "Blob exceeds the 8 MiB byte limit", JSEXN_RANGEERR);
  }
  return true;
}

bool ApplyBlobEndings(JSContext* cx, JS::HandleValue options, std::string* bytes,
                      const std::vector<std::pair<size_t, size_t>>& strings) {
  if (options.isUndefined() || options.isNull()) return true;
  JS::RootedObject object(cx, JS::ToObject(cx, options));
  JS::RootedValue value(cx);
  if (!object || !JS_GetProperty(cx, object, "endings", &value)) return false;
  if (value.isUndefined()) return true;
  JS::RootedString text(cx, JS::ToString(cx, value));
  if (!text) return false;
  bool native = false, transparent = false;
  if (!JS_StringEqualsAscii(cx, text, "native", &native) ||
      !JS_StringEqualsAscii(cx, text, "transparent", &transparent)) return false;
  if (!native && !transparent) {
    JS::UniqueChars shown = JS_EncodeStringToUTF8(cx, text);
    if (!shown) return false;
    return WebKit::FailKind(cx, (std::string("'") + shown.get() + "' (value of 'endings' member of "
                                 "BlobPropertyBag) is not a valid value for enumeration EndingType.").c_str(),
                            JSEXN_TYPEERR);
  }
  if (!native || strings.empty()) return true;
  // CR LF stays, a lone CR or LF becomes CR LF; only inside string parts
  // (UTF-8 never encodes CR or LF inside a multi-byte sequence).
  std::string out;
  out.reserve(bytes->size() + bytes->size() / 8);
  size_t at = 0;
  for (const auto& [begin, end] : strings) {
    out.append(*bytes, at, begin - at);
    for (size_t i = begin; i < end; ++i) {
      const char c = (*bytes)[i];
      if (c == '\r') { out += "\r\n"; if (i + 1 < end && (*bytes)[i + 1] == '\n') ++i; }
      else if (c == '\n') out += "\r\n";
      else out += c;
    }
    at = end;
  }
  out.append(*bytes, at, std::string::npos);
  if (out.size() > kBlobByteLimit)
    return WebKit::FailKind(cx, "Blob exceeds the 8 MiB byte limit", JSEXN_RANGEERR);
  bytes->swap(out);
  return true;
}

bool BlobBytes(JSContext* cx, JS::HandleObject self, const uint8_t** data,
               size_t* length) {
  *data = nullptr;
  *length = 0;
  JS::RootedValue stored(cx, JS::GetReservedSlot(self, kBlobBytesSlot));
  if (!stored.isObject()) return true;
  JS::RootedObject view(cx, &stored.toObject());
  JS::AutoCheckCannotGC nogc;
  bool shared = false;
  *data = static_cast<const uint8_t*>(JS_GetArrayBufferViewData(view, &shared, nogc));
  *length = JS_GetTypedArrayByteLength(view);
  return true;
}

JSObject* MakeBlob(State& state, const std::string& bytes, const std::string& type,
                   const JSClass* clasp, JS::HandleObject prototype) {
  JSContext* cx = state.cx;
  JS::RootedObject object(cx, state.kit.New(clasp, prototype));
  JS::RootedObject array(cx, JS_NewUint8Array(cx, bytes.size()));
  JS::RootedValue type_value(cx);
  if (!object || !array || !Value(cx, type, &type_value)) return nullptr;
  if (!bytes.empty()) {
    JS::AutoCheckCannotGC nogc;
    bool shared = false;
    uint8_t* data = static_cast<uint8_t*>(JS_GetArrayBufferViewData(array, &shared, nogc));
    if (data) memcpy(data, bytes.data(), bytes.size());
  }
  JS::SetReservedSlot(object, kBlobBytesSlot, JS::ObjectValue(*array));
  JS::SetReservedSlot(object, kBlobTypeSlot, type_value);
  JS::SetReservedSlot(object, kFileSlotName, JS::UndefinedValue());
  JS::SetReservedSlot(object, kFileSlotModified, JS::UndefinedValue());
  return object;
}
}  // namespace ruyijs::web_platform
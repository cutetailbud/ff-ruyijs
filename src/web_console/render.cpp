#include "web_console/render.h"
#include "web_console/internal.h"
#include "jsapi.h"
#include "jsfriendapi.h"
#include "js/Object.h"
#include "js/Wrapper.h"
#include "js/Symbol.h"
#include "js/Conversions.h"
#include "js/Exception.h"
#include "js/SavedFrameAPI.h"
#include "js/Stack.h"
#include "js/String.h"
#include "vm/ErrorObject.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>

namespace ruyijs::web_console {
namespace {
// U+2018/U+2019/U+201C/U+201D as explicit bytes: the donor frontend wraps labels
// in typographic quotes and these sources stay ASCII.
constexpr char kQuoteOpen[] = "\xE2\x80\x9C";
constexpr char kQuoteClose[] = "\xE2\x80\x9D";
constexpr char kApostrophe[] = "\xE2\x80\x99";

std::string Utf8(JSContext* cx, JS::HandleString string, size_t limit) {
  if (!string) return "";
  JS::UniqueChars bytes = JS_EncodeStringToUTF8(cx, string);
  if (!bytes) { JS_ClearPendingException(cx); return ""; }
  size_t length = std::min(std::strlen(bytes.get()), limit);
  while (length && (static_cast<unsigned char>(bytes.get()[length]) & 0xc0) == 0x80)
    --length;
  return std::string(bytes.get(), length);
}

std::string Shown(const std::string& label) {
  return label.empty() ? std::string("<no label>") : label;
}

// The constructor name of an Error's engine type, as Firefox's console prints
// it. JSEXN_ERR and anything unexpected are the base "Error".
const char* ErrorTypeName(JSExnType type) {
  switch (type) {
    case JSEXN_INTERNALERR: return "InternalError";
    case JSEXN_AGGREGATEERR: return "AggregateError";
    case JSEXN_EVALERR: return "EvalError";
    case JSEXN_RANGEERR: return "RangeError";
    case JSEXN_REFERENCEERR: return "ReferenceError";
    case JSEXN_SUPPRESSEDERR: return "SuppressedError";
    case JSEXN_SYNTAXERR: return "SyntaxError";
    case JSEXN_TYPEERR: return "TypeError";
    case JSEXN_URIERR: return "URIError";
    case JSEXN_DEBUGGEEWOULDRUN: return "DebuggeeWouldRun";
    case JSEXN_WASMCOMPILEERROR: return "CompileError";
    case JSEXN_WASMLINKERROR: return "LinkError";
    case JSEXN_WASMRUNTIMEERROR: return "RuntimeError";
    default: return "Error";
  }
}

// `Math.round(duration * 100) / 100` as the frontend prints it, without
// trailing zeros, so a whole millisecond renders as "50" and not "50.00".
std::string Fixed(double value) {
  char buffer[64];
  std::snprintf(buffer, sizeof(buffer), "%.2f", value);
  std::string text(buffer);
  if (text.find('.') != std::string::npos) {
    text.erase(text.find_last_not_of('0') + 1);
    if (!text.empty() && text.back() == '.') text.pop_back();
  }
  return text;
}

std::string Quoted(const std::string& label) {
  return std::string(kQuoteOpen) + label + kQuoteClose;
}

// `stack`'s frames as "name@source:line", innermost first, bounded by
// Log::kStackLimit. Never leaves a pending exception behind.
std::vector<std::string> Frames(JSContext* cx, JS::HandleObject start) {
  std::vector<std::string> frames;
  JS::RootedObject frame(cx, start), parent(cx);
  JS::RootedString name(cx), source(cx);
  while (frame && frames.size() < Log::kStackLimit) {
    uint32_t line = 0;
    if (JS::GetSavedFrameSource(cx, nullptr, frame, &source) != JS::SavedFrameResult::Ok)
      break;
    JS::GetSavedFrameFunctionDisplayName(cx, nullptr, frame, &name);
    JS::GetSavedFrameLine(cx, nullptr, frame, &line);
    frames.push_back(Utf8(cx, name, 128) + "@" + Utf8(cx, source, 512) + ":" +
                     std::to_string(line));
    if (JS::GetSavedFrameParent(cx, nullptr, frame, &parent) != JS::SavedFrameResult::Ok)
      break;
    frame = parent;
  }
  JS_ClearPendingException(cx);
  return frames;
}
}

// A logged argument is kept, not converted: Firefox runs no page code for
// console.log arguments while no console consumer is attached, so an object's
// toString/valueOf/Symbol.toPrimitive/getters must not run here (DataDome's
// devtools probe logs an object with an overridden toString). Objects render as
// their engine class name, symbols as their description.
std::string Render(JSContext* cx, JS::HandleValue value) {
  if (value.isObject()) {
    JSObject* object = js::UncheckedUnwrap(&value.toObject());
    // An Error renders as "Name: message", the line Firefox's console shows,
    // instead of a bare "[object Error]" that names neither. The name and the
    // message are read from the engine's own Error slots, so this still runs
    // no page code: a getter, a replaced `name`, or a `toString` on the object
    // or its prototype is never called (issue #53).
    if (object && object->is<js::ErrorObject>()) {
      auto& error = object->as<js::ErrorObject>();
      std::string text = ErrorTypeName(error.type());
      JS::RootedString message(cx, error.getMessage());
      if (message) {
        const std::string rendered = Utf8(cx, message, State::kLabelLimit);
        if (!rendered.empty()) text += ": " + rendered;
      }
      return text;
    }
    const char* name = JS::GetClass(object)->name;
    return std::string("[object ") + (name ? name : "Object") + "]";
  }
  if (value.isSymbol()) {
    JS::RootedSymbol symbol(cx, value.toSymbol());
    JS::RootedString description(cx, JS::GetSymbolDescription(cx, symbol));
    return "Symbol(" + Utf8(cx, description, State::kLabelLimit) + ")";
  }
  return LabelText(cx, value);
}

std::string LabelText(JSContext* cx, JS::HandleValue value) {
  JS::RootedString text(cx, JS::ToString(cx, value));
  if (!text) { JS_ClearPendingException(cx); return "[unrenderable]"; }
  return Utf8(cx, text, State::kLabelLimit);
}

std::string Join(JSContext* cx, const JS::CallArgs& args, unsigned first) {
  std::string joined;
  for (unsigned index = first; index < args.length(); ++index) {
    if (index != first) joined += ' ';
    joined += Render(cx, args[index]);
    if (joined.size() >= Log::kTextLimit) break;
  }
  if (joined.size() > Log::kTextLimit) joined.resize(Log::kTextLimit);
  return joined;
}

std::string Label(JSContext* cx, const JS::CallArgs& args, const char* fallback) {
  return args.length() ? LabelText(cx, args[0]) : std::string(fallback);
}

std::string Counted(const std::string& label, int count) {
  return Shown(label) + ": " + std::to_string(count);
}

std::string CounterMissing(const std::string& label) {
  return "Counter " + Quoted(label) + " doesn" + kApostrophe + "t exist.";
}

std::string TimerExists(const std::string& label) {
  return "Timer " + Quoted(label) + " already exists.";
}

std::string TimerMissing(const std::string& label) {
  return "Timer " + Quoted(label) + " doesn" + kApostrophe + "t exist.";
}

std::string Timed(const std::string& label, double duration, bool ended) {
  std::string text = label + ": " + Fixed(duration) + "ms";
  return ended ? text + " - timer ended" : text;
}

std::vector<std::string> Capture(JSContext* cx) {
  JS::RootedObject stack(cx);
  if (!JS::CaptureCurrentStack(cx, &stack,
          JS::StackCapture(JS::MaxFrames(Log::kStackLimit + 1)))) {
    JS_ClearPendingException(cx);
    return {};
  }
  return Frames(cx, stack);
}

std::vector<std::string> ArgumentStack(JSContext* cx, const JS::CallArgs& args) {
  // The first logged Error's own saved stack: where that Error was constructed,
  // which is what a page that only logs an error otherwise never reveals. The
  // stack lives on the engine's Error object, so reading it runs no page code.
  for (unsigned index = 0; index < args.length(); ++index) {
    if (!args[index].isObject()) continue;
    JS::RootedObject unwrapped(cx, js::CheckedUnwrapStatic(&args[index].toObject()));
    if (!unwrapped || !unwrapped->is<js::ErrorObject>()) continue;
    JS::RootedObject stack(cx, JS::ExceptionStackOrNull(unwrapped));
    if (!stack) continue;
    std::vector<std::string> frames = Frames(cx, stack);
    if (!frames.empty()) return frames;
  }
  return {};
}
}

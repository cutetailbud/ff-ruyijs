// The 23 content-visible `console` members of the donor namespace
// (dom/webidl/Console.webidl). Each call renders its arguments to bounded text
// at call time and appends one record to the Context log; nothing is written to
// stdout or stderr. Severity, labels and durations come from the donor devtools
// frontend strings rendered in render.cpp.
#include "web_console/internal.h"
#include "web_console/render.h"
#include "runtime/page_helpers.h"
#include "jsapi.h"
#include "js/CallAndConstruct.h"
#include "js/Conversions.h"
#include "js/PropertyAndElement.h"
#include "js/Realm.h"
#include "jsfriendapi.h"
#include <string>
#include <vector>

namespace ruyijs::web_console {
namespace {
enum Kind {
  kPlain, kAssert, kCounter, kCounterReset, kClear, kGroup, kGroupCollapsed,
  kGroupEnd, kTime, kTimeLog, kTimeEnd, kTimeStamp, kProfile, kProfileEnd,
  kException, kTrace,
};
struct MethodDef {
  const char* name;
  const char* level;
  Kind kind;
};
// WebIDL order. `level` is the devtools severity; `kind` the per-member policy.
constexpr MethodDef kDefs[] = {
    {"assert", "error", kAssert},
    {"clear", "log", kClear},
    {"count", "log", kCounter},
    {"countReset", "log", kCounterReset},
    {"debug", "debug", kPlain},
    {"error", "error", kPlain},
    {"info", "info", kPlain},
    {"log", "log", kPlain},
    {"table", "log", kPlain},
    {"trace", "trace", kTrace},
    {"warn", "warn", kPlain},
    {"dir", "log", kPlain},
    {"dirxml", "log", kPlain},
    {"group", "log", kGroup},
    {"groupCollapsed", "log", kGroupCollapsed},
    {"groupEnd", "log", kGroupEnd},
    {"time", "log", kTime},
    {"timeLog", "log", kTimeLog},
    {"timeEnd", "log", kTimeEnd},
    {"_exception", "error", kException},
    {"timeStamp", "log", kTimeStamp},
    {"profile", "log", kProfile},
    {"profileEnd", "log", kProfileEnd},
};
constexpr size_t kCount = sizeof(kDefs) / sizeof(kDefs[0]);

bool Emit(State& state, const MethodDef& def, int indent, std::string text,
          std::vector<std::string> stack = {}) {
  Record record;
  record.realm = state.realm;
  record.method = def.name;
  record.level = def.level;
  record.text = std::move(text);
  record.stack = std::move(stack);
  record.indent = indent;
  state.log->Append(std::move(record));
  return true;
}

bool Perform(State& state, const MethodDef& def, const JS::CallArgs& args) {
  JSContext* cx = state.cx;
  const int indent = static_cast<int>(state.groups.size());
  switch (def.kind) {
    case kAssert:
      if (args.length() > 0 && JS::ToBoolean(args[0])) return true;
      return Emit(state, def, indent, "Assertion failed: " + Join(cx, args, 1));
    case kCounter: {
      const std::string label = Label(cx, args, "default");
      if (state.counters.count(label) == 0 && state.counters.size() >= State::kCounterLimit)
        return Emit(state, def, indent, CounterMissing(label));
      return Emit(state, def, indent, Counted(label, ++state.counters[label]));
    }
    case kCounterReset: {
      const std::string label = Label(cx, args, "default");
      if (state.counters.erase(label) == 0)
        return Emit(state, def, indent, CounterMissing(label));
      state.counters[label] = 0;
      return Emit(state, def, indent, Counted(label, 0));
    }
    case kGroup:
    case kGroupCollapsed: {
      const std::string joined = Join(cx, args, 0);
      state.groups.push_back(joined);
      return Emit(state, def, indent, joined.empty() ? "<no group label>" : joined);
    }
    case kGroupEnd:
      if (!state.groups.empty()) state.groups.pop_back();
      return Emit(state, def, indent, "");
    case kTime: {
      const std::string label = Label(cx, args, "default");
      if (state.timers.count(label) != 0)
        return Emit(state, def, indent, TimerExists(label));
      if (state.timers.size() >= State::kTimerLimit)
        return Emit(state, def, indent,
                    "The maximum allowed number of timers in this page was exceeded.");
      state.timers[label] = state.Now();
      return Emit(state, def, indent, "");
    }
    case kTimeLog:
    case kTimeEnd: {
      const std::string label = Label(cx, args, "default");
      auto found = state.timers.find(label);
      if (found == state.timers.end())
        return Emit(state, def, indent, TimerMissing(label));
      // The one clock is read in seconds; the timer reports milliseconds.
      const double duration = (state.Now() - found->second) * 1000.0;
      const bool ended = def.kind == kTimeEnd;
      if (ended) state.timers.erase(found);
      std::string text = Timed(label, duration, ended);
      if (!ended) {
        std::string rest = Join(cx, args, 1);
        if (!rest.empty()) text += " " + rest;
      }
      return Emit(state, def, indent, text);
    }
    case kClear:
      state.log->ClearRealm(state.realm);
      return true;
    case kTrace:
      return Emit(state, def, indent, Join(cx, args, 0), Capture(cx));
    default:
      // A logged Error carries the stack of where it was constructed. Firefox's
      // console shows it, and it is the only way to see where an Error a page
      // only logs came from, so it is recorded alongside the text (issue #53).
      return Emit(state, def, indent, Join(cx, args, 0), ArgumentStack(cx, args));
  }
}

bool MethodNative(JSContext* cx, unsigned argc, JS::Value* vp) {
  JS::CallArgs args = JS::CallArgsFromVp(argc, vp);
  JS::RootedObject callee(cx, &args.callee());
  auto* state = static_cast<State*>(js::GetFunctionNativeReserved(callee, 0).toPrivate());
  const uint32_t index =
      static_cast<uint32_t>(js::GetFunctionNativeReserved(callee, 1).toInt32());
  args.rval().setUndefined();
  if (!state || index >= kCount) {
    JS_ReportErrorASCII(cx, "console member called without its owner");
    return false;
  }
  return Perform(*state, kDefs[index], args);
}
}

size_t MethodCount() { return kCount; }

const char* MethodName(size_t index) { return index < kCount ? kDefs[index].name : nullptr; }

JSObject* Method(State& state, uint32_t index) {
  JS::RootedFunction function(state.cx, js::NewFunctionWithReserved(
      state.cx, MethodNative, 0, 0, kDefs[index].name));
  if (!function) return nullptr;
  JS::RootedObject object(state.cx, JS_GetFunctionObject(function));
  js::SetFunctionNativeReserved(object, 0, JS::PrivateValue(&state));
  js::SetFunctionNativeReserved(object, 1, JS::Int32Value(static_cast<int32_t>(index)));
  return object;
}
}

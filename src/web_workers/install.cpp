#include "web_workers/state.h"
#include "web_workers/handlers.h"
#include "web_workers/bootstrap.h"
#include "web_workers/error_event.h"
#include "web_workers/options.h"
#include "document_model/exception.h"
#include "runtime/native_constructor_trace.h"
#include "runtime/runtime.h"
#include "interaction/realm.h"
#include "jsapi.h"
#include "jsfriendapi.h"
#include "js/Conversions.h"
#include "js/PropertyAndElement.h"
#include "js/String.h"
#include "js/Realm.h"
#ifdef _WIN32
#include <windows.h>
#elif defined(__APPLE__)
#include <mach-o/dyld.h>
#include <cstdint>
#endif

namespace ruyijs::web_workers {
namespace {
void FinalizeWorker(JS::GCContext*, JSObject* object) {
  auto slot = JS::GetReservedSlot(object, 0);
  if (!slot.isUndefined()) delete static_cast<WorkerRecord*>(slot.toPrivate());
}
const JSClassOps kWorkerOps = {nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
                               FinalizeWorker};
}  // namespace
std::wstring WorkerExecutable() {
#ifdef _WIN32
  std::wstring path(32768, L'\0');
  const DWORD size = GetModuleFileNameW(nullptr, path.data(), DWORD(path.size()));
  if (!size || size >= path.size()) return {};
  path.resize(size);
  return path;
#elif defined(__linux__)
  // The procfs link follows this executable even in a Unicode installation path.
  return L"/proc/self/exe";
#else
  char buffer[4096];
  uint32_t size = sizeof(buffer);
  if (_NSGetExecutablePath(buffer, &size) != 0) return {};
  std::wstring path;
  for (unsigned char ch : std::string(buffer)) path.push_back(wchar_t(ch));
  return path;
#endif
}
namespace {
bool UrlText(JSContext* cx, JS::HandleValue value, std::string* text) {
  JS::RootedString string(cx, JS::ToString(cx, value));
  if (!string) return false;
  return ToUTF8(cx, string, text);
}
}
namespace {
// The refused script's task: the Worker's plain `error` event.
bool FireRefused(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  JS::RootedObject task(cx, &args.callee());  // before rval(), which shares the callee slot
  auto* state = static_cast<State*>(js::GetFunctionNativeReserved(task, 0).toPrivate());
  JS::RootedObject target(cx, &js::GetFunctionNativeReserved(task, 1).toObject());
  args.rval().setUndefined();
  return state->events->Fire(target, "error", false, false);
}
}

// A Worker whose script may not load: no process, one queued `error` event.
static bool RefusedWorker(State& state, const JS::CallArgs& args, JS::HandleObject proto,
                   NativeConstructorTrace& trace) {
  JSContext* cx = state.cx;
  auto* record = new(std::nothrow) WorkerRecord(&state, state.budget);
  JS::RootedObject object(cx, JS_NewObjectWithGivenProto(cx, &kWorkerClass, proto));
  if (!record || !object) { delete record; JS_ReportOutOfMemory(cx); return false; }
  JS::SetReservedSlot(object, 0, JS::PrivateValue(record));
  record->Stop();
  JS::RootedObject handlers(cx, NewHandlerStore(cx, state.handlerSource));
  if (!handlers) return false;
  JS::SetReservedSlot(object, 1, JS::ObjectValue(*handlers));
  if (!state.events->Attach(object)) return false;
  JSFunction* fn = js::NewFunctionWithReserved(cx, FireRefused, 0, 0, "Worker error");
  if (!fn) return false;
  JS::RootedObject fire(cx, JS_GetFunctionObject(fn));
  js::SetFunctionNativeReserved(fire, 0, JS::PrivateValue(&state));
  js::SetFunctionNativeReserved(fire, 1, JS::ObjectValue(*object));
  if (!state.task(state.taskContext, fire, 0)) return false;
  args.rval().setObject(*object);
  return trace.Finish(true);
}
const JSClass kWorkerClass = {"Worker", JSCLASS_HAS_RESERVED_SLOTS(2) |
                              JSCLASS_FOREGROUND_FINALIZE, &kWorkerOps};

bool WorkerConstructor(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  auto* state = static_cast<State*>(WebKit::Owner(args));
  NativeConstructorTrace trace(cx, args);
  if (!state || !args.isConstructing() || !argc)
    return WebKit::Fail(cx, "Worker requires a script URL");
  state->Prune();
  if (!state->task) return WebKit::Fail(cx, "Worker task port is not installed");
  JS::RootedObject proto(cx);
  if (!state->kit.ConstructProto(args, state->worker.prototype, &proto)) return false;
  std::string url;
  std::u16string name;
  if (!UrlText(cx, args[0], &url) || !ReadWorkerName(cx, args.get(1), &name)) return false;
  // A Blob URL carries its script in the startup frame; any other same-origin
  // URL is fetched by the Worker's own network owner (Child::EvaluateStartup),
  // so the creating document never waits for it.
  std::string script;
  bool blocked = false;
  if (url.rfind("blob:", 0) == 0) {
    std::string type;
    if (!state->objectUrl || !state->objectUrl(state->objectUrlContext, url, &script, &type))
      return WebKit::Fail(cx, "Worker script URL could not be loaded");
  } else {
    std::string resolved;
    bool sameOrigin = false;
    JS::RootedObject exception(cx, state->exceptionPrototype);
    if (!state->scriptUrl || !state->scriptUrl(state->scriptUrlContext, url, &resolved, &sameOrigin)) {
      if (JS_IsExceptionPending(cx)) return false;
      return document_model::ThrowException(cx, exception, "SyntaxError",
          ("Worker constructor: Failed to load worker script at \"" + url + "\"").c_str());
    }
    // data: scripts run with an opaque origin; other schemes must be same-origin
    // http(s). A refused script is a load failure: the Worker gets `error` later.
    const bool network = resolved.rfind("http:", 0) == 0 || resolved.rfind("https:", 0) == 0;
    blocked = resolved.rfind("data:", 0) != 0 && !(network && sameOrigin);
    url = std::move(resolved);
  }
  if (blocked) return RefusedWorker(*state, args, proto, trace);
  if (state->budget->active >= Budget::kLimit)
    return WebKit::Fail(cx, "Active Worker limit exceeded");
  auto* record = new(std::nothrow) WorkerRecord(state, state->budget);
  if (!record) { JS_ReportOutOfMemory(cx); return false; }
  if (!state->environment) return WebKit::Fail(cx, "Worker environment port is not installed");
  std::string bootstrap, environment;
  if (!state->environment(state->environmentContext, &environment)) { delete record; return false; }
  if (!EncodeStartup(url, script, &bootstrap, name, environment)) {
    delete record; return WebKit::Fail(cx, "Worker startup frame limit exceeded");
  }
  const std::wstring executable = WorkerExecutable();
  if (executable.empty()) { delete record; return WebKit::Fail(cx, "Worker executable is unavailable"); }
  JS::RootedObject object(cx, JS_NewObjectWithGivenProto(cx, &kWorkerClass, proto));
  if (!record || !object) { delete record; JS_ReportOutOfMemory(cx); return false; }
  JS::SetReservedSlot(object, 0, JS::PrivateValue(record));
  if (state->serviceFactory && !state->serviceFactory(state->serviceContext, &record->service)) {
    record->Stop(); return false;
  }
  JS::RootedObject handlers(cx, NewHandlerStore(cx, state->handlerSource));
  if (!handlers) { record->Stop(); return false; }
  JS::SetReservedSlot(object, 1, JS::ObjectValue(*handlers));
  if (!state->events->Attach(object) || !state->active.append(object)) {
    record->Stop(); return false;
  }
  if (!record->process.Start(executable, std::move(bootstrap))) {
    record->Stop(); state->Prune();
    return WebKit::Fail(cx, "Worker could not be started");
  }
  record->workerId = record->process.id();
  if (!state->Schedule()) { record->Stop(); state->Prune(); return false; }
  args.rval().setObject(*object);
  return trace.Finish(true);
}

bool Install(State& state, interaction::Realm& events, bool sharedWorker) {
  JSContext* cx = state.cx;
  JS::RootedObject parent(cx, events.prototype());
  if (!parent || !state.kit.Define("Worker", &kWorkerClass, parent,
                                   WorkerConstructor, &state.worker)) return false;
  JS::RootedValue eventConstructor(cx);
  if (!JS_GetProperty(cx, parent, "constructor", &eventConstructor) || !eventConstructor.isObject()) return false;
  JS::RootedObject eventCtor(cx, &eventConstructor.toObject());
  if (!JS_SetPrototype(cx, state.worker.constructor, eventCtor)) return false;
  JS::RootedValue constructor(cx), prototype(cx);
  if (!JS_GetProperty(cx, state.kit.page, "DOMException", &constructor) || !constructor.isObject()) return false;
  JS::RootedObject exception(cx, &constructor.toObject());
  if (!JS_GetProperty(cx, exception, "prototype", &prototype) || !prototype.isObject()) return false;
  state.exceptionPrototype = &prototype.toObject();
  state.errorPrototype = ErrorPrototype(cx, state.kit.page);
  state.handlerSource = NewHandlerStore(cx, parent);
  state.pollTask = state.kit.Function(Poll, "workerPoll", 0, 0);
  if (!state.errorPrototype || !state.handlerSource || !state.pollTask ||
      !JS_DefineProperty(cx, state.worker.constructor, "length", 1, JSPROP_READONLY) ||
      !InstallHandlers(cx, *state.kit.helpers, state.worker.prototype)) return false;
  // SharedWorker is Exposed=Window only: a Worker realm must not install it.
  if (sharedWorker) {
    if (!state.kit.Define("SharedWorker", &kSharedWorkerClass, parent,
                          SharedWorkerConstructor, &state.sharedWorker) ||
        !JS_SetPrototype(cx, state.sharedWorker.constructor, eventCtor) ||
        !JS_DefineProperty(cx, state.sharedWorker.constructor, "length", 1, JSPROP_READONLY) ||
        !InstallHandlers(cx, *state.kit.helpers, state.sharedWorker.prototype, nullptr, nullptr,
                         HandlerExposure::SharedWorker) ||
        !state.kit.Accessor(state.sharedWorker, "SharedWorker", "port", SharedWorkerPort, nullptr, 0)) return false;
  }
  return state.kit.Method(state.worker, "Worker", "terminate", WorkerTerminate, 0, 0) &&
         state.kit.Method(state.worker, "Worker", "postMessage", WorkerPostMessage, 1, 1);
}
}

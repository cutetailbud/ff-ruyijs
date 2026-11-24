#include "runtime/runtime_internal.h"
#include "runtime/pipe.h"
#include "runtime/host_bridge.h"
#include "runtime/host_values.h"
#include "runtime/environment_control.h"
#include "runtime/scheduling_control.h"
#include "runtime/resource_control.h"
#include "runtime/cookie_control.h"
#include "runtime/console_control.h"
#include "runtime/page_load.h"
#include "runtime/api_monitor.h"
#include "runtime/garbage_collection.h"
#include "runtime/random_control.h"
#include "runtime/input_control.h"
#include "runtime/user_input.h"
#include "runtime/top_level.h"
#include "jsapi.h"
#include "js/Exception.h"
#include "js/PropertyAndElement.h"
#include "js/String.h"

namespace ruyijs {
static bool Failure(JSContext* cx, JS::MutableHandleValue value) {
  return FormatOperationFailure(cx, value);
}

bool Serve(JSContext* cx, TopLevel& top,
                  JS::HandleObject codec, const char* name, Checkpoint checkpoint,
                  ExceptionCapture& errors, EnvironmentControl& environment,
                  SchedulingControl& scheduling, ResourceControl& resources, ConsoleControl& console,
                  PageLoad& pages, ApiMonitor& monitor) {
  Pipe pipe;
  if (!pipe.Connect(name)) {
    JS_ReportErrorASCII(cx, "ruyijs pipe connection failed");
    return false;
  }
  HostBridge bridge(cx, top.Page(), codec, pipe, checkpoint, *top.Active().helpers);
  HostValues hostValues(cx, top.Page(), bridge);
  // A new top-level document gets the host registrations before its scripts
  // run; the port is cleared before the bridge goes away.
  struct Host {
    HostBridge& bridge;
    HostValues& values;
  } host{bridge, hostValues};
  struct HostPort {
    TopLevel& top;
    ~HostPort() { top.SetHostPort(nullptr, nullptr); }
  } hostPort{top};
  top.SetHostPort([](void* context, BrowsingContext& target) {
    auto& session = *static_cast<Host*>(context);
    JS::RootedObject global(target.cx, target.page);
    session.values.Rebind(global);
    return session.bridge.Activate(global, *target.helpers);
  }, &host);
  std::string frame;
  while (pipe.Read(&frame)) {
    JS::RootedObject page(cx, top.Page());
    errors.BeginRequest();
    hostValues.Begin();
    scheduling.BeginRequest();
    JS::RootedObject request(cx);
    JS::RootedValue id(cx, JS::NullValue()), value(cx);
    std::string operation;
    bool closing = false;
    bool success = ParseRequest(cx, frame, &request);
    if (success) success = JS_GetProperty(cx, request, "id", &id);
    if (success && !(id.isNumber() || id.isString())) {
      id.setNull();
      JS_ReportErrorASCII(cx, "RPC id must be a number or string");
      success = false;
    }
    if (success) success = StringProperty(cx, request, "op", &operation);
    if (success && (operation == "eval" || operation == "set" || operation == "expose" ||
                    operation == "expose_data" || operation == "native" || operation == "page" || operation == "input" || operation == "user" ||
                    operation == "locals")) {
      environment.Freeze();
      scheduling.Freeze();
    }
    if (success && operation == "eval") {
      if (!EvalRequest(cx, page, request, hostValues, checkpoint, &value, &success)) return false;
    } else if(success && operation=="locals") {
      JS::RootedValue global(cx, JS::ObjectValue(*page));
      success=hostValues.Encode(global,false,&value);
      if(!success&&!Failure(cx,&value))return false;
    } else if(success && operation=="host_value") {
      success=hostValues.Request(request,&value);
      if(!success&&!Failure(cx,&value))return false;
    } else if (success && (operation == "expose" || operation == "set" ||
                           operation == "expose_data" || operation == "native")) {
      JS::RootedValue nameValue(cx);
      JS::RootedString binding(cx);
      JS::RootedValue input(cx);
      success = JS_GetProperty(cx, request, "name", &nameValue);
      if (success && !nameValue.isString()) {
        JS_ReportErrorASCII(cx, "Host binding name must be a string");
        success = false;
      }
      if (success) binding = nameValue.toString();
      if (success && operation == "expose") success = bridge.Expose(binding);
      else if (success && operation == "native") {
        JS::RootedValue constructor(cx);
        bool hasConstructor = false;
        success = JS_HasOwnProperty(cx, request, "constructor", &hasConstructor) &&
                  (!hasConstructor || JS_GetProperty(cx, request, "constructor", &constructor)) &&
                  JS_GetProperty(cx, request, "code", &input);
        if (success && !constructor.isUndefined() && !constructor.isBoolean()) {
          JS_ReportErrorASCII(cx, "Native constructor option must be a boolean");
          success = false;
        }
        if (success && !input.isString()) {
          JS_ReportErrorASCII(cx, "Native wrapper code must be a string");
          success = false;
        }
        if (success) {
          JS::RootedString code(cx, input.toString());
          success = bridge.NativeWrap(binding, code, constructor.isTrue());
        }
      }
      else if (success) success = JS_GetProperty(cx, request, "value", &input) &&
          (operation == "expose_data" ? bridge.ExposeData(binding, input) : bridge.Set(binding, input));
      if (!success && !Failure(cx, &value)) return false;
    } else if (success && (operation == "configure" || operation == "defaults")) {
      success = environment.Request(operation, request, &value);
      if (!success && !Failure(cx, &value)) return false;
    } else if (success && operation == "configure_random") {
      success = ConfigureRandom(cx, page, request, environment.Frozen(), &value);
      if (!success && !Failure(cx, &value)) return false;
    } else if (success && operation == "configure_crypto_replay") {
      success = pages.ConfigureCrypto(request, environment.Frozen(), &value);
      if (!success && !Failure(cx, &value)) return false;
    } else if (success && operation == "crypto_replay_status") {
      success = pages.CryptoStatus(&value);
      if (!success && !Failure(cx, &value)) return false;
    } else if (success && operation == "document_start") {
      success = pages.DocumentStartRequest(request, &value);
      if (!success && !Failure(cx, &value)) return false;
    } else if (success && operation == "event_loop") {
      success = scheduling.Request(request, &value);
      if (!success && !Failure(cx, &value)) return false;
    } else if (success && operation == "timer_realm") {
      success = scheduling.TimerRealm(page, request, &value);
      if (!success && !Failure(cx, &value)) return false;
    } else if (success && operation == "resource") {
      success = resources.Request(request, &value);
      if (!success && !Failure(cx, &value)) return false;
    } else if (success && operation == "cookies") {
      success = CookieRequest(cx, top.Active().navigatorNetwork->Resources(), request, &value);
      if (!success && !Failure(cx, &value)) return false;
    } else if (success && operation == "console") {
      success = console.Request(request, &value);
      if (!success && !Failure(cx, &value)) return false;
    } else if (success && operation == "api_log") {
      success = monitor.Request(request, &value);
      if (!success && !Failure(cx, &value)) return false;
    } else if (success && operation == "page") {
      success = pages.Request(request, &value);
      if (!success && !Failure(cx, &value)) return false;
    } else if (success && operation == "input") {
      success = InputRequest(cx, page, request, scheduling, &value);
      if (!success && !Failure(cx, &value)) return false;
    } else if (success && operation == "user") {
      success = UserInputRequest(cx, top, request, scheduling, &value);
      if (!success && !Failure(cx, &value)) return false;
    } else if (success && operation == "collect") {
      CollectGarbage(cx, &value);
    } else if (success && operation == "checkpoint") {
      checkpoint(cx);
      if (JS_IsExceptionPending(cx)) {
        success = false;
        if (!Failure(cx, &value)) return false;
      }
    } else if (success && operation == "close") {
      closing = true;
      checkpoint(cx);
    } else {
      if (success) JS_ReportErrorASCII(cx, "Unknown ruyijs RPC operation");
      success = false;
      if (!Failure(cx, &value)) return false;
    }
    if (!Reply(cx, codec, id, success, &value, checkpoint, errors, &frame, &success)) return false;
    hostValues.Finish(success);
    if (!pipe.Write(frame) || closing) return true;
    if (!bridge.healthy()) return false;
  }
  return true;
}

}

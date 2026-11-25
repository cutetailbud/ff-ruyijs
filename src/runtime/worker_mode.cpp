#include "runtime/interface_objects.h"
#include "runtime/runtime.h"
#include "runtime/pipe.h"
#include "runtime/exception_capture.h"
#include "runtime/page_helpers.h"
#include "runtime/worker_owners.h"
#include "runtime/binding_order.h"
#include "interaction/realm.h"
#include "document_model/exception.h"
#include "resource_network/realm.h"
#include "runtime/blob_stream_port.h"
#include "execution_scheduling/realm.h"
#include "browser_environment/environment.h"
#include "web_platform/realm.h"
#include "web_media/realm.h"
#include "web_geometry/realm.h"
#include "web_crypto/realm.h"
#include "web_trusted_types/realm.h"
#include "web_workers/child.h"
#include "web_messaging/realm.h"
#include "web_console/realm.h"
#include "web_canvas/realm.h"
#include "web_graphics/realm.h"
#include "font_loading/realm.h"
#include "web_service_worker/realm.h"
#include "web_observers/realm.h"
#include "web_workers/realm.h"
#include "runtime/worker_nested.h"
#include "runtime/broadcast_ports.h"
#include "web_broadcast/realm.h"
#include "web_rtc/realm.h"
#include <cstring>
#include "jsapi.h"
#include "js/Realm.h"
#include "shell/jsoptparse.h"

namespace ruyijs {
bool RunWorker(JSContext* cx, js::cli::OptionParser* options, Checkpoint checkpoint) {
  JS_SetGCParameter(cx, JSGC_COMPACTING_ENABLED, 0);
  const char* name = options->getStringOption("ruyijs-worker");
  if (!name) { JS_ReportErrorASCII(cx, "ruyijs worker requires a pipe name"); return false; }
  Pipe pipe;
  if (!pipe.ConnectAsync(name)) {
    JS_ReportErrorASCII(cx, "ruyijs worker pipe connection failed"); return false;
  }
  // A service worker process (Service Worker owner), a shared worker, or a
  // dedicated Worker. The string is written by Process::Start.
  const char* kind = options->getStringOption("ruyijs-worker-kind");
  const bool service = kind && !std::strcmp(kind, "service");
  const bool shared = kind && !std::strcmp(kind, "shared");
  const web_workers::WorkerKind workerKind = service ? web_workers::WorkerKind::Service :
      shared ? web_workers::WorkerKind::Shared : web_workers::WorkerKind::Dedicated;
  JS::RootedObject page(cx, CreatePage(cx));
  if (!page) return false;
  JSAutoRealm realm(cx, page);
  PageHelpers helpers(cx, page);
  interaction::Realm events(cx, page);
  if (!events.Install(helpers) || !events.InstallWorkerDerived(service, !service && !shared)) return false;
  // Event init dictionary brands in a worker: RTCTransformEventInit.transformer
  // (brand 3, the Web RTC owner's RTCRtpScriptTransformer). The port is stateless
  // and lives as long as `events`; Element/DOMException brands do not exist here.
  events.SetBrandPort([](void*, int brand, JS::HandleValue value, bool* matches) {
    *matches = brand == 3 && value.isObject() && web_rtc::IsScriptTransformer(&value.toObject());
    return true;
  }, nullptr);
  JS::RootedObject eventTargetPrototype(cx, events.prototype());
  if (!eventTargetPrototype || !events.Attach(page)) return false;
  JS::RootedObject exceptionPrototype(cx), exceptionConstructor(cx);
  if (!document_model::InstallExceptionSurface(cx, page, &exceptionPrototype, &exceptionConstructor,
                                               false) ||
      !helpers.Interface("DOMException", exceptionPrototype) ||
      !helpers.Constructor("DOMException", exceptionConstructor)) return false;
  execution_scheduling::Realm scheduling(cx, page, checkpoint);
  if (!scheduling.Install(helpers, events, execution_scheduling::Exposure::Worker)) return false;
  resource_network::Realm resources(cx, page, helpers);
  resources.SetTransportExceptionPorts({&document_model::CreateDerivedException,
                                       &document_model::DerivedExceptionData});
  if (!resources.InstallCancellation(helpers, events) ||
      !resources.InstallWorkerStreams(helpers)) return false;
  resources.SetCallbackEntryPort([](void* context,JS::HandleObject global) {
    return !static_cast<execution_scheduling::Realm*>(context)->Retired(global);
  },&scheduling);
  resources.SetAbortDelayPort([](void* context, JS::HandleObject handler, double delay) {
    return static_cast<execution_scheduling::Realm*>(context)->PostDelayedTask(handler, delay);
  }, &scheduling);
  // Native buffered Body EOF completion shares the original resource task port.
  resources.SetKeepaliveTaskPort([](void* context, JS::HandleObject handler) {
    return static_cast<execution_scheduling::Realm*>(context)->PostKeepaliveTask(handler);
  }, &scheduling);
  struct AbortBinding {
    resource_network::Realm& resources;
    ~AbortBinding() {
      resources.SetCallbackEntryPort(nullptr, nullptr);
      resources.SetAbortDelayPort(nullptr, nullptr);
      resources.SetKeepaliveTaskPort(nullptr, nullptr);
    }
  } abortBinding{resources};
  if (!scheduling.InstallTasks(page, helpers, events)) return false;
  events.SetMicrotaskCheckpointPort([](void* context) {
    static_cast<execution_scheduling::Realm*>(context)->PerformMicrotaskCheckpoint();
  }, &scheduling);
  struct CheckpointPort { interaction::Realm& events; ~CheckpointPort() { events.SetMicrotaskCheckpointPort(nullptr, nullptr); } }
      checkpointPort{events};
  // console: the Worker realm's own bounded log on this realm's one clock.
  web_console::Realm console(cx, page, helpers, nullptr);
  if (!console.Install()) return false;
  console.SetClockPort([](void* context) {
    return static_cast<execution_scheduling::Realm*>(context)->ClockSeconds();
  }, &scheduling);
  struct ConsoleClock { web_console::Realm& console; ~ConsoleClock() { console.SetClockPort(nullptr, nullptr); } }
      consoleClock{console};
  web_messaging::Realm messaging(cx, page, helpers, events);
  if (!messaging.Install()) return false;
  messaging.SetTaskPort([](void* context, JS::HandleObject task) {
    return static_cast<execution_scheduling::Realm*>(context)->PostTask(task);
  }, &scheduling);
  web_platform::Realm platform(cx, page, helpers, events);
  BindBlobStreams(platform, resources);
  browser_environment::Environment environment(cx, page);
  platform.SetLineBreakPort([](void* context) {
    // navigator.platform "Win32" (and an unset one: the default profile is
    // Windows) writes CR LF; "MacIntel", "Linux x86_64"... write LF.
    std::string platform;
    return !static_cast<browser_environment::Environment*>(context)->ReadPlatform(&platform) ||
           platform.empty() || platform.rfind("Win", 0) == 0;
  }, &environment);
  struct LineBreakBinding { web_platform::Realm& platform; ~LineBreakBinding() { platform.SetLineBreakPort(nullptr, nullptr); } }
      lineBreakBinding{platform};
  // DOMException errors of Environment members (atob's InvalidCharacterError)
  // use this Worker realm's one DOMException prototype.
  struct ExceptionBinding {
    JSContext* cx;
    JS::HandleObject prototype;
    browser_environment::Environment& environment;
    ~ExceptionBinding() {
      environment.SetClonePort(nullptr, nullptr);
      environment.SetExceptionPort(nullptr, nullptr);
    }
  } exceptionBinding{cx, exceptionPrototype, environment};
  environment.SetExceptionPort([](void* context, const char* name, const char* message) {
    auto& binding = *static_cast<ExceptionBinding*>(context);
    return document_model::ThrowException(binding.cx, binding.prototype, name, message);
  }, &exceptionBinding);
  // Worker structuredClone uses the existing MessageChannel clone owner.
  environment.SetClonePort([](void* context, JS::HandleValue value, JS::HandleValue transfer,
      JS::HandleObject destination, JS::MutableHandleValue result, JS::MutableHandleObject ports) {
    return static_cast<web_messaging::Realm*>(context)->Clone(value, transfer, destination, result, ports);
  }, &messaging);
  web_workers::Child child(cx, page, helpers, events, pipe, exceptionPrototype, workerKind);
  if (!child.Install()) return false;
  // MessagePorts transferred across the worker pipe; worker-made ids carry the
  // top bit, the parent's bit 30 (clone.h).
  web_workers::SetPortTransfer(&web_messaging::Realm::IsPort, &web_messaging::Realm::BridgePort, 0x80000000u);
  if (!service) {
    // SharedWorker ports, and ports a dedicated worker transferred or received:
    // web_messaging owns the ports; this Child encodes and routes their frames
    // over the worker pipe (docs/owners/WORKER.md).
    messaging.SetRemoteRelay([](void* context, uint32_t id, JS::HandleValue value, JS::HandleValue transfer) {
      return static_cast<web_workers::Child*>(context)->RelaySharedPort(id, value, transfer);
    }, &child);
    messaging.SetRemoteClose([](void* context, uint32_t id) {
      static_cast<web_workers::Child*>(context)->CloseSharedPort(id);
    }, &child);
    child.SetMessagePortFactory([](void* context, uint32_t id) {
      return static_cast<web_messaging::Realm*>(context)->NewRemotePort(id);
    }, &messaging);
    child.SetMessagePortDeliver([](void* context, uint32_t id, JS::HandleValue value) {
      return static_cast<web_messaging::Realm*>(context)->DeliverRemote(id, value);
    }, &messaging);
    child.SetMessagePortClose([](void* context, uint32_t id) {
      static_cast<web_messaging::Realm*>(context)->CloseRemote(id);
    }, &messaging);
  }
  struct ObjectUrlBinding {
    JSContext* cx;
    browser_environment::Environment& environment;
    web_platform::Realm& platform;
    ~ObjectUrlBinding() {
      platform.SetObjectUrlExchange(nullptr, nullptr);
      platform.SetObjectUrlPorts(nullptr, nullptr, nullptr);
    }
  } objectUrls{cx, environment, platform};
  platform.SetObjectUrlExchange([](void* context, std::string_view request, std::string* reply) {
    return static_cast<web_workers::Child*>(context)->Exchange(request, reply);
  }, &child);
  platform.SetObjectUrlPorts([](void* context, std::string* origin) {
    auto& source = *static_cast<ObjectUrlBinding*>(context);
    JS::RootedValue value(source.cx);
    if (!source.environment.Origin(&value) || !value.isString()) return false;
    JS::RootedString text(source.cx, value.toString());
    return ToUTF8(source.cx, text, origin);
  }, &objectUrls, nullptr);
  if (!platform.InstallWorker(!service)) return false;
  web_media::Realm media(cx, page, helpers, events, environment);
  web_geometry::Realm geometry(cx, page, helpers);
  if (!geometry.InstallWorkerValues()) return false;
  { JS::RootedObject prototype(cx, geometry.ReadOnlyRectPrototype());
    media.SetImageRectPrototype(prototype); }
  web_trusted_types::Realm trusted(cx, page, helpers);
  // Worker graphics (docs/owners/WORKER.md, 2026-10-21): OffscreenCanvas/2D and
  // WebGL from the same Canvas and WebGL owners, installed after the startup
  // profile so WebGL reads the creator's buffered "webgl" group.
  JS::RootedObject noCanvasElement(cx);
  web_canvas::Realm canvas(cx, page, helpers, noCanvasElement, events);
  web_graphics::Realm graphics(cx, page, helpers, environment);
  canvas.SetMatrixFactory([](void* context, const double* components, JS::MutableHandleObject result) {
    return static_cast<web_geometry::Realm*>(context)->MatrixFrom2D(components, result);
  }, &geometry);
  struct CanvasPorts { web_canvas::Realm& canvas;
    ~CanvasPorts() { canvas.SetContextPort(nullptr, nullptr); canvas.SetMatrixFactory(nullptr, nullptr); } }
      canvasPorts{canvas};
  WorkerOwners owners(cx, page, helpers, events, environment, resources, scheduling, child, checkpoint);
  // ServiceWorker/Registration/Container/Push interfaces and WorkerNavigator.serviceWorker
  // (Exposed=(Window,Worker)); a service process also gets its global scope.
  web_service_worker::Realm serviceWorkers(cx, page, helpers, events, environment);
  // ReportingObserver/ReportBody (Exposed=(Window,Worker)); no report source exists.
  web_observers::Realm observers(cx, page, helpers);
  if (!observers.InstallWorker()) return false;
  // Worker in a dedicated Worker (not exposed to service workers).
  web_workers::Realm nestedWorkers(cx, page, helpers, events);
  NestedWorkerPorts nestedPorts{cx, scheduling, platform, environment, child};
  struct NestedBinding { web_workers::Realm& workers; ~NestedBinding() { UnbindNestedWorkers(workers); } }
      nestedBinding{nestedWorkers};
  // BroadcastChannel (Exposed=(Window,Worker)): this process's registry, routed to
  // the parent and to this worker's own children (runtime/broadcast_ports.h).
  web_broadcast::Realm broadcast(cx, page, helpers, events, environment, nullptr);
  if (!broadcast.Install()) return false;
  broadcast.SetTaskPort([](void* context, JS::HandleObject task) {
    return static_cast<execution_scheduling::Realm*>(context)->PostTask(task);
  }, &scheduling);
  BroadcastRoute broadcastRoute(cx, page, broadcast, service ? nullptr : &nestedWorkers, &child);
  BindBroadcastRoute(broadcastRoute);
  struct BroadcastBinding {
    web_broadcast::Realm& broadcast; BroadcastRoute& route;
    ~BroadcastBinding() { UnbindBroadcastRoute(route); broadcast.SetTaskPort(nullptr, nullptr); }
  } broadcastBinding{broadcast, broadcastRoute};
  struct Startup {
    JSContext* cx;
    browser_environment::Environment& environment;
    PageHelpers& helpers;
    web_workers::Child& child;
    web_media::Realm& media;
    web_trusted_types::Realm& trusted;
    resource_network::Realm& resources;
    JS::HandleObject page;
    web_canvas::Realm& canvas;
    web_graphics::Realm& graphics;
    interaction::Realm& events;
    WorkerOwners& owners;
    web_service_worker::Realm& serviceWorkers;
    web_workers::Realm& nestedWorkers;
    NestedWorkerPorts& nestedPorts;
  } startup{cx, environment, helpers, child, media, trusted, resources, page, canvas, graphics, events, owners,
            serviceWorkers, nestedWorkers, nestedPorts};
  web_workers::ChildStartup prepare{&startup, [](void* context, std::string_view url,
                                               std::string_view profile) {
    auto& value = *static_cast<Startup*>(context);
    JS::RootedObject prototype(value.cx, value.child.globalScopePrototype());
    // The network surface reads the parent's HttpContext over the service
    // channel, which carries frames only after the startup frame was taken.
    return value.environment.InstallWorker(value.helpers, prototype, std::string(url), profile,
                                           value.child.dedicated()) &&
           value.media.InstallWorker(value.child.dedicated()) &&
           value.media.ConfigureExposure() &&
           web_crypto::Install(value.cx, value.page, value.helpers, prototype) &&
           value.trusted.Install(prototype) &&
           value.resources.InstallWorkerNetwork(value.helpers, prototype,
               [](void* owner, std::string_view request, std::string* reply) {
                 return static_cast<web_workers::Child*>(owner)->Exchange(request, reply);
               }, &value.child) &&
           // The creator's canvas fingerprint and font groups, as the Window binds them.
           (value.environment.SetOwnerPort("canvas", &web_canvas::ConfigureFingerprintGroup, value.cx),
            value.environment.SetOwnerPort("fonts", &web_geometry::ConfigureProfileGroup, value.cx),
            web_graphics::SetReadPixelsFilter(&web_canvas::PerturbExportedPixels), true) &&
           value.canvas.InstallWorker(prototype) &&
           value.graphics.Install() &&
           (value.canvas.SetContextPort(&web_graphics::ContextPort, value.graphics.state()), true) &&
           font_loading::InstallWorker(value.cx, value.page, value.helpers, value.events, prototype) &&
           value.owners.Install(prototype) &&
           value.serviceWorkers.Install() &&
           (!value.child.service() || value.serviceWorkers.InstallServiceScope(value.child)) &&
           (value.child.service() || InstallNestedWorkers(value.nestedWorkers, value.nestedPorts)) &&
           value.serviceWorkers.ConfigureExposure() &&
           value.owners.Finish(prototype) &&
           LinkInterfaceObjects(value.cx, value.page) &&
           OrderInterfacePrototypes(value.cx, value.page);
  }};
  ExceptionCapture errors(cx);
  errors.SetNativeExceptionReader(document_model::ExceptionFields);
  struct TaskLoop { execution_scheduling::Realm& scheduling; web_workers::Child& child; } loop{scheduling, child};
  web_workers::ChildTasks tasks{&loop,
    [](void* context) {
      auto& scheduling = static_cast<TaskLoop*>(context)->scheduling;
      scheduling.Freeze();
      scheduling.BeginRequest();
    },
    [](void* context) {
      auto& loop = *static_cast<TaskLoop*>(context);
      return loop.scheduling.Drain(1, [](void* value) {
        return !static_cast<web_workers::Child*>(value)->closed();
      }, &loop.child).executed != 0;
    }};
  const bool served=child.Serve(checkpoint, errors, tasks, prepare);
  // Retire native consumers before their original Child exchange endpoint dies.
  resources.RetireNativeFetches();
  resources.RetireWebTransports();
  return served;
}
}

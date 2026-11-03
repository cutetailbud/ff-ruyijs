#include "runtime/user_activation_ports.h"
#include "runtime/secure_exposure.h"
#include "runtime/runtime_internal.h"
#include "runtime/native_stack.h"
#include "resource_network/async_http.h"
#include "resource_network/necko_http.h"
#include "runtime/transport_partition.h"
#include "runtime/http_environment_ports.h"
#include "runtime/pipe.h"
#include "runtime/exception_capture.h"
#include "runtime/host_bridge.h"
#include "runtime/arguments.h"
#include "interaction/realm.h"
#include "browser_environment/environment.h"
#include "runtime/environment_control.h"
#include "runtime/page_helpers.h"
#include "runtime/scheduling_control.h"
#include "runtime/resource_control.h"
#include "runtime/console_control.h"
#include "runtime/page_load.h"
#include "runtime/api_monitor.h"
#include "runtime/browsing_context.h"
#include "runtime/top_level.h"
#include "runtime/top_level_assembly.h"
#include "runtime/window_message_ports.h"
#include "runtime/worker_tasks.h"
#include "runtime/navigator_ports.h"
#include "runtime/binding_order.h"
#include "runtime/object_url_ports.h"
#include "runtime/blob_stream_port.h"
#include "runtime/styling_tasks.h"
#include "runtime/styling_load_ports.h"
#include "runtime/image_load_ports.h"
#include "runtime/interface_objects.h"
#include "web_crypto/realm.h"
#include "web_geolocation/realm.h"
#include "document_model/realm.h"
#include "document_model/exception.h"
#include "domain/styling/css_font_value.h"
#include "web_geometry/system_font.h"
#include <cstring>
#include <memory>
#include "jsapi.h"
#include "js/CharacterEncoding.h"
#include "js/Exception.h"
#include "js/PropertyAndElement.h"
#include "js/String.h"
#include "shell/jsoptparse.h"

namespace ruyijs {

static bool Failure(JSContext* cx, JS::MutableHandleValue value) {
  return FormatOperationFailure(cx, value);
}

bool Reply(JSContext* cx, JS::HandleObject codec, JS::HandleValue id,
                  bool success, JS::MutableHandleValue value, Checkpoint checkpoint,
                  ExceptionCapture& errors, std::string* frame, bool* delivered) {
  if (!errors.Finish(value, &success) ||
      !EncodeReply(cx, codec, id, success, value, frame, &success)) return false;
  checkpoint(cx);
  bool serialized = true;
  if (!errors.Finish(value, &serialized)) return false;
  if (!serialized && !EncodeReply(cx, codec, id, false, value, frame, &success)) return false;
  if (frame->size() > Pipe::kMaxFrame) {
    JS_ReportErrorASCII(cx, "Result exceeds 16 MiB protocol limit");
    if (!Failure(cx, value) || !EncodeReply(cx, codec, id, false, value, frame, &success))
      return false;
  }
  if (delivered) *delivered = success;
  return true;
}


bool Run(JSContext* cx, js::cli::OptionParser* options, Checkpoint checkpoint,
         const SessionThreads& threads) {
  SessionOptions session;
  if (!options->getMultiStringOption('m').empty() ||
      !options->getMultiStringOption('p').empty() ||
      !options->getMultiStringOption('u').empty() || options->getBoolOption('i') ||
      !options->getMultiStringArg("scriptArgs").empty()) {
    JS_ReportErrorASCII(cx, "ruyijs currently accepts UTF-8 scripts with -e/-f or a script path");
    return false;
  }
  const char* mode = options->getStringOption("ruyijs-mode");
  if (mode && strcmp(mode, "debug") && strcmp(mode, "default")) {
    JS_ReportErrorASCII(cx, "ruyijs mode must be default or debug");
    return false;
  }
  const char* rpc = options->getStringOption("ruyijs-rpc");
  if (rpc && (!options->getMultiStringOption('e').empty() ||
              !options->getMultiStringOption('f').empty() || options->getStringArg("script"))) {
    JS_ReportErrorASCII(cx, "RPC mode accepts code through the request pipe only");
    return false;
  }
  if (rpc) session.rpc = rpc;
  session.debug = mode && !strcmp(mode, "debug");
  if (const char* profile = options->getStringOption("ruyijs-environment"))
    session.environment = profile;
  if (options->getStringOption("ruyijs-host")) session.host = &threads;
  return RunSession(cx, session, options, checkpoint);
}

RUYIJS_NEVER_INLINE
bool RunSession(JSContext* cx, const SessionOptions& session, js::cli::OptionParser* cli,
                Checkpoint checkpoint) {
  // A single session and a shared engine enter here from the same statement
  // of Run, so this is the native stack a single session has; the engine's
  // sessions get exactly as much (SessionOptions::stack). Never inlined, so
  // this is RunSession's own frame whether Run or an engine session calls it.
  const uintptr_t entry = RUYIJS_ENTRY_POSITION();
  if (session.host) return Host(cx, cli, checkpoint, *session.host, NativeStackBelow(cx, entry));
  if (session.stack) MatchNativeStack(cx, entry, session.stack);
#if defined(__linux__)
  // A plain Context (no engine stack) owns the process. Its helpers are a new
  // process group, so they are not reaped by the shell exiting; they notice
  // the closed pipe and spend 100-200 ms leaving. Drop them before return,
  // which is before the process exits. An engine session has a stack and
  // releases its own partition from the host, including a shared network.
  struct ReleasePlainHelpers {
    bool armed;
    ~ReleasePlainHelpers() {
      if (armed) resource_network::ReleaseNeckoPartition(CurrentPartition()->key);
    }
  } releasePlainHelpers{session.stack == 0};
#endif
  // Heap compaction stays off for the whole runtime. The donor engine's
  // compacting GC faults while relocating the shape/property-map graph the
  // browser installers build (see docs/DEVIATIONS.md D16 for the reproducer
  // and the bisection evidence). Compaction is an internal memory
  // optimization with no observable surface; JS::GCOptions::Shrink keeps the
  // cache purge and chunk decommit semantics that "collect" promises.
  JS_SetGCParameter(cx, JSGC_COMPACTING_ENABLED, 0);
  // Firefox's initial font-family is the locale group's default generic.
  ruyijs::domain::styling::CssSetInitialFontFamily(web_geometry::DefaultGenericFamily());
  ExceptionCapture errors(cx);
  JS::RootedObject page(cx), codec(cx);
  // The asynchronous HTTP owner outlives every realm whose image port names it;
  // each realm's State releases its in-flight element roots first.
  std::unique_ptr<resource_network::AsyncHttp> asyncHttp;
  // Child frames are destroyed before the main context, both after the drain.
  std::unique_ptr<BrowsingContext> main;
  // The resource owner is declared before Frames so it outlives every child
  // realm that holds a view over its one catalog, NetLog and Storage owner.
  std::unique_ptr<resource_network::Realm> resources;
  // Later top-level documents view the resource owner and the first document's
  // Context-scoped owners, and the frames inside them are Frames' children:
  // destroyed after Frames and before both.
  std::unique_ptr<TopLevel> top;
  std::unique_ptr<Frames> frames;
  std::unique_ptr<execution_scheduling::Realm> scheduling;
  // Declared before the original checkpoint: callback-entry and native task
  // ports remain live through it, then clear before Scheduling destruction.
  struct CallbackEntryBinding {
    std::unique_ptr<resource_network::Realm>& main;
    std::unique_ptr<Frames>& frames; std::unique_ptr<TopLevel>& top;
    ~CallbackEntryBinding() {
      if (main) {
        main->SetCallbackEntryPort(nullptr,nullptr);
        main->SetKeepaliveTaskPort(nullptr,nullptr);
      }
      if (frames) frames->UnbindResourceCallbacks();
      if (top) top->UnbindResourceCallbacks();
    }
  } callbackEntryBinding{resources,frames,top};
  struct DrainOnExit {
    JSContext* cx;
    Checkpoint checkpoint;
    ~DrainOnExit() {
      JS::AutoSaveExceptionState saved(cx);
      checkpoint(cx);
      saved.restore();
    }
  } drain{cx, checkpoint};
  page = CreatePage(cx);
  if (!page || !InitCodec(cx, &codec)) return false;
  main = std::make_unique<BrowsingContext>(cx, page);
  frames = std::make_unique<Frames>(cx);
  if (!main->Begin()) return false;
  EnvironmentControl control(cx, *main->environment);
  if (!session.environment.empty() && !control.File(session.environment.c_str())) return false;
  if (!main->Finish(*frames)) return false;
  top = std::make_unique<TopLevel>(cx, *main);
  frames->SetTopLevel(top.get());
  struct CryptoExposureBinding {
    EnvironmentControl& control;
    ~CryptoExposureBinding() { control.SetConfiguredPort(nullptr, nullptr); }
  } cryptoExposureBinding{control};
  control.SetConfiguredPort([](void* context) {
    return ConfigureSecureExposure(*static_cast<BrowsingContext*>(context));
  }, main.get());
  PageHelpers& helpers = *main->helpers;
  errors.SetNativeExceptionReader(document_model::ExceptionFields);
  scheduling = std::make_unique<execution_scheduling::Realm>(cx, page, checkpoint);
  if (!scheduling->Install(helpers, *main->interaction)) return false;
  BindUserActivation(*main, *scheduling);
  struct ActivationPorts {
    BrowsingContext& target;
    Frames& frames;
    TopLevel& top;
    ~ActivationPorts() {
      UnbindUserActivation(target);
      frames.UnbindActivation();
      top.UnbindActivation();
      frames.SetTopLevel(nullptr);
    }
  } activationPorts{*main, *frames, *top};
  BindWorkerTasks(*main->workers, *scheduling);
  struct WorkerTaskBinding {
    web_workers::Realm& workers;
    ~WorkerTaskBinding() { workers.Stop(); workers.SetTaskPort(nullptr, nullptr); }
  } workerTaskBinding{*main->workers};
  struct IndexedDBTaskPort {
    web_indexeddb::Realm& realm;
    ~IndexedDBTaskPort() { realm.SetTaskPort(nullptr, nullptr); }
  } indexedDBTaskPort{*main->indexeddb};
  main->indexeddb->SetTaskPort([](void* context, JS::HandleObject callback) {
    return static_cast<execution_scheduling::Realm*>(context)->PostTask(callback);
  }, scheduling.get());
  // Web Audio reads currentTime from this one clock owner; the port is
  // bound here, after Install, so the audio realm never opens a second one.
  main->audio->SetClockPort([](void* context) {
    return static_cast<execution_scheduling::Realm*>(context)->ClockSeconds();
  }, scheduling.get());
  main->audio->SetTaskPort([](void* context, JS::HandleObject callback) {
    return static_cast<execution_scheduling::Realm*>(context)->PostTask(callback);
  }, scheduling.get());
  // Console `time`/`timeEnd` measure with this same one clock. The guard clears
  // the port before the owner dies, and a child realm copies it when it shares
  // the Context log, so the console never opens a second time source.
  struct ConsoleClockPort {
    web_console::Realm& console;
    ~ConsoleClockPort() { console.SetClockPort(nullptr, nullptr); }
  } consoleClockPort{*main->console};
  main->console->SetClockPort([](void* context) {
    return static_cast<execution_scheduling::Realm*>(context)->ClockSeconds();
  }, scheduling.get());
  // The Observer owner delivers its observer callbacks on this Context's one
  // microtask queue and stamps IntersectionObserver entries from this Context's
  // one clock. The guard is declared after both owners, so its destructor
  // clears the ports before either one is destroyed.
  struct ObserverPorts {
    web_observers::Realm& observers;
    ~ObserverPorts() {
      observers.SetMicrotaskPort(nullptr, nullptr);
      observers.SetClockPort(nullptr, nullptr);
    }
  } observerPorts{*main->observers};
  main->observers->SetMicrotaskPort([](void* context, JS::HandleObject callback) {
    return static_cast<execution_scheduling::Realm*>(context)->QueueMicrotask(callback);
  }, scheduling.get());
  main->observers->SetClockPort([](void* context) {
    return static_cast<execution_scheduling::Realm*>(context)->ClockSeconds() * 1000.0;
  }, scheduling.get());
  SchedulingControl schedulingControl(cx, *scheduling, control);
  StylingTaskBinding stylingTasks(*main->styling, *scheduling);
  // The Window facade's postMessage task: Scheduling owns the queue turn and
  // Interaction owns the trusted MessageEvent. The guard is declared after both
  // owners, so its destructor clears the ports before either is destroyed.
  struct WindowMessagePorts {
    browser_environment::Environment& environment;
    ~WindowMessagePorts() {
      environment.SetPostTaskPort(nullptr, nullptr);
      environment.SetPostMessageTaskPort(nullptr, nullptr);
      environment.SetScrollEventPort(nullptr, nullptr);
      environment.SetMessagePort(nullptr, nullptr);
      environment.SetPopStatePort(nullptr, nullptr);
      environment.SetEventPort(nullptr, nullptr);
    }
  } windowMessagePorts{*main->environment};
  BindWindowMessages(*main, *scheduling);
  resources = std::make_unique<resource_network::Realm>(cx, page, *main->helpers);
  resources->SetTransportExceptionPorts({&document_model::CreateDerivedException,
                                        &document_model::DerivedExceptionData});
  if (!resources->Install(helpers, *main->interaction) || !main->environment->InstallFileSystem()) return false;
  resources->SetCallbackEntryPort([](void* context,JS::HandleObject global) {
    return !static_cast<execution_scheduling::Realm*>(context)->Retired(global);
  },scheduling.get());
  struct BeaconBinding {
    browser_environment::Environment& environment;
    ~BeaconBinding() { environment.SetSendBeaconPort(nullptr, nullptr); }
  } beaconBinding{*main->environment};
  BindNavigatorPorts(*main, *resources);
  BindHttpEnvironment(*main,*resources,*scheduling);
  BindObjectUrlNetwork(*main, *resources);
  BindBlobStreams(*main->platform, *resources);
  // Helper sessions start in the background when prewarm is set, so the page's
  // first requests do not wait for the helper interpreter (NECKO_TRANSPORT.md).
  // A plain Linux Context leaves prewarm at 0 and starts a helper on demand.
  resource_network::PrewarmNeckoSessions(session.prewarm);
  asyncHttp = std::make_unique<resource_network::AsyncHttp>(resources->Resources());
  if (!resources->BindAsyncHttp(*asyncHttp)) return false;
  BindImageLoads(*main, *asyncHttp);

  // Background transport completions run as tasks of the one scheduling owner.
  scheduling->SetExternalWork({
      [](void* http) { return static_cast<resource_network::AsyncHttp*>(http)->Pending(); },
      [](void* http, uint32_t ms) { return static_cast<resource_network::AsyncHttp*>(http)->Wait(ms); },
      [](void* http) { return static_cast<resource_network::AsyncHttp*>(http)->Run(); },
      asyncHttp.get()});
  StylingLoadBinding stylingLoads(*main,*resources);
  if (!scheduling->InstallTasks(page,helpers,*main->interaction)) return false;
  // The main realm's last installer has run: apply the WebIDL interface
  // object inheritance once (runtime/interface_objects.h).
  if (!LinkInterfaceObjects(cx, page)) return false;
  // ...and every installed prototype takes Firefox's binding member order.
  if (!OrderInterfacePrototypes(cx, page)) return false;
  resources->SetAbortDelayPort([](void* context,JS::HandleObject fn,double delay){
    return static_cast<execution_scheduling::Realm*>(context)->PostDelayedTask(fn,delay);
  },scheduling.get());
  struct XhrTaskBinding {
    resource_network::Realm& resources;
    BrowsingContext& main;
    ~XhrTaskBinding() { main.workerNetwork=nullptr;resources.Resources().SetHttpObservation(nullptr,nullptr,nullptr,nullptr);resources.SetXhrTaskPort(nullptr, nullptr, nullptr);resources.SetCookieTaskPort(nullptr,nullptr);resources.SetAbortDelayPort(nullptr,nullptr);resources.SetHttpEnvironment(nullptr,nullptr,nullptr,nullptr,nullptr); }
  } xhrTaskBinding{*resources, *main};
  resources->SetUserInputPort(&interaction::HandlingUserInput);
  resources->SetXhrTaskPort([](void* context, JS::HandleObject callback) {
    return static_cast<execution_scheduling::Realm*>(context)->PostTask(callback);
  }, scheduling.get(), checkpoint);
  // Beacon delivery survives the document that queued it.
  resources->SetKeepaliveTaskPort([](void* context, JS::HandleObject callback) {
    return static_cast<execution_scheduling::Realm*>(context)->PostKeepaliveTask(callback);
  }, scheduling.get());
  // window.cookieStore fires its CookieChangeEvent as one task of this Context;
  // a child realm inherits the binding with the rest of the resource owner.
  resources->SetCookieTaskPort([](void* context, JS::HandleObject callback) {
    return static_cast<execution_scheduling::Realm*>(context)->PostTask(callback);
  }, scheduling.get());
  ResourceControl resourceControl(cx, resources->Resources());
  ConsoleControl consoleControl(cx, main->console->log());
  PageLoad pageLoad(cx, checkpoint, *top, resources->Resources(), *scheduling, *frames);
  // The parser's speculative script/style preloads share this one transport.
  pageLoad.SetTransport(*asyncHttp);
  struct ScriptPortBinding {
    document_model::Realm& document;
    ~ScriptPortBinding(){document.SetScriptPort(nullptr,nullptr);document.SetFormPorts(nullptr,nullptr,nullptr);}
  } scriptBinding{*main->document};
  main->document->SetScriptPort(&PageLoad::ScriptPort,&pageLoad);
  main->document->SetFormPorts(&PageLoad::FormDataPort,&PageLoad::FormNavigatePort,&pageLoad);
  // The frame-load ports: the one coordinator above runs an iframe `src` load
  // or an inline `srcdoc` document against that frame's own owners. The guard
  // is declared after the coordinator so its destructor clears the ports first.
  struct FrameLoadPorts {
    Frames& frames;
    ~FrameLoadPorts() {
      frames.SetLoadPort(nullptr, nullptr);
      frames.SetLoadHtmlPort(nullptr, nullptr);
      frames.SetBlankPort(nullptr, nullptr);
      frames.SetSchedulingPort(nullptr, nullptr);
      frames.SetResourcePort(nullptr, nullptr);
      frames.SetRetirePort(nullptr, nullptr);
    }
  } frameLoadPorts{*frames};
  frames->SetLoadPort([](void* context, BrowsingContext& target, const std::string& url) {
    return static_cast<PageLoad*>(context)->QueueFrameLoad(target, url);
  }, &pageLoad);
  frames->SetLoadHtmlPort([](void* context, BrowsingContext& target, const std::string& html) {
    return static_cast<PageLoad*>(context)->LoadFrameHtml(target, html);
  }, &pageLoad);
  frames->SetBlankPort([](void* context, BrowsingContext& target) {
    return static_cast<PageLoad*>(context)->CompleteBlankFrame(target);
  }, &pageLoad);
  // One child-realm Scheduling surface installer, for an iframe and for a later
  // top-level document alike.
  Frames::SchedulingPort installScheduling = [](void* context, BrowsingContext& target) {
    BindUserActivation(target, *static_cast<execution_scheduling::Realm*>(context));
    BindWorkerTasks(*target.workers, *static_cast<execution_scheduling::Realm*>(context));
    BindWindowMessages(target, *static_cast<execution_scheduling::Realm*>(context));
    BindStylingTasks(*target.styling, *static_cast<execution_scheduling::Realm*>(context));
    return static_cast<execution_scheduling::Realm*>(context)->InstallInto(
        target.cx, target.page, *target.helpers, *target.interaction);
  };
  frames->SetSchedulingPort(installScheduling, scheduling.get());
  top->SetSchedulingPort(installScheduling, scheduling.get());
  // A retired frame's timers, frames and tasks stop with its document.
  frames->SetRetirePort([](void* context, BrowsingContext& target) {
    JS::RootedObject global(target.cx, target.page);
    static_cast<execution_scheduling::Realm*>(context)->Retire(global);
    if (target.navigatorNetwork) target.navigatorNetwork->RetireBodyConsumers();
    if (target.navigatorNetwork) target.navigatorNetwork->RetireNativeFetches();
    if (target.navigatorNetwork) target.navigatorNetwork->RetireWebTransports();
  }, scheduling.get());
  // The child realm's own resource surface, over this Context's one resource
  // owner: the same catalog, NetLog and Storage area, realm-local objects.
  struct FrameResourceBinding {
    resource_network::Realm& resources; execution_scheduling::Realm& scheduling;
    resource_network::AsyncHttp& http;
  } frameResourceBinding{*resources,*scheduling,*asyncHttp};
  Frames::ResourcePort installResources = [](void* context, BrowsingContext& target) {
    auto& binding = *static_cast<FrameResourceBinding*>(context);
    auto& source = binding.resources;
    JS::RootedObject global(target.cx, target.page.get());
    target.resources =
        std::make_unique<resource_network::Realm>(target.cx, global, *target.helpers, source);
    if (!target.resources->BindAsyncHttp(binding.http)) return false;
    BindNavigatorPorts(target, *target.resources);
    BindHttpEnvironment(target,*target.resources,binding.scheduling);
    BindObjectUrlNetwork(target, *target.resources);
    BindBlobStreams(*target.platform, *target.resources);
    BindImageLoads(target, binding.http);
    BindStylingLoads(target,*target.resources);
    // The resource surface is a child or later top-level realm's last
    // installer, so the interface object inheritance is applied after it.
    return target.resources->InstallInto(*target.helpers, *target.interaction)&&
      target.environment->InstallFileSystem()&&
      binding.scheduling.InstallTasks(global,*target.helpers,*target.interaction)&&
      LinkInterfaceObjects(target.cx, global) && OrderInterfacePrototypes(target.cx, global);
  };
  frames->SetResourcePort(installResources, &frameResourceBinding);
  top->SetResourcePort(installResources, &frameResourceBinding);
  // The Location navigation ports: the one page-load coordinator above
  // performs a JS-driven navigation, and the Interaction owner delivers the
  // hashchange its one same-document case fires. The guard is declared after
  // both owners, so it clears the ports before either is destroyed.
  struct NavigationPorts {
    browser_environment::Environment& environment;
    ~NavigationPorts() {
      environment.SetNavigatePort(nullptr, nullptr);
      environment.SetHashChangePort(nullptr, nullptr);
    }
  } navigationPorts{*main->environment};
  // Location navigation, popstate, hashchange and reportError of the first
  // document; a later one binds the same through TopLevel's document port.
  BindDocumentEvents(*main, pageLoad);
  // Debug session: declared after the owners it observes so it outlives none of
  // them, and bound to the Document Model port only while this scope is alive.
  ApiMonitor monitor(cx, session.debug);
  struct MonitorBinding {
    document_model::Realm& document;
    interaction::Realm& interaction;
    browser_environment::Environment& environment;
    execution_scheduling::Realm& scheduling;
    resource_network::Realm& resources;
    ~MonitorBinding() {
      document.SetApiMonitor(nullptr, nullptr);
      interaction.SetApiMonitor(nullptr, nullptr);
      environment.SetApiMonitor(nullptr, nullptr);
      scheduling.SetApiMonitor(nullptr, nullptr);
      resources.SetApiMonitor(nullptr, nullptr);
    }
  } binding{*main->document, *main->interaction, *main->environment, *scheduling,
            *resources};
  if (monitor.enabled()) {
    main->document->SetApiMonitor(&ApiMonitor::Port, &monitor);
    main->interaction->SetApiMonitor(&ApiMonitor::Port, &monitor);
    main->environment->SetApiMonitor(&ApiMonitor::Port, &monitor);
    scheduling->SetApiMonitor(&ApiMonitor::Port, &monitor);
    resources->SetApiMonitor(&ApiMonitor::Port, &monitor);
  }
  // Later top-level documents (docs/owners/TOP_LEVEL.md). Declared after every
  // owner their ports reach, so Close retires the active one and clears the
  // ports before any of those owners is destroyed.
  struct TopLevelBinding {
    TopLevelAssembly owners;
    TopLevel& top;
    ~TopLevelBinding() { top.Close(); }
  } topLevelBinding{{*scheduling, pageLoad, monitor, *frames}, *top};
  top->SetDocumentPort(&BindTopLevelDocument, &topLevelBinding.owners);
  top->SetRetirePort(&RetireTopLevelDocument, &topLevelBinding.owners);
  if (!session.rpc.empty())
    return Serve(cx, *top, codec, session.rpc.c_str(), checkpoint, errors, control,
                 schedulingControl, resourceControl, consoleControl, pageLoad, monitor);
  if (!cli) return true;
  control.Freeze();
  schedulingControl.Freeze();
  auto expressions = cli->getMultiStringOption('e');
  auto files = cli->getMultiStringOption('f');
  while (!expressions.empty() || !files.empty()) {
    if (!expressions.empty() && (files.empty() || expressions.argno() < files.argno())) {
      const char* code = expressions.front();
      if (!Command(cx, top->Page(), codec, code, strlen(code),
                   "-e", checkpoint, errors)) return false;
      expressions.popFront();
    } else {
      const char* path = files.front();
      if (!File(cx, top->Page(), codec, path, checkpoint, errors)) return false;
      files.popFront();
    }
  }
  if (const char* path = cli->getStringArg("script"))
    return File(cx, top->Page(), codec, path, checkpoint, errors);
  return true;
}
}


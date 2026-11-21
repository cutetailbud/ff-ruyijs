#include "runtime/secure_exposure.h"
#include "runtime/user_activation_ports.h"
#include "runtime/browsing_context.h"
#include "execution_scheduling/realm.h"
#include "web_styling/computed_source.h"
#include "document_model/exception.h"
#include "runtime/adopted_ports.h"
#include "runtime/style_notification_ports.h"
#include "runtime/geometry_ports.h"
#include "runtime/object_url_ports.h"
#include "runtime/navigator_ports.h"
#include "runtime/binding_order.h"
#include "web_media/media_keys.h"
#include "web_media/realm.h"
#include "web_workers/state.h"
#include "js/Wrapper.h"
#include "runtime/runtime.h"
#include "runtime/module_session.h"
#include "resource_network/page_http.h"
#include "resource_network/abort.h"
#include "jsapi.h"

namespace ruyijs {
BrowsingContext::BrowsingContext(JSContext* context, JS::HandleObject global)
    : cx(context), page(context, global), loadCompletion(context) {}
namespace {
// Node identity comparison; the document_model state helper is internal.
bool SameNode(const document_model::NodeKey& a, const document_model::NodeKey& b) {
  return a.document == b.document && a.node == b.node;
}
// The runtime frame port: this browsing context's connected iframe count, read
// from the Document Model owner. browser_environment never sees the DOM.
size_t FrameCount(void* context) {
  auto* document = static_cast<document_model::Realm*>(context);
  return document ? document->IframeCount() : 0;
}
// The style-source ports: the Styling owner reads one element cascade
// input and the document ordered <style> sources from the Document
// Model owner, never a second tree index.
bool StyleChain(void* context, JS::HandleObject element,
                std::vector<document_model::StyleFrame>* chain, bool* is_element) {
  return static_cast<document_model::Realm*>(context)->ReadStyleChain(element, chain, is_element);
}
bool StyleSheets(void* context, std::vector<document_model::StyleSheetSource>* out) {
  return static_cast<document_model::Realm*>(context)->ReadStyleSheets(out);
}
bool StyleRelation(void* context,JS::HandleObject element,uint64_t document,uint64_t key,
                   document_model::StyleRelationFrame* out) {
  return static_cast<document_model::Realm*>(context)->ReadStyleRelation(element,document,key,out);
}
bool CanvasElementCheck(void* context, JS::HandleObject element, bool* result) {
  std::vector<document_model::StyleFrame> chain;
  return static_cast<document_model::Realm*>(context)->ReadStyleChain(element, &chain, result);
}
// The `src` of a frame element, read through the Document Model owner that
// holds the tree: the runtime keeps no attribute copy of its own. A missing
// attribute yields an empty reference, which leaves the child at about:blank.
bool FrameSource(BrowsingContext& parent, JS::HandleObject element, std::string* reference) {
  reference->clear();
  JS::RootedValue source(parent.cx);
  if (!parent.document->Attribute(element, u"src", &source)) return false;
  if (!source.isString()) return true;
  JS::RootedString text(parent.cx, source.toString());
  return ToUTF8(parent.cx, text, reference);
}
// The `srcdoc` of a frame element. Unlike `src` it is not a URL: a present
// attribute wins over `src` even when its value is the empty string (the same
// precedence Firefox applies), and the markup reaches the parser as-is.
bool FrameSrcdoc(BrowsingContext& parent, JS::HandleObject element, std::string* html,
                 bool* present) {
  *present = false;
  html->clear();
  JS::RootedValue source(parent.cx);
  if (!parent.document->Attribute(element, u"srcdoc", &source)) return false;
  if (!source.isString()) return true;
  *present = true;
  JS::RootedString text(parent.cx, source.toString());
  return ToUTF8(parent.cx, text, html);
}
}
BrowsingContext::~BrowsingContext() {
  if (security) security->Unbind();
  if(resources)resources->Resources().SetHttpObservation(nullptr,nullptr,nullptr,nullptr);
  if (interaction) interaction->SetInputActivationPort(nullptr, nullptr);
  if (interaction) interaction->SetCloseWatcherPorts({}, nullptr);
  if (environment) environment->SetActivationClock(nullptr, nullptr);
  scheduling=nullptr;
  if(resources)resources->SetHttpEnvironment(nullptr,nullptr,nullptr,nullptr,nullptr);
  if (geometry) {
    geometry->SetExceptionPort(nullptr, nullptr);
    geometry->ClearProjections();
  }
  if (environment) {
    environment->SetSendBeaconPort(nullptr, nullptr);
    navigatorNetwork = nullptr;
    workerNetwork = nullptr;
    environment->SetPostTaskPort(nullptr, nullptr);
    environment->SetPostMessageTaskPort(nullptr, nullptr);
    environment->SetScrollEventPort(nullptr, nullptr);
    environment->SetMessagePort(nullptr, nullptr);
    environment->SetClonePort(nullptr, nullptr);
    environment->SetFrameCount(nullptr, nullptr);
    environment->SetFrameWindowPort(nullptr, nullptr);
    SetPageFrames(page, nullptr);
    environment->SetWindowNamePort(nullptr, nullptr);
    environment->SetViewportPort(nullptr, nullptr);
    environment->SetExceptionPort(nullptr, nullptr);
    environment->SetGraphicsPort(nullptr, nullptr);
    environment->SetGraphicsDescribe(nullptr, nullptr);
    environment->SetGpuPort(nullptr, nullptr);
    environment->SetGpuDescribe(nullptr, nullptr);
    environment->SetPermissionsPort(nullptr, nullptr);
    environment->SetPermissionsDescribe(nullptr, nullptr);
    environment->SetGeolocationPort(nullptr, nullptr);
    environment->SetSpeechPort(nullptr, nullptr);
    for (const char* group : {"canvas", "audio", "mediaFeatures", "rtc", "fonts"})
      environment->SetOwnerPort(group, nullptr, nullptr);
  }
  if (interaction) interaction->SetMicrotaskCheckpointPort(nullptr, nullptr);
  if (speech) speech->SetTaskPort(nullptr, nullptr);
  if (document) document->SetFileListPort(nullptr, nullptr);
  if (interaction) interaction->SetFileListPort(nullptr, nullptr);
  if (broadcastRoute) UnbindBroadcastRoute(*broadcastRoute);
  if (serviceWorker) {
    serviceWorker->SetTaskPort(nullptr, nullptr);
    serviceWorker->SetClient(nullptr, false, nullptr, nullptr);
  }
  if (rtc) {
    rtc->SetTaskPort(nullptr, nullptr);
    rtc->SetRemoteTrackPort(nullptr, nullptr);
  }
  if (media) media->SetCanvasSizePort(nullptr, nullptr);
  if (permissions) permissions->SetTaskPort(nullptr, nullptr);
  if (geolocation) {
    geolocation->SetTaskPort(nullptr, nullptr);
    geolocation->SetPermissionPort(nullptr, nullptr);
  }
  if (audio) audio->SetChannelPort(nullptr, nullptr);
  if (messaging) messaging->Retire();
  if (broadcast) {
    broadcast->SetTaskPort(nullptr, nullptr);
  }
  if (locks) locks->SetTaskPort(nullptr, nullptr);
  if (wakeLock) wakeLock->SetTaskPort(nullptr, nullptr);
  if (animation) {
    animation->SetSchedulingPorts({});
    animation->SetDocumentPorts({});
  }
  if (canvas) {
    if (document) document->SetCanvasDimensionPort(nullptr, nullptr);
    canvas->SetMatrixFactory(nullptr, nullptr);
    canvas->SetContextPort(nullptr, nullptr);
    canvas->SetCaptureStreamPort(nullptr, nullptr);
    canvas->SetElementCheck(nullptr, nullptr);
  }
  if (workers) {
    workers->Stop();
    workers->SetTaskPort(nullptr, nullptr);
    workers->SetLogicalClockPort(nullptr, nullptr);
    workers->SetObjectUrlPort(nullptr, nullptr);
    workers->SetEnvironmentPort(nullptr, nullptr);
    workers->SetScriptUrlPort(nullptr, nullptr);
    workers->SetServiceFactory(nullptr, nullptr);
  }
  if (styling) {
    styling->SetLoadPort(nullptr, nullptr);
    styling->SetLoadEventPort(nullptr, nullptr);
    styling->SetTaskPort(nullptr, nullptr);
    styling->SetDocumentPorts(nullptr, nullptr, nullptr);
    styling->SetDocumentNodePort(nullptr, nullptr);
    styling->SetDocumentSourcePorts(nullptr, nullptr, nullptr);
    styling->SetDocumentListPorts(nullptr, nullptr, nullptr);
    styling->SetAdoptedPorts(nullptr, nullptr, nullptr, nullptr);
    styling->SetExceptionPort(nullptr, nullptr);
  }
  if (document) {
    document->SetFormPorts(nullptr, nullptr, nullptr);
    document->SetImageLoadPort(nullptr, nullptr);
    document->SetOffsetGeometryPort(nullptr, nullptr);
    document->SetPointQueryPort(nullptr, nullptr);
    document->SetElementScrollPort({});
    document->SetMediaKeysCheckPort(nullptr, nullptr);
    document->SetTextTrackPorts({});
    document->SetStyleDeclarationPort({});
    document->SetGeometryFactory(nullptr);
    document->SetStyleNotifyPort(nullptr, nullptr);
    document->SetStyleElementPort(nullptr, nullptr);
    document->SetStyleDocumentListPort(nullptr, nullptr);
    document->SetAdoptedStylesListPort(nullptr, nullptr);
    document->SetFrameLifecycle(nullptr, nullptr, nullptr, nullptr);
  }
  if (!interaction) return;
  interaction->SetPathResolver(nullptr, nullptr, nullptr);
  interaction->SetExceptionReporter(nullptr, nullptr);
  interaction->SetReclaimPort(nullptr, nullptr);
  interaction->SetBrandPort(nullptr, nullptr);
  interaction->SetEventClockPort(nullptr, nullptr, nullptr);
}
bool BrowsingContext::Begin(BrowsingContext* parent, JS::HandleObject frameElement) {
  helpers = std::make_unique<PageHelpers>(cx, page);
  interaction = std::make_unique<interaction::Realm>(cx, page);
  environment = parent ? std::make_unique<browser_environment::Environment>(
                             cx, page, *parent->environment, frameElement)
                       : std::make_unique<browser_environment::Environment>(cx, page);
  return true;
}
bool BrowsingContext::BeginSuccessor(BrowsingContext& previous) {
  predecessor = &previous;
  helpers = std::make_unique<PageHelpers>(cx, page);
  interaction = std::make_unique<interaction::Realm>(cx, page);
  environment = std::make_unique<browser_environment::Environment>(
      cx, page, *previous.environment, browser_environment::Environment::Succession{});
  return true;
}
bool BrowsingContext::Finish(Frames& owner) {
  // An iframe shares Context-scoped state with its parent, a later top-level
  // document with the one it replaced; the first document creates it.
  BrowsingContext* origin = parent ? parent : predecessor;
  // The existing Environment prepares and resolves the native modern Touch
  // decision before derived installation, including startup without a profile.
  // Navigator's reported point count remains independent of interface exposure.
  if (!interaction->Install(*helpers)) return false;
  // The AbortSignal owner lends its context-free capability to EventTarget
  // (addEventListener signal) and CloseWatcher; see interaction/abort_ports.h.
  interaction->SetAbortPorts({resource_network::IsAbortSignal, resource_network::AbortSignalled,
                              resource_network::AddAbortAlgorithm});
  bool touchEvents = false;
  if (!environment->TouchEventsEnabled(&touchEvents) ||
      !interaction->InstallDerived(touchEvents)) return false;
  // The Console owner installs before the document exists, so a script running
  // during parsing can log. The main page creates the Context's one log; an
  // iframe child shares it (one ordered host stream) and copies the clock port
  // the coordinator bound. See docs/owners/CONSOLE.md.
  console = std::make_unique<web_console::Realm>(
      cx, page, *helpers, origin ? &origin->console->log() : nullptr);
  if (!console->Install()) return false;
  if (origin) console->CopyClockFrom(*origin->console);
  if (!environment->Install(*interaction, *helpers)) return false;
  document = std::make_unique<document_model::Realm>(cx, page, *interaction, *environment, *helpers);
  if (!document->Install() || !interaction->Attach(page)) return false;
  interaction->SetPathResolver([](void* context, JS::HandleObject target, JS::HandleString type,
                                  bool composed, JS::MutableHandleObjectVector path,
                                  std::vector<uint8_t>* flags) {
    return static_cast<document_model::Realm*>(context)->EventPath(target, type, composed, path, flags);
  }, [](void* context, JS::HandleObject original, JS::HandleObject against,
        JS::MutableHandleObject result, bool* inShadowTree) {
    return static_cast<document_model::Realm*>(context)->RetargetEventTarget(
        original, against, result, inShadowTree);
  }, document.get());
  interaction->SetExceptionReporter([](void* context, const char* name, const char* message) {
    return static_cast<document_model::Realm*>(context)->ThrowException(name, message);
  }, document.get());
  interaction->SetReclaimPort([](void* context) {
    static_cast<document_model::Realm*>(context)->Reclaim();
  }, document.get());
  // Event.timeStamp on the document's performance timeline: relative to its
  // time origin, clamped like performance.now() (Event::TimeStamp).
  interaction->SetEventClockPort([](void* context, double clock_ms) {
    auto& owner = *static_cast<BrowsingContext*>(context);
    JS::RootedObject page(owner.cx, owner.page);
    return owner.scheduling->OriginRelative(page, clock_ms);
  }, [](void* context, double ms) {
    auto& owner = *static_cast<BrowsingContext*>(context);
    JS::RootedObject page(owner.cx, owner.page);
    return owner.scheduling->ReduceRelative(page, ms);
  }, this);
  // Event init dictionary brands (docs/owners/INTERACTION.md): Element and
  // DOMException from Document Model, MediaStreamTrack from Web Media.
  interaction->SetBrandPort([](void* context, int brand, JS::HandleValue value, bool* matches) {
    auto& owner = *static_cast<BrowsingContext*>(context);
    *matches = false;
    if (!value.isObject()) return true;
    if (brand == 0) return owner.document->IsElementValue(value, matches);
    if (brand == 1) { *matches = web_media::IsMediaStreamTrack(&value.toObject()); return true; }
    JS::RootedValue data(owner.cx);
    JSObject* object = js::CheckedUnwrapStatic(&value.toObject());
    *matches = object && document_model::DerivedExceptionData(object, &data);
    return true;
  }, this);
  frames = &owner;
  environment->SetFrameCount(&FrameCount, document.get());
  environment->SetFrameWindowPort([](void* context, size_t index, JS::MutableHandleValue window) {
    return static_cast<document_model::Realm*>(context)->FrameWindowAt(index, window);
  }, document.get());
  pageFrames.count = &FrameCount;
  pageFrames.at = [](void* context, size_t index, JS::MutableHandleValue window) {
    return static_cast<document_model::Realm*>(context)->FrameWindowAt(index, window);
  };
  pageFrames.context = document.get();
  SetPageFrames(page, &pageFrames);
  environment->SetWindowNamePort([](void* context, JS::HandleString name,
                                    JS::MutableHandleValue value) {
    return static_cast<document_model::Realm*>(context)->ResolveWindowName(name, value);
  }, document.get());
  environment->SetExceptionPort([](void* context, const char* name, const char* message) {
    return static_cast<document_model::Realm*>(context)->ThrowException(name, message);
  }, document.get());
  document->SetFrameLifecycle(&Frames::Create, &Frames::Discard, &Frames::Navigate, this);
  // The Content Security owner after the Document Model and Interaction it
  // reports through. A frame's initial document inherits its creator's
  // policies (srcdoc/about:blank); PageLoad resets them for a `src` load.
  security = std::make_unique<web_security::Realm>(cx, page, *interaction, *environment, *document);
  if (!security->Install()) return false;
  if (parent && parent->security) security->InheritFrom(*parent->security);
  audio = std::make_unique<web_audio::Realm>(cx, page, *helpers, *interaction);
  if (!audio->Install()) return false;
  environment->SetOwnerPort("audio", &web_audio::ConfigureProfileGroup, cx);
  platform = std::make_unique<web_platform::Realm>(cx, page, *helpers, *interaction,
                                                 origin ? origin->platform.get() : nullptr);
  platform->SetFormFieldsPort([](void* context, JS::HandleObject form,
                                 JS::HandleValue submitter,
                                 std::vector<web_platform::FormField>* result) {
    auto& source = *static_cast<document_model::Realm*>(context);
    std::vector<document_model::FormField> fields;
    if (!source.FormFields(form, submitter, &fields)) return false;
    result->clear();
    result->reserve(fields.size());
    for (const auto& field : fields)
      result->push_back({field.name, field.value, field.emptyFile});
    return true;
  }, document.get());
  BindObjectUrlCreation(*this);
  if (!platform->Install()) return false;
  // FileList instances come from the Web Platform owner (input.files, DataTransfer.files).
  auto fileList = [](void* context, JS::HandleObject files, JS::MutableHandleObject result) {
    return static_cast<web_platform::Realm*>(context)->NewFileList(files, result);
  };
  document->SetFileListPort(fileList, platform.get());
  interaction->SetFileListPort(fileList, platform.get());
  workers = std::make_unique<web_workers::Realm>(cx, page, *helpers, *interaction,
                                              origin ? origin->workers.get() : nullptr);
  if (!workers || !workers->Install(true)) return false;
  BindObjectUrlWorkers(*this);
  workers->SetObjectUrlPort([](void* context, const std::string& url,
                               std::string* bytes, std::string* type) {
    return static_cast<web_platform::Realm*>(context)->ReadObjectUrl(url, bytes, type);
  }, platform.get());
  workers->SetEnvironmentPort([](void* context, std::string* result) {
    return static_cast<browser_environment::Environment*>(context)->ExportWorkerProfile(result);
  }, environment.get());
  BindWorkerScriptUrls(*this);
  if (!web_crypto::Install(cx, page, *helpers)) return false;
  trusted = std::make_unique<web_trusted_types::Realm>(cx, page, *helpers);
  if (!trusted->Install()) return false;
  geometry = std::make_unique<web_geometry::Realm>(cx, page, *helpers);
  if (!geometry->Install()) return false;
  JS::RootedObject rangePrototype(cx, document->rangePrototype());
  if (!rangePrototype || !geometry->InstallRangeProjection(rangePrototype,
      [](void* context, JS::HandleValue value, JS::MutableHandleObjectVector elements) {
        return static_cast<document_model::Realm*>(context)->RangeContents(value, elements);
      }, document.get())) return false;
  JS::RootedObject elementPrototype(cx, document->elementPrototype());
  if (!elementPrototype || !geometry->InstallElementProjection(elementPrototype,
      [](void* context, JS::HandleValue value) {
        return static_cast<document_model::Realm*>(context)->CheckElementReceiver(value);
      }, document.get())) return false;
  geometry->SetExceptionPort([](void* context, const char* name, const char* message) {
    return static_cast<document_model::Realm*>(context)->ThrowException(name, message);
  }, document.get());
  JS::RootedObject content(cx, document->document());
  if (!content) return false;
  if (!font_loading::Install(cx, page, *helpers, *interaction, content, &document_model::IsDocumentValue)) return false;
  JS::RootedObject canvasPrototype(cx, document->canvasPrototype());
  if (!canvasPrototype) return false;
  canvas = std::make_unique<web_canvas::Realm>(cx, page, *helpers, canvasPrototype, *interaction);
  canvas->SetElementCheck(&CanvasElementCheck, document.get());
  canvas->SetMatrixFactory([](void* context, const double* components, JS::MutableHandleObject result) {
    return static_cast<web_geometry::Realm*>(context)->MatrixFrom2D(components, result);
  }, geometry.get());
  if (!canvas->Install()) return false;
  // A width/height content-attribute change, by any DOM path, resets the bitmap.
  document->SetCanvasDimensionPort([](void* context, JS::HandleObject element,
                                      JS::HandleValue width, JS::HandleValue height) {
    return static_cast<web_canvas::Realm*>(context)->DimensionsChanged(element, width, height);
  }, canvas.get());
  // The "canvas" profile group (donor --fpfile canvas.* keys) goes to the Canvas owner.
  environment->SetOwnerPort("canvas", &web_canvas::ConfigureFingerprintGroup, cx);
  environment->SetOwnerPort("fonts", &web_geometry::ConfigureProfileGroup, cx);
  graphics = std::make_unique<web_graphics::Realm>(cx, page, *helpers, *environment);
  if (!graphics->Install()) return false;
  web_graphics::SetReadPixelsFilter(&web_canvas::PerturbExportedPixels);
  // The getContext port is bound last, after both owners exist. The WebGL owner
  // installs the environment's "webgl" describe/configure ports itself, so the
  // profile group configured before Finish is flushed at that point.
  canvas->SetContextPort(&web_graphics::ContextPort, graphics->state());
  JS::RootedObject shadowPrototype(cx, document->shadowPrototype());
  if (!shadowPrototype) return false;
  styling = std::make_unique<web_styling::Realm>(cx, page, *helpers, *environment, content, shadowPrototype);
  styling->SetRangeBrandPort(&document_model::IsAbstractRangeValue);
  if (!styling->Install()) return false;
  // ctx.lang "inherit" (CanvasRenderingContext2D::ResolveFontLang): the canvas
  // element's inherited lang from the cascade, or the document root's.
  canvas->SetLangPort([](void* context, JS::HandleObject element, std::string* lang) {
    auto& owner = *static_cast<BrowsingContext*>(context);
    lang->clear();
    JS::RootedObject target(owner.cx, element);
    if (!target) {
      JSObject* document = owner.document->document();
      if (!document) return true;
      JS::RootedValue value(owner.cx, JS::ObjectValue(*document));
      if (!owner.document->SvgLengthRoot(value, &target)) return false;
      if (!target) return true;
    }
    web_styling::ComputedStyle style;
    if (!owner.styling->ReadComputed(target, &style)) return false;
    for (const auto& entry : style.values)
      if (entry.first == "-x-lang") { *lang = entry.second; break; }
    return true;
  }, this);
  environment->SetOwnerPort("mediaFeatures", &web_styling::ConfigureProfileGroup, cx);
  styling->SetDocumentPorts(&StyleChain, &StyleSheets, document.get(), &StyleRelation);
  styling->SetDocumentNodePort([](void* context, uint64_t document, uint64_t key,
                                 JS::MutableHandleValue result) {
    return static_cast<document_model::Realm*>(context)->ReadStyleNode(document, key, result);
  }, document.get());
  styling->SetDocumentSourcePorts([](void* context, uint64_t document, uint64_t key,
                                      std::vector<document_model::StyleSheetSource>* result) {
    return static_cast<document_model::Realm*>(context)->ReadStyleScope(document, key, result);
  }, [](void* context, JS::HandleObject node, JS::HandleValue expected, JS::HandleValue value) {
    return static_cast<document_model::Realm*>(context)->AssociateStyleSheet(node, expected, value);
  }, document.get());
  document->SetStyleElementPort([](void* context, uint64_t document, uint64_t key,
                                   document_model::StyleElementAction action, bool disabled,
                                   JS::MutableHandleValue result) {
    return static_cast<web_styling::Realm*>(context)->StyleElement(document, key, action, disabled, result);
  }, styling.get());
  styling->SetDocumentListPorts([](void* context, JS::HandleValue receiver, JS::MutableHandleValue result, bool shadow) {
    return static_cast<document_model::Realm*>(context)->ResolveStyleSheets(receiver, result, shadow);
  }, [](void* context, JS::HandleObject document, document_model::StyleSheetScope* scope,
          std::vector<document_model::StyleSheetSource>* result) {
    return static_cast<document_model::Realm*>(context)->ReadDocumentStyleSources(document, scope, result);
  }, document.get());
  document->SetStyleDocumentListPort([](void* context, JS::HandleObject document, JS::MutableHandleValue result) {
    return static_cast<web_styling::Realm*>(context)->NewDocumentList(document, result);
  }, styling.get());
  styling->SetExceptionPort([](void* context, const char* name, const char* message) {
    return static_cast<document_model::Realm*>(context)->ThrowException(name, message);
  }, document.get());
  document->SetStyleDeclarationPort(styling->DeclarationPort());
  BindAdoptedStyles(*this);
  BindStyleNotifications(*this);
  if (!BindGeometryPorts(*this)) return false;
  // WebRTC needs only the Interaction event owner and PageHelpers; it installs
  // after Document so its peer/channel events travel the shared dispatch tree.
  rtc = std::make_unique<web_rtc::Realm>(cx, page, *helpers, *interaction);
  // RTCRtpScriptTransform's WebIDL Worker check is answered by the Workers owner.
  rtc->SetWorkerCheck(&web_workers::Realm::IsWorker);
  // RTCError instances are the DOMException owner's native storage.
  web_rtc::Realm::ExceptionPorts rtcExceptions;
  rtcExceptions.create = &document_model::CreateDerivedException;
  rtcExceptions.data = &document_model::DerivedExceptionData;
  rtc->SetExceptionPorts(rtcExceptions);
  if (!rtc->Install()) return false;
  environment->SetOwnerPort("rtc", &web_rtc::ConfigureProfileGroup, cx);
  media = std::make_unique<web_media::Realm>(cx, page, *helpers, *interaction, *environment);
  { JS::RootedObject prototype(cx, geometry->ReadOnlyRectPrototype());
    media->SetImageRectPrototype(prototype); }
  if (!media->Install()) return false;
  media->SetAutoplayBrandPorts(document_model::IsMediaElementValue, web_audio::IsAudioContextValue);
  if (canvas) canvas->SetCaptureStreamPort([](void* context, JS::HandleObject element, JS::MutableHandleObject stream) {
    return static_cast<web_media::Realm*>(context)->CaptureCanvas(element, stream);
  }, media.get());
  if (canvas) media->SetCanvasSizePort([](void* context, JS::HandleObject element, uint32_t* width,
                                          uint32_t* height) {
    return static_cast<web_canvas::Realm*>(context)->ElementSize(element, width, height);
  }, canvas.get());
  rtc->SetRemoteTrackPort([](void* context, bool video, JS::MutableHandleObject track) {
    return static_cast<web_media::Realm*>(context)->NewRemoteTrack(video, track);
  }, media.get());
  // The "media" profile group: the MediaDevices list and the decoding answers
  // that owner answers from (docs/owners/MEDIA.md).
  environment->SetMediaPort([](void* context, JS::HandleObject group) {
    return static_cast<web_media::Realm*>(context)->Configure(group);
  }, media.get());
  environment->SetMediaDescribe([](void* context, JS::MutableHandleValue result) {
    return static_cast<web_media::Realm*>(context)->Describe(result);
  }, media.get());
  document->SetMediaKeysCheckPort([](void*, JS::HandleValue value) {
    return web_media::IsMediaKeysValue(value);
  }, nullptr);
  document->SetTextTrackPorts({web_media::NewElementTextTrackList,
                               web_media::AppendElementTextTrack});
  indexeddb = std::make_unique<web_indexeddb::Realm>(cx, page, *helpers, *interaction,
      *environment, origin ? origin->indexeddb.get() : nullptr);
  if (!indexeddb->Install()) return false;
  // WebGPU installs last: it appends its single "gpu" accessor to the
  // Navigator prototype the Browser Environment owner already built, and its
  // own GPU object graph lives entirely in this realm.
  gpu = std::make_unique<web_gpu::Realm>(cx, page, *helpers, *interaction, *environment);
  if (!gpu->Install()) return false;
  // Permissions also appends one Navigator accessor; an iframe reads its
  // parent's permission-state table (docs/owners/PERMISSIONS.md).
  permissions = std::make_unique<web_permissions::Realm>(cx, page, *helpers, *interaction,
      origin ? origin->permissions.get() : nullptr);
  JS::RootedObject navigatorPrototype(cx, environment->navigatorPrototype());
  if (!permissions->Install(navigatorPrototype)) return false;
  environment->SetPermissionsPort([](void* context, JS::HandleObject group) {
    return static_cast<web_permissions::Realm*>(context)->Configure(group);
  }, permissions.get());
  environment->SetPermissionsDescribe([](void* context, JS::MutableHandleValue result) {
    return static_cast<web_permissions::Realm*>(context)->Describe(result);
  }, permissions.get());
  // Geolocation appends one Navigator accessor after Permissions and reads the
  // permission from that owner's table; the position Config is shared with the
  // parent or predecessor realm (docs/owners/GEOLOCATION.md).
  geolocation = std::make_unique<web_geolocation::Realm>(cx, page, *helpers,
      origin ? origin->geolocation.get() : nullptr);
  geolocation->SetPermissionPort([](void* context) {
    return static_cast<web_permissions::Realm*>(context)->StateOf("geolocation");
  }, permissions.get());
  if (!geolocation->Install(navigatorPrototype)) return false;
  environment->SetGeolocationPort([](void* context, JS::HandleObject group) {
    return static_cast<web_geolocation::Realm*>(context)->Configure(group);
  }, geolocation.get());
  // Speech Synthesis: Window accessor and EventTarget-derived interfaces; its
  // voice list is shared with the parent or predecessor realm and its events
  // go through this realm's Interaction owner (docs/owners/SPEECH_SYNTHESIS.md).
  speech = std::make_unique<web_speech::Realm>(cx, page, *helpers, *interaction,
      origin ? origin->speech.get() : nullptr);
  if (!speech->Install()) return false;
  environment->SetSpeechPort([](void* context, JS::HandleObject group) {
    return static_cast<web_speech::Realm*>(context)->Configure(group);
  }, speech.get());
  // BroadcastChannel: the main context owns the Context's channel registry and
  // each frame shares it; its delivery task port is bound with the window
  // message ports (docs/owners/BROADCAST_CHANNEL.md).
  broadcast = std::make_unique<web_broadcast::Realm>(cx, page, *helpers, *interaction,
      *environment, origin ? origin->broadcast.get() : nullptr);
  if (!broadcast->Install()) return false;
  messaging = std::make_unique<web_messaging::Realm>(cx, page, *helpers, *interaction);
  if (!messaging->Install()) return false;
  // AudioWorkletNode.port: Web Audio asks the realm's MessagePort owner.
  audio->SetChannelPort([](void* context, JS::MutableHandleObject first, JS::MutableHandleObject second) {
    return static_cast<web_messaging::Realm*>(context)->NewChannel(first, second);
  }, messaging.get());
  environment->SetClonePort([](void* context, JS::HandleValue value, JS::HandleValue transfer,
      JS::HandleObject destination, JS::MutableHandleValue result, JS::MutableHandleObject ports) {
    return static_cast<web_messaging::Realm*>(context)->Clone(value, transfer, destination, result, ports);
  }, messaging.get());
  // SharedWorker ports: web_messaging owns the MessagePorts; the Web Workers
  // owner encodes and routes their frames to the shared worker processes
  // (docs/owners/WORKER.md, "Shared workers").
  workers->SetMessagePortFactory([](void* context, uint32_t id) {
    return static_cast<web_messaging::Realm*>(context)->NewRemotePort(id);
  }, messaging.get());
  workers->SetMessagePortDeliver([](void* context, uint32_t id, JS::HandleValue value) {
    return static_cast<web_messaging::Realm*>(context)->DeliverRemote(id, value);
  }, messaging.get());
  workers->SetMessagePortClose([](void* context, uint32_t id) {
    static_cast<web_messaging::Realm*>(context)->CloseRemote(id);
  }, messaging.get());
  messaging->SetRemoteRelay(&web_workers::RelaySharedWindow, workers->state());
  messaging->SetRemoteClose(&web_workers::CloseSharedWindow, workers->state());
  // ClipboardItem settles with this realm's Blob (Web Platform) and
  // DOMException (Document Model), so it installs after both.
  clipboard = std::make_unique<web_clipboard::Realm>(cx, page, *helpers);
  clipboard->SetBlobBrandPort(web_platform::IsBlobValue);
  if (!clipboard->Install()) return false;
  {
    JS::RootedObject eventTarget(cx, interaction->prototype()), navigatorPrototype(cx, environment->navigatorPrototype());
    if (!clipboard->InstallNavigatorClipboard(eventTarget, navigatorPrototype,
        [](void* context, JS::HandleObject target) {
          return static_cast<interaction::Realm*>(context)->Attach(target);
        }, interaction.get())) return false;
  }
  credentials = std::make_unique<web_credentials::Realm>(cx, page, *helpers);
  if (!credentials->Install()) return false;
  {
    JS::RootedObject navigatorPrototype(cx, environment->navigatorPrototype());
    if (!credentials->InstallNavigator(navigatorPrototype)) return false;
  }
  // Web Locks shares its parent's per-origin queues (docs/owners/WEB_LOCKS.md).
  locks = std::make_unique<web_locks::Realm>(cx, page, *helpers, *environment,
                                             origin ? origin->locks.get() : nullptr);
  if (!locks->Install()) return false;
  wakeLock = std::make_unique<web_wake_lock::Realm>(cx, page, *helpers, *interaction, *environment);
  if (!wakeLock->Install()) return false;
  midi = std::make_unique<web_midi::Realm>(cx, page, *helpers, *interaction, *environment);
  if (!midi->Install()) return false;
  // Gamepad device interfaces (docs/owners/GAMEPAD.md); no device, no instance.
  gamepad = std::make_unique<web_gamepad::Realm>(cx, page, *helpers);
  if (!gamepad->Install()) return false;
  // Web Animations (docs/owners/WEB_ANIMATION.md): the Document Model hands over
  // its Document/Element prototypes and node queries; the scheduling ports are
  // bound by the shared window coordinator (window_message_ports.cpp).
  animation = std::make_unique<web_animation::Realm>(cx, page, *helpers, *interaction);
  {
    web_animation::Realm::DocumentPorts ports;
    ports.kind = [](void* c, JS::HandleValue v) {
      return static_cast<document_model::Realm*>(c)->AnimationNodeKind(v);
    };
    ports.document = [](void* c, JS::HandleObject node, JS::MutableHandleObject out) {
      return static_cast<document_model::Realm*>(c)->AnimationNodeDocument(node, out);
    };
    ports.inScope = [](void* c, JS::HandleObject root, JS::HandleObject node, bool subtree, bool* result) {
      return static_cast<document_model::Realm*>(c)->AnimationInScope(root, node, subtree, result);
    };
    ports.mainDocument = [](void* c) { return static_cast<document_model::Realm*>(c)->document(); };
    ports.context = document.get();
    animation->SetDocumentPorts(ports);
  }
  JS::RootedObject animationDocument(cx, document->animationDocumentPrototype());
  JS::RootedObject animationElement(cx, document->elementPrototype());
  if (!animation->Install(animationDocument, animationElement)) return false;
  serviceWorker = std::make_unique<web_service_worker::Realm>(cx, page, *helpers, *interaction, *environment,
      origin ? origin->serviceWorker.get() : nullptr);
  if (!serviceWorker->Install()) return false;
  broadcastRoute = std::make_unique<BroadcastRoute>(cx, page, *broadcast, workers.get(), nullptr);
  BindBroadcastRoute(*broadcastRoute);
  // Service workers of this client run in its Web Workers owner (ARCHITECTURE.md).
  serviceWorker->SetClient(workers.get(), parent != nullptr, [](void* context) {
    return static_cast<document_model::Realm*>(context)->DocumentParsed();
  }, document.get());
  // Every owner that appends a Navigator member has installed by now: apply
  // [SecureContext] exposure, which also fixes the Navigator key order.
  if (!ConfigureSecureExposure(*this)) return false;
  // The Observer owner installs last, after the Document Model whose
  // real-change log it reads. Its delivery microtask port is bound by the
  // runtime coordinator once the Scheduling owner exists; a child frame
  // copies its parent's port in Frames::Create, because one agent has one
  // microtask queue.
  observers = std::make_unique<web_observers::Realm>(cx, page, *helpers, *document);
  return observers->Install();
}
void Frames::SetLoadPort(LoadPort port, void* context) {
  load_ = port;
  loadContext_ = context;
}
void Frames::SetLoadHtmlPort(LoadHtmlPort port, void* context) {
  loadHtml_ = port;
  loadHtmlContext_ = context;
}
void Frames::SetBlankPort(BlankPort port, void* context) {
  blank_ = port;
  blankContext_ = context;
}
void Frames::SetSchedulingPort(SchedulingPort port, void* context) {
  scheduling_ = port;
  schedulingContext_ = context;
}
void Frames::SetResourcePort(ResourcePort port, void* context) {
  resource_ = port;
  resourceContext_ = context;
}
void Frames::SetRetirePort(RetirePort port, void* context) {
  retire_ = port;
  retireContext_ = context;
}
void Frames::RetireUnder(BrowsingContext& top) {
  for (const auto& child : children_)
    if (child->parent == &top) { Retire(&top); return; }
}
bool Frames::LoadChild(BrowsingContext& parent, BrowsingContext& child,
                       const std::string& reference, bool* loaded) {
  *loaded = false;
  if (reference.empty() || !load_) return true;
  // The reference is resolved against the parent document's URL through the
  // one resource resolver, and only an absolute HTTP(S) target is fetched by
  // the one coordinator (same scheme rule as the top-level Location).
  JS::RootedValue href(cx_);
  if (!parent.environment->URL(&href) || !href.isString()) return true;
  JS::RootedString text(cx_, href.toString());
  std::string base, url;
  if (!ToUTF8(cx_, text, &base) || !resource_network::ResolveURL(base, reference, &url))
    return true;
  if (url.rfind("http://", 0) != 0 && url.rfind("https://", 0) != 0) return true;
  *loaded = true;
  if (!load_(loadContext_, child, url)) {
    // A child document that cannot load does not break the document that
    // inserted the frame; the pending failure is dropped here (Firefox
    // reports it as an `error` event on the element, recorded in D8).
    JS_ClearPendingException(cx_);
  }
  return true;
}
bool Frames::LoadChildHtml(BrowsingContext& child, const std::string& html) {
  if (!loadHtml_) return true;
  if (!loadHtml_(loadHtmlContext_, child, html)) {
    // An inline document is produced by the same coordinator; a failure in it
    // must not break the document that inserted the frame.
    JS_ClearPendingException(cx_);
  }
  return true;
}
bool Frames::CompleteBlank(BrowsingContext& child) {
  if (blank_ && !blank_(blankContext_, child)) JS_ClearPendingException(cx_);
  return true;
}
bool Frames::Live(const BrowsingContext& context) const {
  for (const auto& child : children_)
    if (child.get() == &context) return true;
  return false;
}
bool Frames::LoadEventsPending(const BrowsingContext& parent,
                               bool (*pending)(void*, JS::HandleObject), void* context) const {
  for (const auto& child : children_)
    if (child->parent == &parent && child->loadCompletion &&
        pending(context, child->loadCompletion)) return true;
  return false;
}
void Frames::UnbindResourceCallbacks() {
  for (const auto& child:children_)
    if (child->resources) {
      child->resources->SetCallbackEntryPort(nullptr,nullptr);
      child->resources->SetKeepaliveTaskPort(nullptr,nullptr);
    }
  for (const auto& child:retired_)
    if (child->resources) {
      child->resources->SetCallbackEntryPort(nullptr,nullptr);
      child->resources->SetKeepaliveTaskPort(nullptr,nullptr);
    }
}
Frames::~Frames() {
  // Youngest first: a nested frame drops before the frame whose environment
  // and document it was created against.
  while (!children_.empty()) children_.pop_back();
  while (!retired_.empty()) retired_.pop_back();
}
BrowsingContext* Frames::FindDocument(const document_model::Realm& document) const {
  for (const auto& context : children_)
    if (context->document.get() == &document) return context.get();
  for (const auto& context : retired_)
    if (context->document.get() == &document) return context.get();
  return nullptr;
}
bool Frames::DescendsFrom(const BrowsingContext* child, const BrowsingContext* ancestor) {
  for (const BrowsingContext* cursor = child; cursor; cursor = cursor->parent)
    if (cursor == ancestor) return true;
  return false;
}
bool Frames::Create(void* context, JS::HandleObject element, document_model::NodeKey key,
                    JS::MutableHandleObject window, JS::MutableHandleObject document) {
  auto& parent = *static_cast<BrowsingContext*>(context);
  Frames& self = *parent.frames;
  JSContext* cx = self.cx_;
  {  // the parent's window[i] properties follow its new child list
    JS::RootedObject parentPage(cx, parent.page);
    if (!InvalidatePageFrames(cx, parentPage)) return false;
  }
  // A repeated notice for the same connected element returns the context it
  // already has; only a re-insertion after retirement builds a new one.
  for (const auto& existing : self.children_) {
    if (existing->parent != &parent) continue;
    if (!SameNode(existing->frame_element, key)) continue;
    window.set(existing->page);
    document.set(existing->document->document());
    return !!document.get();
  }
  if (self.children_.size() + self.retired_.size() >= kLimit) {
    JS_ReportErrorASCII(cx, "iframe browsing context limit exceeded");
    return false;
  }
  JS::RootedObject global(cx, CreatePage(cx, parent.page));
  if (!global) return false;
  auto child = std::make_unique<BrowsingContext>(cx, global);
  child->parent = &parent;
  child->frame_element = key;
  if (!child->Begin(&parent, element) || !child->Finish(self)) return false;
  child->observers->CopyPortsFrom(*parent.observers);
  // The child realm runs its own timers/rAF/microtasks on this Context's one
  // clock and queues, and its own fetch/XHR/Headers/Storage over this Context's
  // one resource catalog and NetLog; both install before any child script can
  // run, because the first load follows within the same structural change.
  if (self.scheduling_ && !self.scheduling_(self.schedulingContext_, *child)) return false;
  if (self.resource_ && !self.resource_(self.resourceContext_, *child)) return false;
  child->document->CopyScriptPortFrom(*parent.document);
  child->document->CopyFormPortsFrom(*parent.document);
  // Child Location writes share the existing asynchronous frame-load path.
  // Keep the live-context identity as the port target: a held Window belonging
  // to a removed frame must not load into a later frame or the top-level page.
  child->environment->SetNavigatePort([](void* context, const char* url, bool, bool) {
    auto& target = *static_cast<BrowsingContext*>(context);
    Frames& frames = *target.frames;
    if (!frames.Live(target)) return true;
    if (!frames.load_) {
      JS_ReportErrorASCII(target.cx, "frame navigation requires the runtime coordinator");
      return false;
    }
    return frames.load_(frames.loadContext_, target, url ? url : "");
  }, child.get());
  child->environment->SetHashChangePort([](void* context, JS::HandleObject window,
                                         const char* oldURL, const char* newURL) {
    return static_cast<interaction::Realm*>(context)->FireHashChange(window, oldURL, newURL);
  }, child->interaction.get());
  JS::RootedObject content(cx, child->document->document());
  if (!content) return false;
  // Assembly only: the structure notice links the element to this context and
  // then runs the first load through Navigate, so `srcdoc`/`src` already on the
  // element at insertion and later changes share one reader.
  self.children_.push_back(std::move(child));
  window.set(global);
  document.set(content);
  return true;
}
bool Frames::Navigate(void* context, JS::HandleObject element, document_model::NodeKey key,
                      bool initial) {
  auto& parent = *static_cast<BrowsingContext*>(context);
  Frames& self = *parent.frames;
  for (const auto& child : self.children_) {
    if (child->parent != &parent || !SameNode(child->frame_element, key)) continue;
    // Each branch returns before a load can grow children_.
    BrowsingContext& target = *child;
    std::string srcdoc;
    bool inline_document = false;
    if (!FrameSrcdoc(parent, element, &srcdoc, &inline_document)) return false;
    if (inline_document) return self.LoadChildHtml(target, srcdoc);
    std::string reference;
    bool loaded = false;
    if (!FrameSource(parent, element, &reference) ||
        !self.LoadChild(parent, target, reference, &loaded)) return false;
    // A first load without a target keeps the about:blank document, which
    // completes during insertion (Firefox fires the element `load` there).
    return loaded || !initial || self.CompleteBlank(target);
  }
  return true;
}
void Frames::Discard(void* context, document_model::NodeKey key) {
  auto& parent = *static_cast<BrowsingContext*>(context);
  Frames& self = *parent.frames;
  BrowsingContext* target = nullptr;
  for (const auto& child : self.children_)
    if (child->parent == &parent && SameNode(child->frame_element, key))
      target = child.get();
  {
    JS::RootedObject parentPage(self.cx_, parent.page);
    if (!InvalidatePageFrames(self.cx_, parentPage)) JS_ClearPendingException(self.cx_);
  }
  if (!target) return;
  // The whole frame subtree goes with it: a nested document stays connected in
  // its own tree, so its context is retired here instead of by its own realm.
  self.Retire(target);
}
void Frames::Retire(const BrowsingContext* ancestor) {
  std::vector<std::unique_ptr<BrowsingContext>> kept;
  kept.reserve(children_.size());
  for (auto& child : children_) {
    if (DescendsFrom(child.get(), ancestor)) {
      child->interaction->RetireCloseWatchers();
      child->workers->Stop();
      child->document->SetScriptPort(nullptr,nullptr);
      child->document->SetFormPorts(nullptr,nullptr,nullptr);
      child->document->RetireImageLoads();
      child->platform->ClearDocumentUrls();
      child->environment->RetireMessages();
      child->messaging->Retire();
      child->environment->SetViewportPort(nullptr, nullptr);
      if (retire_) retire_(retireContext_, *child);
      retired_.push_back(std::move(child));
    }
    else kept.push_back(std::move(child));
  }
  children_ = std::move(kept);
}
}




#include "runtime/secure_exposure.h"
#include "runtime/page_load.h"
#include "runtime/browsing_context.h"
#include "runtime/top_level.h"
#include "runtime/exception_capture.h"
#include "runtime/module_session.h"
#include "runtime/resource_schema.h"
#include "browser_environment/environment.h"
#include "document_model/realm.h"
#include "interaction/realm.h"
#include "execution_scheduling/realm.h"
#include "execution_scheduling/navigation_timing.h"
#include "resource_network/page_http.h"
#include "domain/browser_environment/url_value.h"
#include "domain/execution_scheduling/resource_timing.h"
#include <algorithm>
#include <cctype>
#include <chrono>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif
#include "domain/resource_network/cookie_scope.h"
#include "web_console/realm.h"
#include "browser_environment/cross_origin.h"
#include "js/Exception.h"
#include "js/SourceText.h"
#include "js/CompilationAndEvaluation.h"
#include "web_speech/realm.h"
#include "jsapi.h"
#include "js/CharacterEncoding.h"
#include "js/GCVector.h"
#include "js/PropertyAndElement.h"
#include "js/Promise.h"
#include "js/Realm.h"
#include "js/String.h"

namespace ruyijs {
namespace {
// A document's own scripts may navigate again; following that without a bound
// would recurse in the same synchronous load, so the product stops instead.
constexpr uint32_t kMaxNavigationDepth = 8;
struct DepthGuard {
  explicit DepthGuard(uint32_t& counter) : counter(++counter) {}
  ~DepthGuard() { --counter; }
  uint32_t& counter;
};
}
bool PageLoad::Request(JS::HandleObject request, JS::MutableHandleValue result) {
  std::string action, url;
  JS::RootedValue html(cx_);
  if (!resource_wire::Fields(cx_, request, {"id", "op", "action", "url", "html", "replay_async_scripts"}) ||
      !StringProperty(cx_, request, "action", &action)) return false;
  if (action == "navigate") {
    bool hasOrder = false;
    if (!JS_HasOwnProperty(cx_, request, "replay_async_scripts", &hasOrder)) return false;
    if (hasOrder) return resource_wire::Error(cx_, "recorded script order requires page load");
    // A browser-started navigation of the top-level document (address bar):
    // it commits on this call, the same way the host `load` action does.
    if (!StringProperty(cx_, request, "url", &url)) return false;
    if (!BrowserNavigation(url)) return false;
    JS::RootedObject summary(cx_, JS_NewPlainObject(cx_));
    JS::RootedString text(cx_, JS_NewStringCopyUTF8N(cx_, JS::UTF8Chars(url.data(), url.size())));
    if (!summary || !text) return false;
    JS::RootedValue value(cx_, JS::StringValue(text));
    if (!JS_DefineProperty(cx_, summary, "url", value, JSPROP_ENUMERATE)) return false;
    result.setObject(*summary);
    return true;
  }
  if (action != "load") {
    JS_ReportErrorASCII(cx_, "Unknown page action");
    return false;
  }
  if (!StringProperty(cx_, request, "url", &url) || !JS_GetProperty(cx_, request, "html", &html))
    return false;
  if (!html.isString()) {
    JS_ReportErrorASCII(cx_, "Page html must be a string");
    return false;
  }
  JS::RootedString markup(cx_, html.toString());
  ReplayScriptOrder order;
  if (!ReadReplayScriptOrder(cx_, request, &order)) return false;
  // A host load is a real top-level navigation: retain the current document's
  // session-history entry and append the requested URL before replacing the
  // active document in place. This preserves the initial about:blank entry,
  // which Firefox exposes through history.length.
  ++ticket_;
  BrowsingContext& active = top_.Active();
  JS::RootedObject page(cx_, active.page);
  Owners owners = OwnersOf(active, page);
  if (active.security) active.security->Reset();  // The replaced document's policies go with it.
  JSAutoRealm realm(cx_, page);
  if (!owners.environment.CommitNavigation(url, false) ||
      !ConfigureSecureExposure(active)) return false;
  return Load(owners, url, markup, result, &order);
}
bool PageLoad::ClaimFullParse(Owners& owners, size_t length) {
  constexpr size_t kFullParseBudget = 10485760;  // dom.script_loader.delazification.max_size
  BrowsingContext* context = Find(owners.document);
  size_t& used = context ? context->parseBudget : rootParseBudget_;
  if (used + length >= kFullParseBudget) return false;  // mTotalFullParseSize + size < max_size
  used += length;
  return true;
}
BrowsingContext* PageLoad::Find(const document_model::Realm& document) const {
  if (auto* frame = frames_.FindDocument(document)) return frame;
  return top_.IsActive(document) ? &top_.Active() : nullptr;
}
// HTML "script-blocking style sheet": a parser script runs only once the
// document's style sheets in flight have loaded and applied (Firefox), so it
// reads their styles. Other network completions keep arriving meanwhile.
bool PageLoad::WaitForStyleSheets(BrowsingContext* context) {
  auto* styling = context ? context->styling.get() : nullptr;
  if (!styling) return true;
  while (styling->LoadsInFlight() && scheduling_.RunExternalTask()) {}
  return styling->CompleteArrived();
}
resource_network::Model& PageLoad::Network(BrowsingContext& target) const {
  return target.resources ? target.resources->Resources() : resources_;
}
PageLoad::Owners PageLoad::OwnersOf(BrowsingContext& target, JS::HandleObject page) {
  return Owners{cx_, page, *target.environment, *target.document, *target.interaction,
                Network(target), *target.platform};
}
// One lifecycle milestone on the produced document's navigation record. A
// document that has no record yet (the host `load` entry, a srcdoc or blank
// frame) gets one whose origin is now, so each document's offsets are its own.
bool PageLoad::Milestone(JS::HandleObject page, MilestoneKind kind) {
  if (!scheduling_.HasNavigation(page)) {
    execution_scheduling::NavigationTiming timing;
    const auto snapshot = scheduling_.Inspect();
    timing.present = true;
    timing.time_origin = snapshot.epoch + snapshot.now;
    if (!scheduling_.RecordNavigation(page, timing)) return false;
  }
  return scheduling_.MarkNavigation(page, int(kind));
}
bool PageLoad::ReportModuleFailure(Owners& owners, JS::HandleObject,
                                   const std::string& filename) {
  JSContext* cx = owners.cx;
  JS::RootedValue error(cx, JS::UndefinedValue());
  if (JS_IsExceptionPending(cx)) {
    if (!JS_GetPendingException(cx, &error)) {
      JS_ClearPendingException(cx);
    } else {
      JS_ClearPendingException(cx);
    }
  }
  ExceptionReport report = DescribeException(cx, error, nullptr);
  JS_ClearPendingException(cx);
  const std::string message = report.name.empty()
      ? report.message : report.name + ": " + report.message;
  if (!owners.events.FireError(owners.page, error, message.c_str(),
                               filename.c_str(), 0, 0)) {
    JS_ClearPendingException(cx);
    return false;
  }
  JS_ClearPendingException(cx);
  return true;
}
bool PageLoad::Load(Owners& owners, const std::string& url, JS::HandleString html,
                    JS::MutableHandleValue result, const ReplayScriptOrder* order) {
  // The host request has already appended the URL as a navigation entry. The
  // load path replaces that active entry while it builds the document, so the
  // URL and session history remain owned by Browser Environment.
  JSAutoRealm realm(owners.cx, owners.page);
  if (!scheduling_.HasNavigation(owners.page)) {
    execution_scheduling::NavigationTiming timing;
    const auto clock = scheduling_.Inspect();
    timing.present = true;
    timing.time_origin = clock.epoch + clock.now;
    timing.name = url.substr(0, url.find('#'));
    if (!scheduling_.RecordNavigation(owners.page, timing)) return false;
  }
  uint32_t executed = 0;
  JS::RootedObject completion(owners.cx);
  return owners.environment.Navigate(url) &&
         Produce(owners, html, result, &executed, false, &completion, order) &&
         (!completion || WaitForCompletion(completion));
}
// An iframe `src` load, run by the runtime frame owner at the change point
// that committed the attribute. The child's Environment commits the URL into
// its own Location source (no profile model, no history entry) and the same
// parse/execute/lifecycle path below produces the document in the child realm.
bool PageLoad::LoadFrame(BrowsingContext& target, const std::string& url,
                         const std::string& method, const std::string& requestBody,
                         const std::string& contentType) {
  if (depth_ >= kMaxNavigationDepth) {
    JS_ReportErrorASCII(cx_, "navigation nesting limit exceeded");
    return false;
  }
  DepthGuard guard(depth_);
  Owners owners{cx_, target.page, *target.environment, *target.document, *target.interaction,
                target.resources->Resources(), *target.platform};
  JSAutoRealm realm(owners.cx, owners.page);
  resource_network::HttpContext source;
  if(!owners.resources.ReadHttpContext(&source))return false;
  resource_network::HttpResult http;
  std::string body;
  uint32_t executed = 0;
  resource_network::HeaderPairs headers;
  if (!contentType.empty()) headers.emplace_back("Content-Type", contentType);
  resource_network::HttpOptions options;options.initiator="iframe";
  options.source=source.source;options.site=source.site;
  // The frame document's own navigation start and response instants, on the
  // Context's clock, as the top-level navigation records them
  // (docs/owners/NAVIGATION_TIMING.md).
  const auto started = scheduling_.Inspect();
  const double origin = started.epoch + started.now;
  // The Necko boundary ticks map onto this clock from here. Windows uses QPC
  // milliseconds; other platforms use the steady clock the Necko helper records.
#ifdef _WIN32
  LARGE_INTEGER counter{}, frequency{};
  QueryPerformanceCounter(&counter);
  QueryPerformanceFrequency(&frequency);
  const double qpc = double(counter.QuadPart) * 1000 / double(frequency.QuadPart);
#else
  const double qpc = std::chrono::duration<double, std::milli>(
      std::chrono::steady_clock::now().time_since_epoch()).count();
#endif
  resource_network::PerformHttp(owners.resources, method, url, headers, requestBody,
      &http, resource_network::RedirectMode::Follow, options);
  // Commit the response URL, not the pre-redirect request URL. This is also
  // the source for Location, origin checks and relative child resources.
  if (http.status == 204 || http.status == 205) return true;
  std::string committed = http.redirected && !http.finalURL.empty() ? http.finalURL : url;
  const auto fragment = url.find('#');
  if (fragment != std::string::npos && committed.find('#') == std::string::npos)
    committed += url.substr(fragment);
  if (!owners.environment.SetFrameLocation(committed) ||
      !ConfigureSecureExposure(target) || !IsolateCrossSiteFrame(target, committed) ||
      !owners.environment.CommitAgentCluster(AgentClusterHeader(http))) return false;
  // A fetched document does not inherit its creator's policies (D133: the
  // response's own CSP header is not read).
  if (target.security) target.security->Reset();
  if (http.status != 0) body = http.body;
  const auto ended = scheduling_.Inspect();
  if (!RecordFrameNavigation(target, committed, http, origin, ended.epoch + ended.now, qpc)) return false;
  owners.document.SetDocumentReferrer(http.referrer);
  JS::RootedString markup(owners.cx, JS_NewStringCopyUTF8N(owners.cx,
      JS::UTF8Chars(body.data(), body.size())));
  if (!markup) return false;
  JS::RootedValue ignored(owners.cx);
  JS::RootedObject completion(owners.cx);
  return Produce(owners, markup, &ignored, &executed, true, &completion);
}
// An iframe `srcdoc` load: the inline markup reaches the same parser without a
// resource request, and the child commits `about:srcdoc` so its Location and
// document URL report the inline document while its origin stays the
// creator's.
bool PageLoad::LoadFrameHtml(BrowsingContext& target, const std::string& html) {
  if (depth_ >= kMaxNavigationDepth) {
    JS_ReportErrorASCII(cx_, "navigation nesting limit exceeded");
    return false;
  }
  DepthGuard guard(depth_);
  Owners owners{cx_, target.page, *target.environment, *target.document, *target.interaction,
                target.resources->Resources(), *target.platform};
  JSAutoRealm realm(owners.cx, owners.page);
  if (!owners.environment.SetFrameLocation("about:srcdoc")) return false;
  // The child document's secure-context surface follows its committed URL.
  if (!ConfigureSecureExposure(target)) return false;
  if (!owners.environment.CommitAgentCluster("")) return false;
  // An about:srcdoc document inherits its creator's policies.
  if (target.security) {
    target.security->Reset();
    if (target.parent && target.parent->security) target.security->InheritFrom(*target.parent->security);
  }
  JS::RootedString markup(owners.cx, JS_NewStringCopyUTF8N(owners.cx,
      JS::UTF8Chars(html.data(), html.size())));
  if (!markup) return false;
  JS::RootedValue ignored(owners.cx);
  uint32_t executed = 0;
  JS::RootedObject completion(owners.cx);
  return Produce(owners, markup, &ignored, &executed, true, &completion);
}
// The navigation record of a frame document, published before any of its
// scripts run so performance.timeOrigin, performance.timing.* and its
// navigation entry read one record for the document's whole life. The
// lifecycle milestones the load path stamps later are offsets from it.
bool PageLoad::RecordFrameNavigation(BrowsingContext& target, const std::string& url,
                                     const resource_network::HttpResult& http,
                                     double origin, double responseEnd, double qpc) {
  execution_scheduling::NavigationTiming timing;
  timing.present = true;
  timing.time_origin = origin;
  // Necko's measured phases of the final hop, as offsets from the frame's own
  // origin (its fetchStart); an unmeasured phase falls back to the one before.
  const double end = responseEnd - origin;
  const auto at = [&](double tick, double fallback) {
    return tick > 0 ? std::clamp(tick - qpc, 0.0, end) : fallback;
  };
  timing.fetch_start = 0.0;
  timing.dns_start = at(http.dnsStartTick, 0);
  timing.dns_end = at(http.dnsEndTick, timing.dns_start);
  timing.connect_start = at(http.connectStartTick, timing.dns_end);
  timing.connect_end = at(http.connectEndTick, timing.connect_start);
  timing.secure_start = url.rfind("https:", 0) == 0 ? at(http.secureTick, timing.connect_end) : 0;
  timing.establishment = true;
  timing.request_start = at(http.requestTick, timing.connect_end);
  timing.response_start = at(http.responseTick, at(http.headersTick, end));
  timing.response_end = end;
  timing.redirect_count = uint16_t(http.redirected ? 1 : 0);
  timing.status = uint16_t(http.status);
  timing.encoded_size = http.encodedBytes;
  timing.decoded_size = http.decodedBytes;
  timing.protocol = http.protocol;
  timing.name = url.substr(0, url.find('#'));
  std::string tao;
  for (const auto& field : http.headers) {
    std::string key = field.first;
    for (auto& c : key) c = char(std::tolower(static_cast<unsigned char>(c)));
    if (key == "timing-allow-origin") tao += (tao.empty() ? "" : ",") + field.second;
    if (key != "content-type") continue;
    timing.content_type = field.second.substr(0, field.second.find(';'));
    while (!timing.content_type.empty() && timing.content_type.back() == ' ') timing.content_type.pop_back();
  }
  // The frame load's loading principal is the parent document's.
  if (target.parent) {
    JS::RootedValue origin(cx_);
    {
      JSAutoRealm realm(cx_, target.parent->page);
      if (!target.parent->environment->Origin(&origin)) return false;
    }
    std::string parent;
    if (origin.isString()) {
      JS::RootedString text(cx_, origin.toString());
      if (!ToUTF8(cx_, text, &parent)) return false;
    }
    const std::string self = domain::browser_environment::UrlValue(timing.name).origin();
    const bool same = !parent.empty() && parent != "null" && parent == self;
    timing.body_allowed = same;
    timing.timing_allowed = same ||
        domain::execution_scheduling::ResourceTimingAccess::TimingOrigin(tao, parent);
  }
  return scheduling_.RecordNavigation(target.page, timing);
}
// Fission: a frame document of another site than the top-level document runs in
// its own content process. Owners whose state Firefox keeps per process start
// over for it; today that is the platform voice list (SPEECH_SYNTHESIS owner).
bool PageLoad::IsolateCrossSiteFrame(BrowsingContext& target, const std::string& url) {
  BrowsingContext* top = &target;
  while (top->parent) top = top->parent;
  JS::RootedValue href(cx_);
  {
    JSAutoRealm realm(cx_, top->page);
    if (!top->environment->URL(&href)) return false;
  }
  if (!href.isString()) return true;
  JS::RootedString text(cx_, href.toString());
  std::string topUrl;
  if (!ToUTF8(cx_, text, &topUrl)) return false;
  namespace rn = domain::resource_network;
  if (rn::CookieSite(rn::CookieURL(url)) != rn::CookieSite(rn::CookieURL(topUrl)) && target.speech)
    target.speech->IsolateVoices();
  return true;
}
bool PageLoad::CompleteBlankFrame(BrowsingContext& target) {
  return FireFrameElementLoad(target);
}
// Runs once at the end of the child's completion task (or synchronously for
// an initial about:blank frame). Parent completion waits for live children.
// The event uses the parent realm; listener errors follow the original Fire.
bool PageLoad::FireFrameElementLoad(BrowsingContext& target) {
  BrowsingContext* parent = target.parent;
  if (!parent || !frames_.Live(target)) return true;
  JSAutoRealm realm(cx_, parent->page);
  JS::RootedObject element(cx_);
  if (!parent->document->WrapNode(target.frame_element, &element)) return false;
  return !element || parent->interaction->Fire(element, "load", false, false);
}
// Host `document_start`: install or clear the diagnostic source.
bool PageLoad::DocumentStartRequest(JS::HandleObject request, JS::MutableHandleValue result) {
  std::string source;
  if (!resource_wire::Fields(cx_, request, {"id", "op", "source"}) ||
      !StringProperty(cx_, request, "source", &source)) return false;
  // Bounded like any other host-supplied script, so a runaway caller cannot
  // make every later document carry an unbounded body.
  if (source.size() > (1u << 20)) {
    JS_ReportErrorASCII(cx_, "document-start script must be at most 1 MiB");
    return false;
  }
  SetDocumentStartScript(std::move(source));
  JS::RootedObject summary(cx_, JS_NewPlainObject(cx_));
  if (!summary ||
      !JS_DefineProperty(cx_, summary, "length", double(documentStart_.size()), JSPROP_ENUMERATE))
    return false;
  result.setObject(*summary);
  return true;
}
// Host diagnostic source run in each new document before its own scripts
// (SetDocumentStartScript). It is host code: its failure goes to that
// document's console, never to the page, and never fails the load.
void PageLoad::RunDocumentStart(Owners& owners) {
  if (documentStart_.empty() || inDocumentStart_) return;
  JSContext* cx = owners.cx;
  inDocumentStart_ = true;
  bool ok = false;
  std::string failure;
  {
    JSAutoRealm realm(cx, owners.page);
    // Compiled and run here rather than through Evaluate: Evaluate hands a
    // failure to ExceptionCapture, which makes it the host request's (the
    // load's) result. This script's failure belongs to nobody but the log.
    JS::CompileOptions options(cx);
    options.setFileAndLine("<document-start>", 1);
    JS::SourceText<mozilla::Utf8Unit> source;
    JS::RootedValue value(cx);
    if (source.init(cx, documentStart_.data(), documentStart_.size(), JS::SourceOwnership::Borrowed)) {
      JS::RootedScript script(cx, JS::Compile(cx, options, source));
      if (script) browser_environment::MarkHostScript(script);
      ok = script && JS_ExecuteScript(cx, script, &value);
    }
    if (!ok && JS_IsExceptionPending(cx)) {
      JS::ExceptionStack exception(cx);
      if (JS::StealPendingExceptionStack(cx, &exception)) {
        const ExceptionReport report = DescribeException(cx, exception.exception(), exception.stack());
        failure = report.name + ": " + report.message;
      }
    }
    JS_ClearPendingException(cx);
  }
  if (!ok) {
    // Recorded where the page's own console records go, so the host reads it
    // back with console_log(); the page never sees it.
    web_console::Record record;
    record.method = "documentStart";
    record.level = "error";
    record.text = failure.empty() ? "document-start script failed" : failure;
    if (BrowsingContext* context = Find(owners.document); context && context->console)
      context->console->Report(std::move(record));
  }
  inDocumentStart_ = false;
}
bool PageLoad::Produce(Owners& owners, JS::HandleString html, JS::MutableHandleValue result,
                       uint32_t* executed, bool frame, JS::MutableHandleObject completion,
                       const ReplayScriptOrder* order) {
  completion.set(nullptr);
  JSContext* cx = owners.cx;
  JS::RootedValue href(cx);
  BrowsingContext* context = Find(owners.document);
  if (context) context->loadCompletion = nullptr;
  web_security::Realm* security = context ? context->security.get() : nullptr;
  {
    JSAutoRealm realm(cx, owners.page);
    std::string base;
    if (!owners.environment.URL(&href) || !href.isString()) return false;
    JS::RootedString text(cx, href.toString());
    JS::RootedValue markup(cx, JS::StringValue(html));
    if (!JS_WrapValue(cx, &markup) || !ToUTF8(cx, text, &base)) return false;
    if (!cryptoReplay_.Bind(cx, owners.page, base)) return false;
    JS::RootedString source(cx, markup.toString());
    owners.platform.ClearDocumentUrls();
    resource_network::Realm* frame = nullptr;
    if (context && context->resources && &context->resources->Resources() != &resources_)
      frame = context->resources.get();
    // A new document, a new ScriptLoader: its full-parse budget starts at 0.
    if (context) context->parseBudget = 0;
    else rootParseBudget_ = 0;
    StartPreloads(source, base, frame);
    JS::RootedObject closeManager(cx);
    if (!owners.events.PrepareCloseWatcherDocument(&closeManager)) return false;
    if (!owners.document.BeginLoad(source)) return false;
    owners.events.CommitCloseWatcherDocument(closeManager);
    owners.environment.ResetActivation();
    // Each parser generation gets its own URL-keyed module map. Retain the
    // preceding session with its realm because an earlier TLA may still have
    // live jobs or promise reactions after a same-context reload.
    if (context) {
      if (context->moduleSession) {
        context->moduleSession->Retire();
        context->retiredModuleSessions.push_back(std::move(context->moduleSession));
      }
      context->moduleSession = std::make_unique<ModuleSession>(cx, owners.resources);
    }
    if (security) security->BeginParse();
    // Host diagnostic source, before this document's first script, so it can
    // observe a page that replaces an API in its own first script and can
    // reach a realm a navigation creates later (issue #53).
    RunDocumentStart(owners);
    const uint64_t generation = owners.document.ScriptGeneration();
    const uint64_t ticket = ticket_;
    *executed = 0;
    // Deferred external scripts run in document order after parsing ends.
    JS::RootedObjectVector deferred(cx);
    JS::RootedVector<JSString*> sources(cx);
    std::vector<std::string> names;
    std::vector<bool> modules;
    JS::RootedObjectVector replayScripts(cx);
    JS::RootedVector<JSString*> replaySources(cx);
    std::vector<std::string> replayNames;
    JS::RootedObject script(cx);
    JS::RootedString code(cx);
    for (;;) {
      if (owners.document.ScriptGeneration() != generation || Started(ticket)) goto finished;
      if (!owners.document.NextScript(&script)) return false;
      if (!script) break;
      std::string filename;
      bool defer = false;
      bool module = false;
      if (!Source(owners, script, base, &code, &filename, &defer, nullptr, &module)) return false;
      if (!code) continue;
      if (order && order->Contains(filename)) {
        JS::RootedValue async(cx);
        if (!owners.document.ScriptAttribute(script, u"async", &async)) return false;
        if (module || async.isNull()) {
          JS_ReportErrorASCII(cx, "replay order accepts only async classic scripts");
          return false;
        }
        for (const auto& previousName : replayNames) {
          if (previousName == filename) {
            JS_ReportErrorASCII(cx, "replay script order requires unique occurrences");
            return false;
          }
        }
        if (!replayScripts.append(script) || !replaySources.append(code)) return false;
        replayNames.push_back(filename);
        continue;
      }
      if (!defer) {
        // Only inline scripts have a parser position; external ones start at 1:1.
        EvalOptions position;
        JS::RootedValue src(cx);
        if (!owners.document.ScriptAttribute(script, u"src", &src) ||
            (src.isNull() && !InlinePosition(owners, script, &position)) ||
            !WaitForStyleSheets(context) ||
            !Execute(owners, script, code, filename, executed, 0, position)) return false;
        continue;
      }
      if (!deferred.append(script) || !sources.append(code)) {
        JS_ReportOutOfMemory(cx);
        return false;
      }
      names.push_back(std::move(filename));
      modules.push_back(module);
    }
    if (owners.document.ScriptGeneration() != generation || Started(ticket)) goto finished;
    // CSP reports made while parsing run before the lifecycle events.
    if (security && !security->EndParse()) return false;
    JS::RootedObject document(cx, owners.document.document());
    if (!document || !owners.document.SetReadyState(document_model::ReadyState::Interactive) ||
        !owners.events.Fire(document, "readystatechange", false, false)) return false;
    if (!Milestone(owners.page, MilestoneKind::Interactive)) return false;
    if (owners.document.ScriptGeneration() != generation) goto finished;
    if (order) {
      if (replayNames.size() != order->urls.size()) {
        JS_ReportErrorASCII(cx, "replay script order did not match the prepared scripts");
        return false;
      }
      for (const auto& name : order->urls) {
        size_t index = 0;
        while (index < replayNames.size() && replayNames[index] != name) ++index;
        if (index == replayNames.size()) {
          JS_ReportErrorASCII(cx, "replay script order has an unresolved entry");
          return false;
        }
        script = replayScripts[index]; code = replaySources[index];
        if (!WaitForStyleSheets(context) || !Execute(owners, script, code, name, executed)) return false;
        if (scheduling_.Drain(10000).exhausted) {
          JS_ReportErrorASCII(cx, "recorded script boundary exceeded the task budget");
          return false;
        }
        if (JS_IsExceptionPending(cx)) CaptureOperationFailure(cx);
        if (owners.document.ScriptGeneration() != generation || Started(ticket)) goto finished;
      }
    }
    // Modules and classic defer scripts share parser order, but modules use the
    // realm's ModuleSession instead of classic Execute/currentScript behavior.
    for (size_t index = 0; index < deferred.length(); ++index) {
      if (owners.document.ScriptGeneration() != generation || Started(ticket)) goto finished;
      script = deferred[index];
      code = sources[index];
      if (!modules[index]) {
        if (!WaitForStyleSheets(context) || !Execute(owners, script, code, names[index], executed)) return false;
        continue;
      }
      if (!context || !context->moduleSession) {
        if (!ReportModuleFailure(owners, script, names[index])) return false;
        continue;
      }
      ModuleSession& session = *context->moduleSession;
      JS::RootedObject module(cx);
      JS::RootedValue promise(cx);
      std::string sourceText;
      if (!ToUTF8(cx, code, &sourceText)) {
        if (!ReportModuleFailure(owners, script, names[index])) return false;
        if (owners.document.ScriptGeneration() != generation || Started(ticket)) goto finished;
        continue;
      }
      bool loaded = false;
      {
        ModuleSession::Scope scope(session);
        loaded = session.Load(names[index], sourceText, &module);
        if (loaded) loaded = session.Evaluate(module, &promise);
      }
      if (!loaded) {
        if (!ReportModuleFailure(owners, script, names[index])) return false;
        if (owners.document.ScriptGeneration() != generation || Started(ticket)) goto finished;
        continue;
      }
      if (promise.isObject()) {
        JS::RootedObject promiseObject(cx, &promise.toObject());
        const bool rejectedSynchronously =
            JS::GetPromiseState(promiseObject) == JS::PromiseState::Rejected;
        JS::RootedValue rejection(cx, JS::UndefinedValue());
        if (rejectedSynchronously) rejection.set(JS::GetPromiseResult(promiseObject));
        JS::RootedObject ignored(cx);
        // Until Interaction gains its promise-rejection error-event port, mark
        // module evaluation handled without a callback retaining realm owners.
        // TODO: report rejected TLA through events.FireErrorEvent(window, init).
        if (!JS::AddPromiseReactionsIgnoringUnhandledRejection(
                cx, promiseObject, ignored, ignored)) {
          if (!ReportModuleFailure(owners, script, names[index])) return false;
          if (owners.document.ScriptGeneration() != generation || Started(ticket)) goto finished;
          continue;
        }
        if (rejectedSynchronously) {
          JS_SetPendingException(cx, rejection);
          if (!ReportModuleFailure(owners, script, names[index])) return false;
          if (owners.document.ScriptGeneration() != generation || Started(ticket)) goto finished;
          continue;
        }
      }
      ++*executed;
      checkpoint_(cx);
      if (JS_IsExceptionPending(cx)) CaptureOperationFailure(cx);
    }
    if (owners.document.ScriptGeneration() != generation || Started(ticket)) goto finished;
    if (!Milestone(owners.page, MilestoneKind::ContentLoaded)) return false;
    if (!owners.events.Fire(document, "DOMContentLoaded", true, false)) return false;
    checkpoint_(cx);
    if (JS_IsExceptionPending(cx)) CaptureOperationFailure(cx);
    if (owners.document.ScriptGeneration() != generation) goto finished;
    // A top-level document focuses its first `autofocus` candidate (Firefox
    // flushes autofocus candidates after parsing; ARCHITECTURE.md, host input).
    if (!frame && !owners.document.RunAutofocus()) return false;
    if (owners.document.ScriptGeneration() != generation) goto finished;
    if (!QueueCompletion(owners, frame, completion)) return false;
  }
finished:
  // An abandoned parse still delivers what it reported (EndParse is idempotent).
  if (security && !security->EndParse()) return false;
  if (!owners.environment.URL(&href)) return false;
  if (!JS_WrapValue(cx, &href)) return false;
  JS::RootedObject summary(cx, JS_NewPlainObject(cx));
  if (!summary || !JS_DefineProperty(cx, summary, "url", href, JSPROP_ENUMERATE) ||
      !JS_DefineProperty(cx, summary, "scripts", double(*executed), JSPROP_ENUMERATE)) return false;
  result.setObject(*summary);
  return true;
}
}

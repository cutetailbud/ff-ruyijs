#ifndef RUYIJS_PAGE_LOAD_H
#define RUYIJS_PAGE_LOAD_H
#include "runtime/runtime.h"
#include "runtime/replay_script_order.h"
#include "runtime/crypto_replay.h"
#include <memory>
#include "resource_network/realm.h"
#include "resource_network/page_http.h"
#include "resource_network/async_http.h"
#include <map>
#include <memory>
namespace ruyijs::resource_network { class AsyncHttp; }
#include "document_model/form_submission.h"

namespace ruyijs::browser_environment { class Environment; }
namespace ruyijs::document_model { class Realm; }
namespace ruyijs::interaction { class Realm; }
namespace ruyijs::execution_scheduling { class Realm; }
namespace ruyijs::web_platform { class Realm; }
namespace ruyijs {
struct BrowsingContext;
// The response's Origin-Agent-Cluster field value ("" when absent).
inline std::string AgentClusterHeader(const resource_network::HttpResult& http) {
  for (const auto& field : http.headers) {
    std::string key = field.first;
    for (auto& c : key) if (c >= 'A' && c <= 'Z') c = char(c - 'A' + 'a');
    if (key == "origin-agent-cluster") return field.second;
  }
  return std::string();
}
class Frames;
class TopLevel;
// Application coordinator for the `page` RPC (docs/owners/PAGE_LOAD.md) and for
// every document load, including an iframe `src` navigation. It holds no
// browser state: Browser Environment commits the URL, Document Model owns the
// parser, readyState and currentScript, the resource owner serves script bytes,
// and Interaction fires the lifecycle events. One implementation runs the same
// fetch/parse/execute/lifecycle path for the top-level document and for a
// frame; only the owner set it runs against changes. A cross-document
// top-level navigation produces its document in a new realm built by TopLevel
// (docs/owners/TOP_LEVEL.md).
class PageLoad final {
 public:
  PageLoad(JSContext* cx, Checkpoint checkpoint, TopLevel& top, resource_network::Model& resources,
           execution_scheduling::Realm& scheduling, Frames& frames)
      : cx_(cx), checkpoint_(checkpoint), top_(top), resources_(resources),
        scheduling_(scheduling), frames_(frames) {}
  static bool ScriptPort(void*, document_model::Realm&, interaction::Realm&,
      browser_environment::Environment&, JS::HandleObject, JS::HandleObject, bool);
  static bool FormDataPort(void*, document_model::Realm&,
      const std::vector<document_model::FormField>&, JS::MutableHandleObject);
  static bool FormNavigatePort(void*, document_model::Realm&, JS::HandleObject,
      JS::HandleObject, const document_model::FormSubmission&);
  bool Request(JS::HandleObject request, JS::MutableHandleValue result);
  bool ConfigureCrypto(JS::HandleObject request, bool frozen, JS::MutableHandleValue result) {
    return cryptoReplay_.Configure(cx_, request, frozen, result);
  }
  bool CryptoStatus(JS::MutableHandleValue result) { return cryptoReplay_.Status(cx_, result); }
  // The Context's asynchronous transport, for speculative preloads (below).
  void SetTransport(resource_network::AsyncHttp& transport) { transport_ = &transport; }
  // A JS-driven navigation requested through the Browser Environment's
  // navigation port (Location.assign/href/component setters, replace, reload).
  // As in Firefox the caller's script continues: the navigation is queued as
  // one task on the Context's task queue, and a later request replaces one
  // that has not run yet.
  // A reload repeats how the active document was loaded: a browser-started
  // load is reloaded as one (Firefox keeps the original load's triggering
  // principal and user activation; trace 20261021-160155).
  bool RequestNavigation(const std::string& url, bool replace, bool reload = false, bool browser = false);
  // The host `page` action "navigate": a navigation the browser starts, as from
  // the address bar (no initiator document, user activation).
  bool BrowserNavigation(const std::string& url);
  // Performs a top-level navigation inside a task, in Firefox's two steps.
  // Here the target is fetched through the one resource owner with the current
  // document as the source; the response then commits as a later task, so the
  // current document's work that came due meanwhile runs first. The commit
  // unloads that document, builds the next document's realm, commits the
  // response URL and one session-history entry and runs the same parse/
  // execute/lifecycle path as `load`. With |commitInline| the commit runs on
  // this call instead of a later task; only the host `navigate` action uses it
  // (BrowserNavigation), where there is no page script to keep running.
  bool Navigate(const std::string& url, bool replace, const std::string& method = "GET",
                const std::string& body = "", const std::string& contentType = "",
                bool browserInitiated = false, bool commitInline = false);
  // Fires the trusted beforeunload a starting navigation of the active
  // top-level document fires (listener errors are reported, not thrown).
  bool BeforeUnload();
  // The same load path against another browsing context's owners, used by the
  // runtime frame owner for an iframe `src` navigation. The child's Browser
  // Environment stores the URL itself: a frame has no profile model and no
  // session-history entry of its own.
  bool LoadFrame(BrowsingContext& target, const std::string& url,
                 const std::string& method = "GET", const std::string& body = "",
                 const std::string& contentType = "");
  // An iframe `src` navigation as Firefox runs it: the frame keeps its current
  // document and LoadFrame runs as a later task on the one Scheduling queue, so
  // the script that inserted the frame or set `src` finishes first and the
  // child's scripts start on their own stack. The queued task is the child's
  // completion token, so the parent's `load` waits for it; a newer load of the
  // same frame or the frame's retirement makes it do nothing.
  bool QueueFrameLoad(BrowsingContext& target, const std::string& url);
  // The same load path with the markup handed in directly, for an iframe
  // `srcdoc` document: the child commits `about:srcdoc` (origin inherited from
  // its creator) and the one parser produces the inline document.
  bool LoadFrameHtml(BrowsingContext& target, const std::string& html);
  // A newly connected frame whose first load has no target keeps its
  // about:blank document; this completes it by firing the frame element's
  // `load`, as Firefox does during insertion.
  bool CompleteBlankFrame(BrowsingContext& target);
  // Diagnostic: source the runtime runs in every document this Context builds,
  // before any of that document's own scripts, including the documents a
  // navigation or a frame creates later. A page whose first script replaces an
  // API, or whose error only surfaces in a realm the host never gets to touch,
  // can be observed this way; an empty string removes it. The script is host
  // code and its own failure is reported to the console, never to the page
  // (issue #53).
  void SetDocumentStartScript(std::string source) { documentStart_ = std::move(source); }
  const std::string& DocumentStartScript() const { return documentStart_; }
  // The host `document_start` operation: {"source": "..."} installs or, with an
  // empty string, removes it; the reply reports the installed length.
  bool DocumentStartRequest(JS::HandleObject request, JS::MutableHandleValue result);
 private:
  bool IsolateCrossSiteFrame(BrowsingContext& target, const std::string& url);
  bool RecordFrameNavigation(BrowsingContext& target, const std::string& url,
                             const resource_network::HttpResult& http, double origin, double responseEnd, double qpc);
  // The owners one document load runs against. The main page's are the ones
  // this coordinator was constructed with; a frame's come from its browsing
  // context, so the code below is shared instead of duplicated.
  struct Owners {
    JSContext* cx;
    JS::HandleObject page;
    browser_environment::Environment& environment;
    document_model::Realm& document;
    interaction::Realm& events;
    resource_network::Model& resources;
    web_platform::Realm& platform;
  };
  // Runs the document-start script in `owners`' realm, after the document
  // exists and before its first script. A failure never fails the load.
  void RunDocumentStart(Owners& owners);
  // The context a document belongs to: a frame, or the active top-level one.
  BrowsingContext* Find(const document_model::Realm& document) const;
  // Firefox's ScriptLoader::ShouldApplyDelazifyStrategy for the document `owners`
  // load: true, and `length` is charged to that document, while it stays below
  // dom.script_loader.delazification.max_size (10 MB) of fully parsed script
  // text. An inline script has no fetched text and asks with 0.
  bool ClaimFullParse(Owners& owners, size_t length);
  // The resource model view of a context; the first document's realm owns it.
  resource_network::Model& Network(BrowsingContext& target) const;
  Owners OwnersOf(BrowsingContext& target, JS::HandleObject page);
  bool Started(uint64_t ticket) const { return ticket != ticket_; }
  // One lifecycle milestone of the document being produced, recorded on its own
  // navigation record at the same change point the state changes
  // (docs/owners/NAVIGATION_TIMING.md). A document with no record yet (the host
  // `load` entry point) starts one whose origin is now.
  enum class MilestoneKind { Interactive, ContentLoaded, Complete, LoadStart, LoadEnd };
  bool Milestone(JS::HandleObject page, MilestoneKind kind);
  static bool DeliverNavigation(JSContext*, unsigned, JS::Value*);
  static bool DeliverCommit(JSContext*, unsigned, JS::Value*);
  bool Commit(uint64_t ticket);
  // The one fetched response waiting for its commit task. A newer navigation
  // replaces it; its ticket keeps a stale commit from running.
  struct Pending {
    uint64_t ticket = 0;
    std::string url;
    bool replace = false;
    resource_network::HttpResult http;
    // This navigation's timing record, measured against the same clock the
    // scheduling owner reads (docs/owners/NAVIGATION_TIMING.md).
    double origin = 0, fetch_start = 0, request_start = 0, response_start = 0, response_end = 0;
    double dns_start = 0, dns_end = 0, connect_start = 0, secure_start = 0, connect_end = 0;
    uint16_t redirects = 0;
    bool browserInitiated = false;
  };
  std::unique_ptr<Pending> pending_;
  bool Load(Owners& owners, const std::string& url, JS::HandleString html,
            JS::MutableHandleValue result, const ReplayScriptOrder* order = nullptr);
  // Parsing ends at DCL; completion is posted to this document's task queue.
  // The host load entry may receive the task to wait for its completion.
  bool Produce(Owners& owners, JS::HandleString html, JS::MutableHandleValue result,
               uint32_t* executed, bool frame, JS::MutableHandleObject completion,
               const ReplayScriptOrder* order = nullptr);
  bool QueueCompletion(Owners&, bool frame, JS::MutableHandleObject);
  bool Complete(JS::HandleObject task);
  bool WaitForCompletion(JS::HandleObject task);
  static bool DeliverCompletion(JSContext*, unsigned, JS::Value*);
  static bool DeliverFrameLoad(JSContext*, unsigned, JS::Value*);
  bool RunFrameLoad(JS::HandleObject task);
  bool ReportModuleFailure(Owners& owners, JS::HandleObject script,
                           const std::string& filename);
  // The iframe load event steps: a trusted `load` at the frame element in the
  // parent document, through the parent's one dispatch tree. Skipped when the
  // context is no longer the element's live one.
  bool FireFrameElementLoad(BrowsingContext& target);
  bool Source(Owners& owners, JS::HandleObject script, const std::string& base,
              JS::MutableHandleString code, std::string* filename, bool* defer,
              bool* load_error = nullptr, bool* module = nullptr);
  static JSString* ScriptBody(JSContext*, const resource_network::HttpResult&);
  // Speculative preload (donor nsHtml5SpeculativeLoad): the markup's classic
  // script URLs are fetched in parallel from this Context's
  // asynchronous transport while the parser runs, so a parser-blocking script
  // usually finds its body already in flight. A miss falls back to the
  // synchronous fetch; ordering and execution are unchanged.
  struct Preload {
    bool done = false, ok = false;
    resource_network::HttpResult http;
  };
  void StartPreloads(JS::HandleString markup, const std::string& base,
                     resource_network::Realm* frame = nullptr);
  // Preloads are keyed by request context: the URL for this Context's root
  // Model, the Model's address and the URL for a frame's own Model.
  std::string PreloadKey(const resource_network::Model&, const std::string& url) const;
  bool TakePreload(const std::string& key, resource_network::HttpResult* http, bool* failed = nullptr);
  // One speculative GET for a script URL (no-op when one is already in flight
  // or there is no asynchronous transport); PreloadPending is true while it is
  // still waiting for its response. With |frame|, the request runs on that
  // frame's own asynchronous source (its Referer, cookies, Resource Timing).
  bool StartPreload(const std::string& url, resource_network::Realm* frame = nullptr);
  bool WaitForStyleSheets(BrowsingContext* context);
  bool PreloadPending(const std::string& key) const;
  static void FinishPreload(void* slot, resource_network::AsyncHttpResult&& result) noexcept;
  // The URL a dynamically inserted external classic script will fetch, when
  // its fetch may start before the delivery task (no enforced CSP).
  bool DynamicPreloadUrl(Owners&, JS::HandleObject script, const std::string& base, std::string* url);
  // |position| places an inline parser script at its document line/column.
  bool Execute(Owners& owners, JS::HandleObject script, JS::HandleString code,
               const std::string& filename, uint32_t* executed, int phase = 0,
               const EvalOptions& position = {}, const char* introduction = nullptr);
  bool InlinePosition(Owners& owners, JS::HandleObject script, EvalOptions* position);
  bool Dynamic(Owners&, JS::HandleObject script, bool external);
  static bool DeliverDynamic(JSContext*, unsigned, JS::Value*);
  static bool DeliverForm(JSContext*, unsigned, JS::Value*);
  JSContext* cx_;
  Checkpoint checkpoint_;
  resource_network::AsyncHttp* transport_ = nullptr;
  std::map<std::string, std::shared_ptr<Preload>> preloads_;
  size_t rootParseBudget_ = 0;  // a document without a browsing context
  TopLevel& top_;
  resource_network::Model& resources_;
  execution_scheduling::Realm& scheduling_;
  Frames& frames_;
  CryptoReplayPlan cryptoReplay_;
  // Firefox lets one navigation of a traversable be ongoing: each request (a
  // Location navigation, a form submission plan, a host `load`) takes the next
  // ticket, and a queued navigation whose ticket is stale does nothing. A
  // document still being parsed stops at the next script once a newer
  // navigation started, as Firefox stops the current load.
  uint64_t ticket_ = 0;
  // How many navigation loads are on the stack. A document's own scripts may
  // navigate again, so the product bounds that recursion. Frame loads share the
  // same budget: they run re-entrantly from the parent's own parse.
  uint32_t depth_ = 0;
  // SetDocumentStartScript's source; empty when nothing is installed.
  std::string documentStart_;
  // Reentrancy guard: the document-start script's own document.write or
  // navigation must not run it again inside itself.
  bool inDocumentStart_ = false;
};
}
#endif

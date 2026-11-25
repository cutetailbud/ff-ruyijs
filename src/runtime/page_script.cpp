// Shared script fetch and execution for the original PageLoad coordinator.
#include "runtime/page_load.h"
#include "runtime/browsing_context.h"
#include "runtime/exception_capture.h"
#include "document_model/realm.h"
#include "interaction/realm.h"
#include "execution_scheduling/realm.h"
#include "resource_network/page_http.h"
#include "web_platform/text_codec.h"
#include "domain/document_model/script_state.h"
#include "jsapi.h"
#include "js/CharacterEncoding.h"
#include "js/Realm.h"
#include "js/String.h"
#include <cctype>
namespace ruyijs {
namespace { void NoCheckpoint(JSContext*) {} }
namespace {
bool ModuleType(const std::string& type) {
  size_t first = 0, last = type.size();
  auto space = [](unsigned char value) {
    return value == '\t' || value == '\n' || value == '\f' || value == '\r' || value == ' ';
  };
  while (first < last && space(static_cast<unsigned char>(type[first]))) ++first;
  while (last > first && space(static_cast<unsigned char>(type[last - 1]))) --last;
  return last - first == 6 &&
      std::tolower(static_cast<unsigned char>(type[first])) == 'm' &&
      std::tolower(static_cast<unsigned char>(type[first + 1])) == 'o' &&
      std::tolower(static_cast<unsigned char>(type[first + 2])) == 'd' &&
      std::tolower(static_cast<unsigned char>(type[first + 3])) == 'u' &&
      std::tolower(static_cast<unsigned char>(type[first + 4])) == 'l' &&
      std::tolower(static_cast<unsigned char>(type[first + 5])) == 'e';
}
}
// One fetched classic script body (ScriptLoadHandler::TrySetDecoder): the BOM's
// encoding (BOM removed), else the response's charset parameter, else the
// fallback, here UTF-8 (the document encoding of a UTF-8 page). Decoding
// replaces malformed bytes with U+FFFD as Firefox's DecodeToUTF16 does, so a
// stray Latin-1 byte in a comment never stops the script. Null on OOM/limit.
JSString* PageLoad::ScriptBody(JSContext* cx, const resource_network::HttpResult& http) {
  std::string_view bytes(http.body);
  std::string encoding;
  if (bytes.rfind("\xEF\xBB\xBF", 0) == 0) { encoding = "UTF-8"; bytes.remove_prefix(3); }
  else if (bytes.rfind("\xFE\xFF", 0) == 0) { encoding = "UTF-16BE"; bytes.remove_prefix(2); }
  else if (bytes.rfind("\xFF\xFE", 0) == 0) { encoding = "UTF-16LE"; bytes.remove_prefix(2); }
  else {
    for (const auto& field : http.headers) {
      std::string name = field.first;
      for (auto& c : name) c = char(std::tolower(static_cast<unsigned char>(c)));
      if (name != "content-type") continue;
      std::string value = field.second;
      for (auto& c : value) c = char(std::tolower(static_cast<unsigned char>(c)));
      const auto at = value.find("charset=");
      if (at == std::string::npos) continue;
      std::string label = field.second.substr(at + 8);
      label = label.substr(0, label.find(';'));
      while (!label.empty() && (label.front() == ' ' || label.front() == '"' || label.front() == '\''))
        label.erase(label.begin());
      while (!label.empty() && (label.back() == ' ' || label.back() == '"' || label.back() == '\''))
        label.pop_back();
      std::string resolved;
      if (web_platform::EncodingFromLabel(label, &resolved)) encoding = resolved;
    }
    if (encoding.empty()) encoding = "UTF-8";
  }
  if (encoding == "UTF-8") {
    // The common case stays on the UTF-8 path; only malformed input decodes.
    JSString* text = JS_NewStringCopyUTF8N(cx, JS::UTF8Chars(bytes.data(), bytes.size()));
    if (text) return text;
    JS_ClearPendingException(cx);
  }
  const auto decoded = web_platform::DecodeBytes(encoding, bytes);
  if (decoded.error) { JS_ReportOutOfMemory(cx); return nullptr; }
  return JS_NewUCStringCopyN(cx, decoded.text.data(), decoded.text.size());
}
bool PageLoad::Source(Owners& owners, JS::HandleObject script, const std::string& base,
                      JS::MutableHandleString code, std::string* filename, bool* defer,
                      bool* load_error, bool* module) {
  JSContext* cx = owners.cx;
  code.set(nullptr);
  if (defer) *defer = false;
  if (module) *module = false;
  if(load_error)*load_error=false;
  bool is_module = false;
  auto failed=[&]{
    if(load_error){*load_error=true;return true;}
    if (!is_module) return owners.events.Fire(script,"error",false,false);
    const bool fired = owners.events.Fire(script,"error",false,false);
    JS_ClearPendingException(cx);
    return fired;
  };
  JS::RootedValue type(cx), skip(cx), src(cx), async(cx), deferred(cx);
  std::string text, reference, url;
  if (!owners.document.ScriptAttribute(script, u"type", &type)) return false;
  if (type.isString()) {
    JS::RootedString value(cx, type.toString());
    if (!ToUTF8(cx, value, &text)) return false;
  }
  is_module = ModuleType(text);
  if (module) *module = is_module;
  if (is_module && !module) return true;
  if (!is_module && !ruyijs::domain::document_model::script_classic<char>(text)) return true;
  if (!is_module) {
    if (!owners.document.ScriptAttribute(script, u"nomodule", &skip)) return false;
    if (!skip.isNull()) return true;
  }
  if (!owners.document.ScriptAttribute(script, u"src", &src)) return false;
  // The document's Content Security owner; a parser-inserted script is the
  // one prepared without a load_error report (docs/owners/CONTENT_SECURITY.md).
  BrowsingContext* context = Find(owners.document);
  web_security::Realm* security = context ? context->security.get() : nullptr;
  const bool parser_inserted = load_error == nullptr;
  if (src.isNull()) {
    // This slice handles parser-inserted external modules only. Inline module
    // evaluation and dynamic module preparation remain outside this owner pass.
    if (is_module) return true;
    JS::RootedValue inline_source(cx);
    if (!owners.document.ScriptText(script, &inline_source)) return false;
    *filename = base;
    JS::RootedString text(cx, inline_source.isString() ? inline_source.toString() : JS_GetEmptyString(cx));
    bool allowed = true;
    if (security && !security->AllowScript(script, nullptr, text, parser_inserted, &allowed)) return false;
    if (!allowed) return true;  // A blocked inline script does not run and fires nothing.
    if (inline_source.isString()) code.set(inline_source.toString());
    return true;
  }
  JS::RootedString value(cx, src.toString());
  if (!ToUTF8(cx, value, &reference) ||
      !owners.document.ScriptAttribute(script, u"async", &async) ||
      !owners.document.ScriptAttribute(script, u"defer", &deferred)) return false;
  // Parser modules are deferred by definition in this bounded slice. Async
  // parser modules are out of scope and are skipped instead of misordering them.
  if (is_module) {
    if (!async.isNull()) return true;
    if (reference.empty()) return true;
    if (defer) *defer = true;
  } else if (defer) {
    *defer = async.isNull() && !deferred.isNull();
  }
  if (reference.rfind("//", 0) == 0) reference = base.substr(0, base.find("://") + 1) + reference;
  if (!reference.empty() && resource_network::ResolveURL(base, reference, &url) && security) {
    // CSP is checked before the request: a blocked script is never fetched and
    // its element gets `error`, as a failed load.
    bool allowed = true;
    JS::RootedString none(cx);
    if (!security->AllowScript(script, &url, none, parser_inserted, &allowed)) return false;
    if (!allowed) return failed();
  }
  if (reference.empty() || !resource_network::ResolveURL(base, reference, &url)) return failed();
  // The script's filename is the request URL, never a redirect target (donor
  // ScriptLoader::FillCompileOptionsForRequest: "use aRequest->URI(), not the
  // final URI of the channel"); stacks therefore name the URL the page asked for.
  *filename = url;
  resource_network::HttpResult http;
  // A speculative preload already in flight for this URL supplies the body;
  // otherwise the same request runs synchronously, as before.
  // The speculative table belongs to classic scripts. Module top-level source
  // and its import graph are fetched by the module owner through Resource.
  bool preload_failed = false;
  if (is_module || !TakePreload(PreloadKey(owners.resources, url), &http, &preload_failed)) {
    if (preload_failed) return failed();
    resource_network::HttpOptions options;options.initiator="script";
    if (!resource_network::PerformHttp(owners.resources, "GET", url, {}, "", &http,
                                       resource_network::RedirectMode::Follow, options) ||
        http.status < 200 || http.status > 299)
      return failed();
  }
  JS::RootedString body(cx, ScriptBody(cx, http));
  if (!body) {  // Undecodable script bytes are reported on the element.
    JS_ClearPendingException(cx);
    return failed();
  }
  code.set(body);
  return true;
}
// A parser-inserted inline script compiles at the document position of its
// text (Firefox ScriptLoader uses the element's line and column numbers).
bool PageLoad::InlinePosition(Owners& owners, JS::HandleObject script, EvalOptions* position) {
  uint32_t line = 0, column = 0;
  if (!owners.document.ScriptPosition(script, &line, &column)) return false;
  if (line) *position = EvalOptions({}, int(line) - 1, int(column) - 1, false, false);
  return true;
}
bool PageLoad::Execute(Owners& owners, JS::HandleObject script, JS::HandleString code,
                       const std::string& filename, uint32_t* executed, int phase,
                       const EvalOptions& position, const char* introduction) {
  // Parser scripts checkpoint before evaluation (donor ScriptLoader), while
  // currentScript still names their caller. Dynamic inline execution remains
  // inside its caller's JS stack; it must not add this parser boundary.
  JSContext* cx = owners.cx;
  JS::RootedValue value(cx);
  JS::RootedObject previous(cx);
  bool ok = true;
  const uint64_t generation = owners.document.ScriptGeneration();
  if (phase == 0) {
    const uint64_t ticket = ticket_;
    checkpoint_(cx);
    if (JS_IsExceptionPending(cx)) CaptureOperationFailure(cx);
    if (owners.document.ScriptGeneration() != generation || Started(ticket) ||
        scheduling_.Retired(owners.page)) return true;
  }
  // Every classic page script is parsed in full while its document's budget
  // lasts (ScriptLoader::ShouldApplyDelazifyStrategy): an external one is
  // charged its length, an inline one asks with 0.
  JS::RootedValue sourceAttribute(cx);
  if (!owners.document.ScriptAttribute(script, u"src", &sourceAttribute)) return false;
  const bool fullParse = ClaimFullParse(owners, sourceAttribute.isNull() ? 0 : JS_GetStringLength(code));
  if (!owners.document.CurrentScript(&previous) || !owners.document.SetCurrentScript(script) ||
      !Evaluate(cx, owners.page, code, filename.c_str(), NoCheckpoint, &value, &ok, position,
                introduction, false, fullParse))
    return false;
  if(phase==2)checkpoint_(cx);
  if(JS_IsExceptionPending(cx))CaptureOperationFailure(cx);
  if(owners.document.ScriptGeneration()==generation && !owners.document.SetCurrentScript(previous))return false;
  ++*executed;
  if(phase==0)checkpoint_(cx);
  if (JS_IsExceptionPending(cx)) CaptureOperationFailure(cx);
  return true;
}
}

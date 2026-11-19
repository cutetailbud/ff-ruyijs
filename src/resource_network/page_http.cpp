#include "resource_network/page_http.h"
#include "runtime/transport_partition.h"
#include "resource_network/http_exchange.h"
#include "resource_network/necko_http.h"
#include "resource_network/model_storage.h"
#include "domain/browser_environment/url_value.h"
#include "domain/resource_network/data_url.h"
#include "domain/resource_network/request_defaults.h"
#include "domain/resource_network/cors.h"
#include <chrono>
#include <cstdlib>
#include <map>
#include <mutex>
#include <string>
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifdef _WIN32
#include <windows.h>
#endif

namespace ruyijs::resource_network {
namespace {
using UrlValue = ruyijs::domain::browser_environment::UrlValue;
namespace domain = ruyijs::domain::resource_network;
bool Failure(HttpResult* result, const char* message) {
  *result = HttpResult{};
  result->error = message;
  return false;
}
// Fetch "CORS-preflight cache": one per Context, keyed by its transport
// partition (a shared engine's sessions share the process, not the cache).
std::mutex& PreflightLock() { static std::mutex lock; return lock; }
std::map<std::string, domain::CorsPreflightEntry>& PreflightCache() {
  static std::map<std::string, domain::CorsPreflightEntry> cache; return cache;
}
double PreflightNow() {
  return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();
}
std::string PreflightKey(const std::string& origin, const std::string& url, bool credentials) {
  return CurrentPartition()->key + '\n' + origin + '\n' + url + '\n' + (credentials ? '1' : '0');
}
bool IsInitiator(const char* initiator, const char* name) { return std::string_view(initiator) == name; }
// One request's whole budget in seconds, redirects included: Context(http_timeout=...)
// sets RUYIJS_HTTP_TIMEOUT for the process, or for an engine session's partition,
// and the Necko helper reads the same value for each hop (#45). The chain gets
// 5 s more than one hop, so the helper's own deadline error reaches the page.
uint32_t ChainLimitMs() {
  std::string text;
  if (const auto partition = CurrentPartition())
    for (const auto& [name, value] : partition->environment)
      if (name == "RUYIJS_HTTP_TIMEOUT") text = value;
  if (text.empty())
    if (const char* inherited = std::getenv("RUYIJS_HTTP_TIMEOUT")) text = inherited;
  char* end = nullptr;
  const double seconds = text.empty() ? 0 : std::strtod(text.c_str(), &end);
  const bool valid = end && *end == '\0' && seconds > 0 && seconds <= 3600;
  return uint32_t(((valid ? seconds : 15) + 5) * 1000);
}
}
bool ResolveURL(std::string_view base, std::string_view input, std::string* url) {
  UrlValue resolved;
  if (!UrlValue::Resolve(std::string(input), std::string(base), &resolved)) return false;
  *url = resolved.href();
  return true;
}

// The hop part of the former PerformHttp loop: defaults, Cookie, the NetLog
// record, and a hop that needs no transport (blob: URL, resource directory).
bool HttpExchange::Prepare(HttpResult* local) {
  const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now() - started_).count();
  const auto limit = ChainLimitMs();
  if (elapsed >= limit) { Fail("HTTP redirect chain timed out"); return false; }
  remaining_ = uint32_t(limit - elapsed);
  crossOriginTainted_ = crossOriginTainted_ || source_.origin() != address_.origin();
  const bool network = address_.protocol() == "http:" || address_.protocol() == "https:";
  const bool crossOrigin = source_.origin() != address_.origin();
  if (options_.mode == "same-origin" && crossOrigin && network) {
    Fail("Cross-origin request rejected by same-origin mode"); return false;
  }
  // A hop the resource directory serves is the caller's own stand-in for the
  // network, so it is neither preflighted nor CORS-checked.
  corsHop_ = enforceCors_ && network && (crossOrigin || originTainted_);
  servedLocally_ = false;
  if (corsHop_) {
    ResourceSnapshot stored; bool found = false;
    servedLocally_ = model_.Get(address_.href(), &stored, &found).code == ErrorCode::None && found;
  }
  std::vector<std::string> unsafeNames;
  preflighting_ = corsHop_ && !servedLocally_ && !preflightPassed_ && NeedsPreflight(&unsafeNames);
  if (preflighting_) {
    outgoing_ = {{"Access-Control-Request-Method", method_}};
    std::string names;
    for (const auto& name : unsafeNames) names += (names.empty() ? "" : ",") + name;
    if (!names.empty()) outgoing_.emplace_back("Access-Control-Request-Headers", std::move(names));
  } else {
    outgoing_ = headers_;
  }
  domain::RequestDefaults defaults{context_.source,address_.href(),context_.userAgent,referrer_,policy_,
    options_.initiator,options_.mode,method(),context_.languages,options_.navigation,options_.cors,redirectChain_,
    context_.site,options_.credentials==Credentials::Include,options_.urgent,options_.userActivated};
  referrer_=domain::DefaultReferrer(defaults);
  domain::AddRequestDefaults(defaults,outgoing_);
  // Fetch "append a request Origin header": CORS requests, and every request
  // whose method is not GET or HEAD.
  const std::string origin = RequestOrigin();
  const bool corsTainting = corsHop_ || (options_.cors && crossOrigin);
  if (!origin.empty() && (corsTainting || (method() != "GET" && method() != "HEAD")))
    domain::DefaultHeader(outgoing_, "Origin", domain::cors_origin_value(origin, address_.href(),
        address_.origin(), method(), policy_, corsTainting));
  // Firefox frames a request body, and an empty POST/PUT/PATCH, with Content-Length.
  if (!preflighting_ && (!body_.empty() || method_ == "POST" || method_ == "PUT" || method_ == "PATCH"))
    domain::DefaultHeader(outgoing_, "Content-Length", std::to_string(body_.size()));
  credentials_=!preflighting_&&(options_.credentials==Credentials::Include||
    (options_.credentials==Credentials::SameOrigin&&!credentialsTainted_&&source_.origin()==address_.origin()));
  if(credentials_&&model_.Cookies()) {
    std::string cookie;
    auto status=CookieJoined(model_.Cookies(),address_.href(),context_.now,true,&cookie,
        context_.site,options_.navigation,method_=="GET"||method_=="HEAD");
    if(status.code!=ErrorCode::None){Fail(status.message);return false;}
    if(!cookie.empty())outgoing_.emplace_back("Cookie",std::move(cookie));
  }
  domain::order_request_fields(outgoing_,
      preflighting_ ? domain::RefererSlot::BeforeOrigin :
      redirects_ && !options_.navigation ? domain::RefererSlot::AfterOrigin :
      IsInitiator(options_.initiator, "fetch") ? domain::RefererSlot::BeforePage :
      options_.navigation && method_ != "GET" && method_ != "HEAD" ? domain::RefererSlot::BeforeOrigin :
      domain::RefererSlot::AfterConnection);
  hopStart_=observe_?model_.HttpNow():0;
  if(observe_) {
#ifdef _WIN32
    LARGE_INTEGER counter,frequency;
    QueryPerformanceCounter(&counter);QueryPerformanceFrequency(&frequency);
    hopTick_=double(counter.QuadPart)*1000/double(frequency.QuadPart);
#else
    hopTick_=std::chrono::duration<double, std::milli>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
#endif
  }
  url_ = address_.href();
  ResourceSnapshot stored;
  bool found = false;
  const bool objectUrl = url_.compare(0, 5, "blob:") == 0;
  const bool dataUrl = url_.compare(0, 5, "data:") == 0;
  const auto status = model_.Record(method(), url_, body(), &stored, &found, &record_,
                                    !objectUrl && !dataUrl && !preflighting_);
  if (status.code != ErrorCode::None) { Failure(local, status.message); return false; }
  model_.RecordCookies(record_,outgoing_);
  if (objectUrl) {
    std::string type;
    if (method_ != "GET" || !model_.ReadObjectUrl(url_, &local->body, &type))
      { Failure(local, "Blob URL request failed"); return false; }
    local->status = 200; local->statusText = "OK"; local->kind = BodyKind::Bytes;
    local->headers = {{"content-type", std::move(type)},
                      {"content-length", std::to_string(local->body.size())}};
    model_.RecordResponse(record_, 200);
    return false;
  }
  // Fetch "scheme fetch" for data: the data URL processor's body and MIME type, no
  // network and no cookies; a URL it cannot process is a network error.
  if (dataUrl) {
    domain::DataUrl parsed;
    if (!domain::ProcessDataUrl(url_, &parsed)) { Failure(local, "Invalid data: URL"); return false; }
    local->status = 200; local->statusText = "OK"; local->kind = BodyKind::Bytes;
    local->headers = {{"content-type", std::move(parsed.mimeType)}};
    local->body = std::move(parsed.body);
    local->encodedBytes = local->decodedBytes = local->body.size();
    model_.RecordResponse(record_, 200);
    return false;
  }
  // A full observation log does not prevent the original request path from running.
  if (found) {
    local->status = stored.status;
    local->body = std::move(stored.body);
    local->headers = std::move(stored.headers);
    local->kind = stored.kind;
    local->encodedBytes=local->decodedBytes=local->body.size();
    return false;
  }
  return true;
}

HttpExchange::HttpExchange(Model& model, const std::string& method, const std::string& url,
                           const HeaderPairs& headers, const std::string& body, RedirectMode mode,
                           const HttpOptions& options)
    : model_(model), mode_(mode), options_(options), initialURL_(url),
      method_(method), body_(body), headers_(headers) {
  if (!UrlValue::Resolve(initialURL_, "", &address_)) { Fail("Invalid request URL"); return; }
  address_.set_hash("");
  if(!model_.ReadHttpContext(&context_)){Fail("Request environment unavailable");return;}
  if(!options_.source.empty())context_.source=options_.source;
  if(!options_.site.empty())context_.site=options_.site;
  if(options_.browserInitiated){context_.source.clear();context_.site.clear();}
  source_ = UrlValue(context_.source);
  referrer_=context_.source;policy_=options_.referrerPolicy;
  if(options_.referrer!="about:client") {
    if(options_.referrer.empty())referrer_.clear();
    else {UrlValue explicitURL;
      if(!UrlValue::Resolve(options_.referrer,context_.source,&explicitURL)||explicitURL.origin()!=source_.origin())referrer_.clear();
      else referrer_=explicitURL.href();}
  }
  observe_=model_.HasHttpObservation()&&*options_.initiator;
  enforceCors_=options_.cors&&(IsInitiator(options_.initiator,"fetch")||
      IsInitiator(options_.initiator,"xmlhttprequest")||IsInitiator(options_.initiator,"beacon"));
}
// The request's serialized origin; "null" once a redirect tainted it.
std::string HttpExchange::RequestOrigin() const {
  if (context_.source.empty()) return {};
  if (originTainted_) return "null";
  const std::string origin = source_.origin();
  return origin.empty() ? std::string() : origin;
}
bool HttpExchange::NeedsPreflight(std::vector<std::string>* unsafeNames) {
  *unsafeNames = domain::cors_unsafe_request_header_names(headers_);
  if (domain::cors_safelisted_method(method_) && unsafeNames->empty()) return false;
  // Firefox reuses a cached preflight only while its host has a DNS cache
  // record (CORSCacheEntry::CheckDNSCache); an IP-literal host never has one,
  // so it preflights every time (measured on Firefox 159.0).
  if (domain::CookieIP(address_.hostname())) return true;
  const bool include = options_.credentials == Credentials::Include;
  std::lock_guard<std::mutex> guard(PreflightLock());
  auto& cache = PreflightCache();
  const auto found = cache.find(PreflightKey(RequestOrigin(), address_.href(), include));
  return found == cache.end() || !found->second.Allows(method_, *unsafeNames, PreflightNow());
}
// Fetch "CORS-preflight fetch" steps 7-8: an ok status, the CORS check, and
// the requested method and fields among those allowed; a pass is cached.
bool HttpExchange::CompletePreflight(const HttpResult& response) {
  const bool include = options_.credentials == Credentials::Include;
  domain::CorsPreflightGrant grant;
  std::vector<std::string> unsafeNames = domain::cors_unsafe_request_header_names(headers_);
  if (!domain::cors_preflight_grant(response.status, response.headers, RequestOrigin(), include, &grant) ||
      !domain::cors_grant_allows(grant, method_, unsafeNames))
    return false;
  std::lock_guard<std::mutex> guard(PreflightLock());
  PreflightCache()[PreflightKey(RequestOrigin(), address_.href(), include)].Add(grant, PreflightNow());
  return true;
}
bool HttpExchange::Record() {
  return !observe_||model_.ObserveHttp({initialURL_,context_,options_,result_,timings_});
}
void HttpExchange::Fail(const char* message) {
  Failure(&result_, message);
  model_.RecordError(record_, result_.error);
  finished_ = true; ok_ = false;
}
// A failed hop keeps its own error result, as the former RequestHop failure did.
void HttpExchange::Finish(bool ok, HttpResult&& response) {
  result_ = std::move(response);
  if(!ok&&observe_){timings_.push_back({url_,{},hopStart_,model_.HttpNow()});Record();}
  finished_ = true;
  ok_ = ok && Record();
}
bool HttpExchange::Next() {
  while (!finished_) {
    HttpResult local;
    if (Prepare(&local)) return true;
    if (finished_) break;
    if (!local.error.empty()) Finish(false, std::move(local));
    else Complete(std::move(local));
  }
  return false;
}
void HttpExchange::Deliver(bool ok, HttpResult&& response) {
  model_.RecordResponse(record_, ok ? response.status : 0);
  if (!ok) {
    model_.RecordError(record_, response.error.empty() ? "Network request failed" : response.error);
    Finish(false, std::move(response));
  }
  else Complete(std::move(response));
}
// The response part of the former loop: Set-Cookie, timing, redirect policy.
void HttpExchange::Complete(HttpResult&& response) {
  if (preflighting_) {
    // The preflight leaves no Set-Cookie, timing or redirect state; the next
    // Prepare sends the actual request on the same hop.
    if (!CompletePreflight(response)) { Fail("CORS preflight did not succeed"); return; }
    preflighting_ = false;
    preflightPassed_ = true;
    return;
  }
  // Fetch "CORS check" on every response of a CORS hop, redirects included.
  if (corsHop_ && !servedLocally_ &&
      !domain::cors_check(response.headers, RequestOrigin(), options_.credentials == Credentials::Include)) {
    Fail("CORS request did not succeed"); return;
  }
  if(observe_) {
    const double end=model_.HttpNow();
    auto offset=[&](double tick) {return tick>0?std::clamp(hopStart_+tick-hopTick_,hopStart_,end):0;};
    timings_.push_back({url_,response.headers,hopStart_,end,offset(response.requestTick),
                       offset(response.responseTick),offset(response.headersTick),
                       offset(response.dnsStartTick),offset(response.dnsEndTick),
                       offset(response.connectStartTick),offset(response.secureTick),
                       offset(response.connectEndTick)});
  }
  response.finalURL = url_;
  response.crossOrigin = crossOriginTainted_;
  response.referrer = referrer_;
  response.redirected = redirects_ != 0;
  HttpContext completed;
  if(!model_.ReadHttpContext(&completed)){Fail("Request environment unavailable");return;}
  context_.now=completed.now;
  if(credentials_&&model_.Cookies())for(const auto& h:response.headers)
    if(domain::lower_header_name(h.first)=="set-cookie") {
      auto status=CookieAssign(model_.Cookies(),url_,h.second,context_.now,true,context_.site,options_.navigation);
      if(status.code!=ErrorCode::None){Fail(status.message);return;}
    }
  if (!domain::is_redirect_status(response.status)) { Finish(true, std::move(response)); return; }
  if (mode_ == RedirectMode::Error) { Fail("HTTP redirect rejected by request mode"); return; }
  if (mode_ == RedirectMode::Manual) {
    HttpResult opaque;
    opaque.finalURL = url_; opaque.kind = BodyKind::Bytes;
    opaque.opaqueRedirect = true;
    Finish(true, std::move(opaque)); return;
  }
  const std::string* location = nullptr;
  for (const auto& field : response.headers)
    if (domain::lower_header_name(field.first) == "location") { location = &field.second; break; }
  if (!location) { Finish(true, std::move(response)); return; }
  if (redirects_ >= domain::kRedirectLimit) { Fail("HTTP redirect limit exceeded"); return; }
  UrlValue next;
  if (!UrlValue::Resolve(*location, address_.href(), &next) ||
      (next.protocol() != "http:" && next.protocol() != "https:"))
    { Fail("Invalid HTTP redirect URL"); return; }
  const bool discardBody = domain::redirect_changes_to_get(response.status, method_);
  for(const auto& field:response.headers)if(domain::lower_header_name(field.first)=="referrer-policy")
    domain::ReadReferrerPolicy(field.second,&policy_);
  if(!context_.source.empty())redirectChain_.push_back(address_.href());
  if(address_.origin()!=next.origin())credentialsTainted_=true;
  // Fetch HTTP-redirect fetch step 11: leaving a hop that was already
  // cross-origin taints the request's origin, so later hops send "null".
  if(address_.origin()!=next.origin()&&source_.origin()!=address_.origin())originTainted_=true;
  preflightPassed_=false;
  domain::redirect_headers(headers_, discardBody, address_.origin() != next.origin());
  if (discardBody) { method_ = "GET"; body_.clear(); }
  next.set_hash("");
  address_ = std::move(next);
  ++redirects_;
}

bool PerformHttp(Model& model, const std::string& method, const std::string& url,
                 const HeaderPairs& headers, const std::string& body,
                 HttpResult* result, RedirectMode mode, const HttpOptions& options) {
  if (model.HasRemoteHttp()) {
    bool ok = false;
    *result = HttpResult{};
    if (!model.RemoteHttp({method, url, headers, body, mode, options}, result, &ok)) {
      *result = HttpResult{};
      result->error = "Worker network channel failed";
      return false;
    }
    return ok;
  }
  HttpExchange exchange(model, method, url, headers, body, mode, options);
  while (exchange.Next()) {
    HttpResult response;
    const bool ok = NeckoHttp(exchange.method(), exchange.url(), exchange.headers(),
                              exchange.body(), &response, exchange.remaining());
    exchange.Deliver(ok, std::move(response));
  }
  *result = std::move(exchange.result());
  return exchange.ok();
}
}

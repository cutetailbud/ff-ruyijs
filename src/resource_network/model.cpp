#include "resource_network/model.h"
#include "resource_network/model_internal.h"
#include <new>
#include <stdexcept>

namespace ruyijs::resource_network {
namespace domain = ruyijs::domain::resource_network;
Model::~Model() noexcept { delete impl_; }
Status Model::Initialize() noexcept {
  if (impl_) return {};
  try { impl_ = new Impl; return {}; }
  catch (...) { return {ErrorCode::OutOfMemory, "Resource allocation failed"}; }
}
Status Model::ShareFrom(Model& parent) noexcept {
  if (impl_) return {ErrorCode::InvalidState, "Resource model is already initialized"};
  if (!parent.impl_) return {ErrorCode::InvalidState, "Resource model is not initialized"};
  try {
    impl_ = new Impl;
    impl_->shared = &parent.impl_->resources();
    return {};
  } catch (...) { return {ErrorCode::OutOfMemory, "Resource allocation failed"}; }
}
Status Model::Put(const ResourceInput& input, ResourceStats* result) noexcept {
  if (!impl_) return {ErrorCode::InvalidState, "Resource model is not initialized"};
  if (input.url.empty()) return {ErrorCode::Type, "Resource url must not be empty"};
  if (input.status < 100 || input.status > 599)
    return {ErrorCode::Type, "Resource status must be between 100 and 599"};
  if (input.url.size() > Limits::key_bytes || input.body.size() > Limits::body_bytes ||
      input.headers.size() > Limits::header_fields)
    return {ErrorCode::Range, "Resource entry limit exceeded"};
  try {
    domain::HttpHeaders headers;
    size_t headerBytes = 0;
    for (const auto& [name, raw] : input.headers) {
      auto value = domain::trim_header_value(raw);
      if (!domain::valid_header_name(name) || !domain::valid_header_value(value))
        return {ErrorCode::Type, "Invalid HTTP header"};
      if (name.size() > Limits::header_bytes || value.size() > Limits::header_bytes - name.size() ||
          headerBytes > Limits::header_bytes - name.size() - value.size())
        return {ErrorCode::Range, "Resource header limit exceeded"};
      headerBytes += name.size() + value.size();
      headers.append(name, std::string(value));
    }
    auto& bundle = impl_->resources();
    const auto* old = bundle.find_by_url(input.url);
    if (!old && bundle.resource_count() >= Limits::resources)
      return {ErrorCode::Range, "Resource count limit exceeded"};
    const size_t bytes = input.url.size() + input.body.size() + headerBytes;
    if (bytes > Limits::resource_bytes - bundle.resource_bytes() + (old ? old->bytes() : 0))
      return {ErrorCode::Range, "Resource retained byte limit exceeded"};
    auto kind = input.kind == BodyKind::Text ? domain::ResourceBodyKind::Text : domain::ResourceBodyKind::Bytes;
    bundle.add_resource(input.url, input.body, domain::HttpStatus(input.status), std::move(headers), kind);
    if (result) *result = Inspect();
    return {};
  } catch (const std::length_error&) { return {ErrorCode::Range, "Resource size limit exceeded"}; }
  catch (...) { return {ErrorCode::OutOfMemory, "Resource allocation failed"}; }
}
Status Model::Get(std::string_view url, ResourceSnapshot* result, bool* found) const noexcept {
  if (!impl_) return {ErrorCode::InvalidState, "Resource model is not initialized"};
  if (url.empty()) return {ErrorCode::Type, "Resource url must not be empty"};
  if (url.size() > Limits::key_bytes) return {ErrorCode::Range, "Resource key limit exceeded"};
  try {
    const auto* resource = impl_->resources().find_by_path(std::string(url));
    *found = resource != nullptr;
    if (!resource) return {};
    ResourceSnapshot candidate;
    candidate.url = resource->url(); candidate.body = resource->body();
    candidate.status = resource->status().code();
    candidate.headers = resource->headers().raw_pairs();
    candidate.kind = resource->kind() == domain::ResourceBodyKind::Text ? BodyKind::Text : BodyKind::Bytes;
    *result = std::move(candidate);
    return {};
  } catch (...) { return {ErrorCode::OutOfMemory, "Resource snapshot allocation failed"}; }
}
Status Model::Record(std::string_view method, std::string_view url, std::string_view body,
                     ResourceSnapshot* result, bool* found, uint64_t* recordedId,
                     bool matchResource) noexcept {
  if (!impl_) return {ErrorCode::InvalidState, "Resource model is not initialized"};
  if (url.empty()) return {ErrorCode::Type, "Resource url must not be empty"};
  if (url.size() > Limits::key_bytes) return {ErrorCode::Range, "Resource key limit exceeded"};
  try {
    // One aggregate call: it appends to the single NetLog owner and marks the
    // entry matched with the served status. No second log is kept here.
    if (!matchResource) {
      const auto id = impl_->resources().net_log().record(
          domain::method_from_string(std::string(method)), std::string(url), std::string(body));
      if (recordedId) *recordedId = id;
      *found = false;
      return {};
    }
    const auto* resource = impl_->resources().record_and_match(
        domain::method_from_string(std::string(method)), std::string(url), std::string(body), recordedId);
    *found = resource != nullptr;
    if (!resource) return {};
    ResourceSnapshot candidate;
    candidate.url = resource->url(); candidate.body = resource->body();
    candidate.status = resource->status().code();
    candidate.headers = resource->headers().raw_pairs();
    candidate.kind = resource->kind() == domain::ResourceBodyKind::Text ? BodyKind::Text : BodyKind::Bytes;
    *result = std::move(candidate);
    return {};
  } catch (...) { return {ErrorCode::OutOfMemory, "Resource snapshot allocation failed"}; }
}
void Model::RecordResponse(uint64_t id, int status) noexcept {
  if (impl_ && id) impl_->resources().net_log().mark_response(id, status);
}
void Model::RecordError(uint64_t id, const std::string& error) noexcept {
  if (impl_ && id) try { impl_->resources().net_log().mark_error(id, error); } catch (...) {}
}
void Model::RecordCookies(uint64_t id,const HeaderPairs& headers) noexcept {
  if(!impl_||!id)return;
  try {
    std::vector<std::string> names;
    for(const auto& h:headers)if(domain::lower_header_name(h.first)=="cookie") {
      size_t start=0;
      while(start<h.second.size()&&names.size()<256) {
        auto end=h.second.find(';',start),eq=h.second.find('=',start);
        if(eq!=std::string::npos&&(end==std::string::npos||eq<end))
          names.push_back(std::string(domain::trim_header_value(h.second.substr(start,eq-start))));
        if(end==std::string::npos)break;start=end+1;
      }
    }
    impl_->resources().net_log().mark_cookies(id,std::move(names));
  } catch (...) {}  // Observation never prevents the request.
}
Status Model::Log(NetLogRecords* result) const noexcept {
  if (!impl_) return {ErrorCode::InvalidState, "Resource model is not initialized"};
  try {
    NetLogRecords records;
    const auto& entries = impl_->resources().net_log().entries();
    records.reserve(entries.size());
    for (const auto& entry : entries)
      records.push_back({entry.id(), domain::to_string(entry.method()), entry.url(),
                         entry.request_body(), entry.response_status(), entry.matched(),entry.cookie_names(),entry.error()});
    *result = std::move(records);
    return {};
  } catch (...) { return {ErrorCode::OutOfMemory, "Net log snapshot allocation failed"}; }
}
Status Model::ClearLog() noexcept {
  if (!impl_) return {ErrorCode::InvalidState, "Resource model is not initialized"};
  if (!impl_->resources().net_log().clear())
    return {ErrorCode::InvalidState, "Net log has requests in flight"};
  return {};
}
ResourceStats Model::Inspect() const noexcept {
  return impl_ ? ResourceStats{impl_->resources().resource_count(), impl_->resources().resource_bytes()} : ResourceStats{};
}
void Model::Clear() noexcept { if (impl_) impl_->resources().clear_resources(); }
}

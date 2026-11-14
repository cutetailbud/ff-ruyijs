#include "resource_network/async_http_internal.h"
#include <algorithm>
#include <limits>
#include <new>
#include <utility>

namespace ruyijs::resource_network {
AsyncHttp::AsyncHttp(Model& model)
    : lifetime_(std::make_shared<AsyncHttpLifetime>()),
      root_(std::make_shared<AsyncHttpSource>()), inbox_(std::make_shared<Inbox>()) {
  lifetime_->owner = this;
  root_->lifetime = lifetime_; root_->model = &model;
}
AsyncHttp::~AsyncHttp() {
  lifetime_->owner = nullptr;
  root_->model = nullptr;
  jobs_.clear();
}
Status AsyncHttp::OpenSource(Model& model, std::shared_ptr<AsyncHttpSource>* result) noexcept {
  if (root_->model == &model) { *result = root_; return {}; }
  try {
    auto source = std::make_shared<AsyncHttpSource>();
    source->lifetime = lifetime_; source->model = &model;
    *result = std::move(source); return {};
  } catch (...) { return {ErrorCode::OutOfMemory, "Async HTTP source allocation failed"}; }
}
void AsyncHttp::DetachSource(AsyncHttpSource& source) noexcept {
  if (source.lifetime != lifetime_) return;
  source.model = nullptr;
  for (auto it = jobs_.begin(); it != jobs_.end();) {
    if (it->second->source.get() == &source) it = jobs_.erase(it);
    else ++it;
  }
  std::erase_if(queued_, [&](uint64_t id) { return jobs_.find(id) == jobs_.end(); });
  inbox_->TrimLocal([&](uint64_t id) { return jobs_.find(id) != jobs_.end(); });
  // Active transports release their slots only when their native outcomes arrive.
  PumpQueue();
}
void AsyncHttp::Cancel(const std::shared_ptr<AsyncHttpSource>& source,AsyncHttpTicket ticket) noexcept {
  if (!source || source->lifetime!=lifetime_ || !ticket.id) return;
  auto found=jobs_.find(ticket.id);
  if (found==jobs_.end() || found->second->source!=source) return;
  // A running transport stops its helper transfer now (issue #43); its late
  // outcome still returns the capacity.
  found->second->cancel->Cancel();
  jobs_.erase(found);
  std::erase(queued_,ticket.id);
  inbox_->TrimLocal([&](uint64_t id){return jobs_.find(id)!=jobs_.end();});
  PumpQueue();
}
Status StartAsyncHttp(const std::shared_ptr<AsyncHttpSource>& source,const NativeHttpRequest& request,
                      const AsyncHttpCompletion& completion,AsyncHttpTicket* ticket) noexcept {
  if (!source || !source->lifetime || !source->lifetime->owner)
    return {ErrorCode::InvalidState,"Async HTTP source is detached"};
  return source->lifetime->owner->Start(source,request,completion,ticket);
}
void CancelAsyncHttp(const std::shared_ptr<AsyncHttpSource>& source,AsyncHttpTicket ticket) noexcept {
  if (source && source->lifetime && source->lifetime->owner)
    source->lifetime->owner->Cancel(source,ticket);
}
void DetachAsyncHttpSource(const std::shared_ptr<AsyncHttpSource>& source) noexcept {
  if (source && source->lifetime && source->lifetime->owner)
    source->lifetime->owner->DetachSource(*source);
}
Status AsyncHttp::Start(const std::shared_ptr<AsyncHttpSource>& source,
    const NativeHttpRequest& request, const AsyncHttpCompletion& completion,
    AsyncHttpTicket* ticket) noexcept {
  if (!completion.finish) return {ErrorCode::InvalidState, "Async HTTP completion is missing"};
  return Admit(source, request, completion, {}, ticket);
}
Status AsyncHttp::Admit(const std::shared_ptr<AsyncHttpSource>& source,
    const NativeHttpRequest& request, const AsyncHttpCompletion& completion,
    Done legacy, AsyncHttpTicket* ticket) noexcept {
  if (!source || source->lifetime != lifetime_ || !source->model || lifetime_->owner != this)
    return {ErrorCode::InvalidState, "Async HTTP source is detached or foreign"};
  if (next_ == std::numeric_limits<uint64_t>::max() || jobs_.size() >= kAsyncPending)
    return {ErrorCode::Range, "Async HTTP pending limit exceeded"};
  try {
    const uint64_t id = next_;
    auto job = std::make_unique<Job>(source, request, completion, std::move(legacy));
    job->outcome->id = id;
    jobs_.emplace(id, std::move(job));
    ++next_;
    ticket->id = id;
    Advance(id);
    return {};
  } catch (...) { return {ErrorCode::OutOfMemory, "Async HTTP admission allocation failed"}; }
}
void AsyncHttp::Start(const std::string& method, const std::string& url, const HeaderPairs& headers,
    const std::string& body, RedirectMode redirect, const HttpOptions& options, Done done) {
  AsyncHttpTicket ticket;
  auto status = Admit(root_, {method, url, headers, body, redirect, options}, {}, std::move(done), &ticket);
  // Legacy void admission already propagated allocation failure. It must never
  // invoke an image callback synchronously or discard an unadmitted callback.
  if (status.code != ErrorCode::None) throw std::bad_alloc();
}
}

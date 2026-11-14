#include "resource_network/async_http_internal.h"
#include "runtime/transport_partition.h"
#include "resource_network/necko_http.h"
#include <new>
#include <system_error>
#include <thread>
#include <utility>

namespace ruyijs::resource_network {
void AsyncHttp::Fail(Job& job, AsyncHttpOutcome failure) noexcept {
  job.outcome->transport = false;
  job.outcome->result = {};
  job.outcome->result.outcome = failure;
  inbox_->Push(job.outcome);
}
void AsyncHttp::Advance(uint64_t id) noexcept {
  Job& job = *jobs_.at(id);
  try {
    if (job.exchange.Next()) {
      if (active_ < kAsyncActive) Launch(id);
      else queued_.push_back(id);
      return;
    }
    job.outcome->transport = false;
    job.outcome->result = {};
    inbox_->Push(job.outcome);
  } catch (...) { Fail(job, AsyncHttpOutcome::OutOfMemory); }
}
void AsyncHttp::Launch(uint64_t id) noexcept {
  Job& job = *jobs_.at(id);
  const HttpExchange& exchange = job.exchange;
  try {
    // Captures are copied while still on the original owning thread.
    // The transport thread leases helpers of the owning session's partition.
    auto task = [inbox = inbox_, outcome = job.outcome, method = exchange.method(),
                 url = exchange.url(), headers = exchange.headers(), body = exchange.body(),
                 remaining = exchange.remaining(), partition = CurrentPartition(),
                 cancel = job.cancel]() noexcept {
      PartitionScope scope(partition);
      outcome->transport = true;
      outcome->result = {};
      try {
        outcome->result.ok = NeckoHttp(method, url, headers, body,
                                       &outcome->result.response, remaining, cancel.get());
      } catch (const std::bad_alloc&) {
        outcome->result.outcome = AsyncHttpOutcome::OutOfMemory;
      } catch (...) { outcome->result.outcome = AsyncHttpOutcome::LaunchFailure; }
      inbox->Push(outcome);
    };
    std::thread transport(std::move(task));
    ++active_;
    // A freshly joinable thread normally detaches without allocation. On an OS
    // detach failure join it here; its preallocated outcome still runs later.
    try { transport.detach(); } catch (...) { transport.join(); }
  } catch (const std::bad_alloc&) { Fail(job, AsyncHttpOutcome::OutOfMemory); }
    catch (...) { Fail(job, AsyncHttpOutcome::LaunchFailure); }
}
}

#include "resource_network/necko_http.h"
#include "resource_network/necko_helper.h"
#include "resource_network/necko_wire.h"
#include "runtime/transport_partition.h"
#include <chrono>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

namespace ruyijs::resource_network {
namespace {
// Each original helper session retains its native connection manager. Origin
// is idle-session affinity, not an origin-wide request lock. Broken framing
// ends that session; its next lease starts a new one.
struct Session {
  NeckoHelper worker;
  NeckoPipe input, output;
  bool Start(const TransportPartition& partition) {
    std::wstring python;
    if (!NeckoPythonPath(&python) || !input.Open(true) || !output.Open(false) ||
        !worker.Start(python, input, output, L"--session --timing-v3", &partition)) return false;
    input.CloseChild();
    output.CloseChild();
    return true;
  }
};
// A lease admits exactly one framed transaction per helper pipe. A slot
// belongs to one transport partition (transport_partition.h) and never serves
// another, so sessions do not share connections or TLS session caches.
struct Slot {
  std::unique_ptr<Session> session;
  PartitionRef partition;
  std::string origin;
  uint64_t used = 0;
  bool busy = false;
  // Its partition was released while a lease was out; it goes when that ends.
  bool retired = false;
};
// Firefox limits connections per server (network.http.max-persistent-
// connections-per-server = 6), not per process: one origin holds at most six
// helpers, while the pool admits the asynchronous transport's 16 active
// requests plus the JS thread's synchronous one, so a few requests that hang
// until their deadline do not starve every other origin. Both limits are per
// partition; a single-session process has one.
constexpr size_t kSessions = 18;
constexpr size_t kPerOrigin = 6;
struct Pool {
  std::mutex lock;
  std::condition_variable available;
  std::vector<std::unique_ptr<Slot>> slots;
  uint64_t tick = 0;
  // A shared engine's helper budget across partitions (0: none).
  size_t limit = 0;
};
// Process lifetime: transport threads can still be inside NeckoHttp when the
// process exits, so the pool is never destroyed by static destruction (that
// destroyed sessions under a running request and closed their handles twice).
// Helpers belong to a KILL_ON_JOB_CLOSE job and end when the process does.
Pool& Sessions() { static Pool* pool = new Pool; return *pool; }
// Takes `slot` out of the pool; the caller destroys it after the lock, so a
// helper process is never waited for while other requests wait on the pool.
std::unique_ptr<Slot> Remove(Pool& pool, Slot* slot) {
  for (auto it = pool.slots.begin(); it != pool.slots.end(); ++it)
    if (it->get() == slot) {
      std::unique_ptr<Slot> removed = std::move(*it);
      pool.slots.erase(it);
      return removed;
    }
  return nullptr;
}
struct Lease {
  Slot* slot = nullptr;
  ~Lease() {
    if (!slot) return;
    auto& pool = Sessions();
    std::unique_ptr<Slot> dropped;
    {
      std::lock_guard<std::mutex> guard(pool.lock);
      slot->busy = false;
      if (slot->retired) dropped = Remove(pool, slot);
    }
    // Waiters differ by origin: wake them all, each rechecks its own limit.
    pool.available.notify_all();
  }
};
bool Mine(const Slot& slot, const TransportPartition& partition) {
  return slot.partition->key == partition.key;
}
// Room for one more helper under the engine budget: the least recently used
// idle helper of another partition makes way when the budget is spent.
bool Room(Pool& pool, const TransportPartition& partition, std::unique_ptr<Slot>* evicted) {
  if (!pool.limit || pool.slots.size() < pool.limit) return true;
  Slot* oldest = nullptr;
  for (auto& slot : pool.slots)
    if (!slot->busy && !Mine(*slot, partition) && (!oldest || slot->used < oldest->used))
      oldest = slot.get();
  if (!oldest) return false;
  *evicted = Remove(pool, oldest);
  return true;
}
// scheme://host[:port] of an absolute URL, lower-cased, without userinfo.
std::string OriginOf(const std::string& url) {
  const size_t scheme = url.find("://");
  if (scheme == std::string::npos) return std::string();
  const size_t end = url.find_first_of("/?#", scheme + 3);
  std::string authority = url.substr(scheme + 3, end == std::string::npos ? end : end - scheme - 3);
  const size_t at = authority.rfind('@');
  if (at != std::string::npos) authority.erase(0, at + 1);
  std::string origin = url.substr(0, scheme + 3) + authority;
  for (char& c : origin) if (c >= 'A' && c <= 'Z') c = char(c - 'A' + 'a');
  return origin;
}
bool Acquire(const PartitionRef& partition, const std::string& origin, ULONGLONG deadline,
             Lease* lease, NeckoCancellation* cancel) {
  auto& pool = Sessions();
  std::unique_lock<std::mutex> guard(pool.lock);
  for (;;) {
    if (cancel && cancel->Cancelled()) return false;
    Slot* chosen = nullptr;
    std::unique_ptr<Slot> evicted;
    size_t busy = 0, owned = 0;
    for (auto& slot : pool.slots) {
      if (!Mine(*slot, *partition)) continue;
      ++owned;
      if (slot->busy && slot->origin == origin) ++busy;
    }
    if (busy < kPerOrigin) {
      for (auto& slot : pool.slots)
        if (Mine(*slot, *partition) && !slot->busy && slot->origin == origin) {
          chosen = slot.get();
          break;
        }
      // A running helper serves any origin (its connection manager keeps one
      // pool per origin), so an idle one is preferred to starting a process.
      if (!chosen)
        for (auto& slot : pool.slots)
          if (Mine(*slot, *partition) && !slot->busy && slot->session &&
              (!chosen || slot->used < chosen->used)) chosen = slot.get();
      if (!chosen && owned < kSessions && Room(pool, *partition, &evicted)) {
        pool.slots.push_back(std::make_unique<Slot>());
        chosen = pool.slots.back().get();
        chosen->partition = partition;
      }
      if (!chosen)
        for (auto& slot : pool.slots)
          if (Mine(*slot, *partition) && !slot->busy && (!chosen || slot->used < chosen->used))
            chosen = slot.get();
    }
    if (chosen) {
      chosen->origin = origin;
      chosen->used = ++pool.tick;
      chosen->busy = true;
      lease->slot = chosen;
      guard.unlock();
      evicted.reset();
      return true;
    }
    const ULONGLONG now = GetTickCount64();
    if (now >= deadline) return false;
    pool.available.wait_for(guard, std::chrono::milliseconds(deadline - now));
  }
}
}
void NeckoCancellation::Cancel() noexcept {
  {
    std::lock_guard<std::mutex> guard(lock_);
    if (cancelled_) return;
    cancelled_ = true;
    if (helper_) helper_->Cancel();
  }
  // Wakes a request waiting for a helper. Taking the pool lock first means
  // that request is either before its cancellation check or already waiting.
  auto& pool = Sessions();
  { std::lock_guard<std::mutex> guard(pool.lock); }
  pool.available.notify_all();
}
bool NeckoCancellation::Cancelled() noexcept {
  std::lock_guard<std::mutex> guard(lock_);
  return cancelled_;
}
bool NeckoCancellation::Attach(NeckoHelper* helper) noexcept {
  std::lock_guard<std::mutex> guard(lock_);
  if (cancelled_) return false;
  helper_ = helper;
  return true;
}
void NeckoCancellation::Detach() noexcept {
  std::lock_guard<std::mutex> guard(lock_);
  helper_ = nullptr;
}
bool NeckoHttp(const std::string& method, const std::string& url, const HeaderPairs& headers,
               const std::string& body, HttpResult* result, uint32_t timeoutMs,
               NeckoCancellation* cancel) {
  const ULONGLONG deadline = GetTickCount64() + timeoutMs;
  const std::string origin = OriginOf(url);
  const PartitionRef partition = CurrentPartition();
  // A helper can disappear between two requests (proxy tunnel reset, worker
  // restart, or a half-closed pipe).  In that case no response frame exists,
  // so retrying the same request on a fresh helper is safe and preserves the
  // original deadline.  Once a frame was read, HTTP status semantics belong to
  // the server and must be returned without transport-level retry.
  for (unsigned attempt = 0; attempt != 2; ++attempt) {
    Lease lease;
    if (!Acquire(partition, origin, deadline, &lease, cancel)) {
      result->error = cancel && cancel->Cancelled() ? "Necko request aborted"
                                                    : "Necko session capacity deadline exceeded";
      return false;
    }
    auto& session = lease.slot->session;
    if (!session) {
      session = std::make_unique<Session>();
      if (!session->Start(*lease.slot->partition)) {
        session.reset();
        result->error = "Necko worker startup failed";
        if (attempt == 0 && GetTickCount64() < deadline) continue;
        return false;
      }
    }
    if (cancel && !cancel->Attach(&session->worker)) {
      result->error = "Necko request aborted";
      return false;
    }
    HttpResult candidate;
    bool framed = false;
    const bool written = WriteNeckoRequest(session->input, method, url, headers, body, deadline);
    const bool read = written && ReadNeckoResponse(session->output, &candidate, deadline, &framed);
    if (cancel) cancel->Detach();
    // A cancelled transfer may have ended its helper, even after a complete
    // response; that session is not reused and the request is not retried.
    if (cancel && cancel->Cancelled()) {
      session.reset();
      result->error = "Necko request aborted";
      return false;
    }
    if (read) {
      *result = std::move(candidate);
      return true;
    }
    if (!framed) session.reset();
    result->error = candidate.error.empty() ? "Necko request failed" : std::move(candidate.error);
    if (!framed && attempt == 0 && GetTickCount64() < deadline) continue;
    return false;
  }
  result->error = "Necko request failed";
  return false;
}
// Starts helpers in the background until the calling thread's partition has
// `count`, so the first requests of a Context do not wait for the helper's
// interpreter; each becomes an idle slot. A session joining a partition that
// already has them starts none.
void PrewarmNeckoSessions(size_t count) noexcept {
  const PartitionRef partition = CurrentPartition();
  for (size_t i = 0; i < count; ++i) {
    Slot* slot = nullptr;
    {
      auto& pool = Sessions();
      std::lock_guard<std::mutex> guard(pool.lock);
      size_t owned = 0;
      for (auto& existing : pool.slots) owned += Mine(*existing, *partition);
      if (owned >= count || owned >= kSessions) return;
      // A warm-up never evicts another partition's helper.
      if (pool.limit && pool.slots.size() >= pool.limit) return;
      pool.slots.push_back(std::make_unique<Slot>());
      slot = pool.slots.back().get();
      slot->partition = partition;
      slot->busy = true;  // leased by the warm-up until its helper runs
    }
    std::thread([slot, partition] {
      Lease lease;
      lease.slot = slot;
      auto session = std::make_unique<Session>();
      const bool started = session->Start(*partition);
      auto& pool = Sessions();
      std::lock_guard<std::mutex> guard(pool.lock);
      if (started) slot->session = std::move(session);
    }).detach();
  }
}
void SetNeckoHelperLimit(size_t limit) noexcept {
  auto& pool = Sessions();
  std::lock_guard<std::mutex> guard(pool.lock);
  pool.limit = limit;
}
void ReleaseNeckoPartition(const std::string& key) noexcept {
  std::vector<std::unique_ptr<Slot>> dropped;
  auto& pool = Sessions();
  {
    std::lock_guard<std::mutex> guard(pool.lock);
    for (auto it = pool.slots.begin(); it != pool.slots.end();) {
      if ((*it)->partition->key != key) { ++it; continue; }
      if ((*it)->busy) { (*it)->retired = true; ++it; continue; }
      dropped.push_back(std::move(*it));
      it = pool.slots.erase(it);
    }
  }
  pool.available.notify_all();
}
}

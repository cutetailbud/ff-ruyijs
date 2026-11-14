#ifndef RUYIJS_NECKO_HTTP_H
#define RUYIJS_NECKO_HTTP_H
#include "resource_network/page_http.h"
#include <mutex>
namespace ruyijs::resource_network {
class NeckoHelper;
// Cancellation of one transport request (issue #43). Cancel() ends the helper
// serving it, if any: its sockets close, so the server stops sending (Firefox
// closes an HTTP/1.1 connection whose response it cancels), and the request
// is not retried. A cancelled request waiting for a helper gives up at once.
class NeckoCancellation {
 public:
  void Cancel() noexcept;
  bool Cancelled() noexcept;
  // The helper about to carry the request; false when already cancelled.
  bool Attach(NeckoHelper* helper) noexcept;
  // Before the helper's session may be destroyed or reused.
  void Detach() noexcept;
 private:
  std::mutex lock_;
  bool cancelled_ = false;
  NeckoHelper* helper_ = nullptr;
};
bool NeckoHttp(const std::string& method, const std::string& url, const HeaderPairs&,
               const std::string& body, HttpResult* result, uint32_t timeoutMs = 20000,
               NeckoCancellation* cancel = nullptr);
// Background start of helper sessions at Context start (necko_http.cpp), for
// the calling thread's transport partition.
void PrewarmNeckoSessions(size_t count) noexcept;
// A shared engine's helper budget across partitions; 0 keeps no global limit.
void SetNeckoHelperLimit(size_t limit) noexcept;
// Ends a closed session's helpers; one still in use goes when it is returned.
void ReleaseNeckoPartition(const std::string& key) noexcept;
}
#endif

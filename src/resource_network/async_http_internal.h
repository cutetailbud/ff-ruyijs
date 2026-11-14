#pragma once
#include "resource_network/async_http.h"
#include "resource_network/http_exchange.h"
#include "resource_network/necko_http.h"
#include <condition_variable>
#include <mutex>

namespace ruyijs::resource_network {
struct AsyncHttpLifetime { AsyncHttp* owner = nullptr; };
struct AsyncHttpSource {
  std::shared_ptr<AsyncHttpLifetime> lifetime;
  Model* model = nullptr;
};
struct AsyncHttp::Outcome {
  uint64_t id = 0;
  bool transport = false;
  AsyncHttpResult result;
  std::shared_ptr<Outcome> next;
};
// Intrusive native node ownership; neither publication nor removal allocates.
struct AsyncHttp::Inbox {
  std::mutex lock;
  std::condition_variable ready;
  std::shared_ptr<Outcome> head, tail;
  ~Inbox() { while (Pop()) {} }
  void Push(const std::shared_ptr<Outcome>& node) noexcept {
    {
      std::lock_guard<std::mutex> guard(lock);
      if (tail) tail->next = node;
      else head = node;
      tail = node;
    }
    ready.notify_all();
  }
  std::shared_ptr<Outcome> Pop() noexcept {
    std::lock_guard<std::mutex> guard(lock);
    auto result = std::move(head);
    if (result) {
      head = std::move(result->next);
      if (!head) tail.reset();
    }
    return result;
  }
  template<class Keep> void TrimLocal(Keep keep) noexcept {
    std::lock_guard<std::mutex> guard(lock);
    auto* slot = &head;
    tail.reset();
    while (*slot) {
      auto& node = *slot;
      if (!node->transport && !keep(node->id)) {
        auto removed = std::move(node);
        node = std::move(removed->next);
      } else { tail = node; slot = &node->next; }
    }
  }
};
struct AsyncHttp::Job {
  std::shared_ptr<AsyncHttpSource> source;
  std::shared_ptr<Outcome> outcome;
  std::string initiator;
  HttpOptions options;
  HttpExchange exchange;
  AsyncHttpCompletion completion;
  Done legacy;
  // Shared with its running transport thread; Cancel ends that transfer.
  std::shared_ptr<NeckoCancellation> cancel = std::make_shared<NeckoCancellation>();
  Job(const std::shared_ptr<AsyncHttpSource>& owner, const NativeHttpRequest& request,
      const AsyncHttpCompletion& callback, Done done)
      : source(owner), outcome(std::make_shared<Outcome>()),
        initiator(request.options.initiator ? request.options.initiator : ""),
        options(request.options),
        exchange(*owner->model, request.method, request.url, request.headers, request.body,
                 request.redirect, PrepareOptions()), completion(callback), legacy(std::move(done)) {}
  const HttpOptions& PrepareOptions() noexcept { options.initiator = initiator.c_str(); return options; }
};
inline constexpr uint32_t kAsyncActive = 16;
inline constexpr size_t kAsyncPending = 10000;
}

// Shared engine (--ruyijs-host PIPE, docs/engine.md): one process serves
// several independent Context sessions. Each session runs RunSession on its
// own thread with its own JSContext, so sessions share the process, the
// engine's code and the self-hosted runtime data, and nothing a page can reach:
// every heap, realm, clock, cookie jar and storage area stays per session, and
// its Necko helpers belong to its own transport partition. The control pipe
// opens sessions; each session then serves its own private RPC pipe exactly as
// a single-session process does.
#include "runtime/runtime_internal.h"
#include "runtime/native_stack.h"
#include "runtime/pipe.h"
#include "runtime/transport_partition.h"
#include "resource_network/necko_http.h"
#include <atomic>
#include <cstdio>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <vector>
#include "jsapi.h"
#include "js/Array.h"
#include "js/Conversions.h"
#include "js/Exception.h"
#include "js/PropertyAndElement.h"
#include "js/String.h"
#include "shell/jsoptparse.h"

namespace ruyijs {
struct SessionRecord {
  uint64_t id = 0;
  SessionOptions options;
  PartitionRef partition;
  // A named network partition several sessions share, or empty when private.
  std::string network;
  std::atomic<bool> done{false};
  std::atomic<bool> abort{false};
  // The session's JSContext while its thread runs; guarded so the host can
  // request an interrupt without racing the thread's teardown.
  std::mutex lock;
  JSContext* cx = nullptr;
  void* thread = nullptr;
};

namespace {
thread_local SessionRecord* hosted = nullptr;
// An `interrupt` request stops the session's running script; the session then
// ends when its pipe closes.
bool AbortInterrupt(JSContext*) { return !(hosted && hosted->abort.load()); }

// A shared named partition: its sessions must agree on the transport profile.
struct Network {
  PartitionRef partition;
  size_t sessions = 0;
};

bool IntegerProperty(JSContext* cx, JS::HandleObject object, const char* key, double* output) {
  JS::RootedValue value(cx);
  if (!JS_GetProperty(cx, object, key, &value)) return false;
  if (!value.isNumber()) {
    JS_ReportErrorASCII(cx, "RPC field %s must be a number", key);
    return false;
  }
  *output = value.toNumber();
  return true;
}

class Engine final {
 public:
  Engine(JSContext* cx, const SessionThreads& threads, size_t stack)
      : cx_(cx), threads_(threads), stack_(stack) {}
  ~Engine() { Stop(); }

  bool Open(JS::HandleObject request, JS::MutableHandleValue value) {
    std::string pipe, mode, proxy, tls, network, httpTimeout, httpReadTimeout;
    if (!StringProperty(cx_, request, "pipe", &pipe) ||
        !StringProperty(cx_, request, "mode", &mode, "default") ||
        !StringProperty(cx_, request, "proxy", &proxy, "") ||
        !StringProperty(cx_, request, "tls", &tls, "") ||
        !StringProperty(cx_, request, "network", &network, "") ||
        !StringProperty(cx_, request, "httpTimeout", &httpTimeout, "") ||
        !StringProperty(cx_, request, "httpReadTimeout", &httpReadTimeout, "")) return false;
    if (mode != "default" && mode != "debug") {
      JS_ReportErrorASCII(cx_, "ruyijs mode must be default or debug");
      return false;
    }
    if (pipe.empty() || pipe.find('\0') != std::string::npos) {
      JS_ReportErrorASCII(cx_, "Session pipe name must be a nonempty string");
      return false;
    }
    double prewarm = 2;
    bool hasPrewarm = false;
    if (!JS_HasProperty(cx_, request, "prewarm", &hasPrewarm) ||
        (hasPrewarm && !IntegerProperty(cx_, request, "prewarm", &prewarm))) return false;
    if (!(prewarm >= 0 && prewarm <= 2) || prewarm != double(int(prewarm))) {
      JS_ReportErrorASCII(cx_, "Session prewarm must be 0, 1 or 2");
      return false;
    }
    auto record = std::make_unique<SessionRecord>();
    record->id = ++next_;
    record->options.rpc = pipe;
    record->options.debug = mode == "debug";
    record->options.prewarm = unsigned(prewarm);
    record->options.stack = stack_;
    record->network = network;
    // The helpers' environment, exactly what a single-session process would
    // inherit from Context(proxy=..., tls=...).
    auto partition = std::make_shared<TransportPartition>();
    partition->environment.emplace_back("RUYIJS_HTTP_PROXY", proxy);
    if (!tls.empty()) partition->environment.emplace_back("NECKO_FIREFOX", tls);
    // Context(http_timeout=..., http_read_timeout=...): the helpers' deadlines and
    // the page's redirect chain read these (#45).
    if (!httpTimeout.empty()) partition->environment.emplace_back("RUYIJS_HTTP_TIMEOUT", httpTimeout);
    if (!httpReadTimeout.empty())
      partition->environment.emplace_back("RUYIJS_HTTP_READ_TIMEOUT", httpReadTimeout);
    if (network.empty()) {
      partition->key = "session:" + std::to_string(record->id);
      record->partition = partition;
    } else {
      partition->key = "network:" + network;
      auto& shared = networks_[network];
      if (!shared.partition) {
        shared.partition = partition;
      } else if (shared.partition->environment != partition->environment) {
        if (!shared.sessions) networks_.erase(network);
        JS_ReportErrorASCII(cx_, "Network partition %s already uses another proxy, tls or HTTP timeout",
                            network.c_str());
        return false;
      }
      ++shared.sessions;
      record->partition = shared.partition;
    }
    record->thread = threads_.start(cx_, record.get());
    if (!record->thread) {
      Released(*record);
      JS_ReportErrorASCII(cx_, "Session thread could not start");
      return false;
    }
    JS::RootedObject result(cx_, JS_NewPlainObject(cx_));
    JS::RootedValue id(cx_, JS::NumberValue(double(record->id)));
    if (!result || !JS_DefineProperty(cx_, result, "session", id, JSPROP_ENUMERATE)) return false;
    sessions_.push_back(std::move(record));
    value.setObject(*result);
    return true;
  }

  bool Status(JS::MutableHandleValue value) {
    Reap();
    JS::RootedObject list(cx_, JS::NewArrayObject(cx_, 0));
    if (!list) return false;
    uint32_t index = 0;
    for (auto& record : sessions_) {
      JS::RootedObject item(cx_, JS_NewPlainObject(cx_));
      JS::RootedValue id(cx_, JS::NumberValue(double(record->id)));
      JS::RootedValue running(cx_, JS::BooleanValue(!record->done.load()));
      JS::RootedString network(cx_, JS_NewStringCopyUTF8N(
          cx_, JS::UTF8Chars(record->network.data(), record->network.size())));
      if (!item || !network || !JS_DefineProperty(cx_, item, "session", id, JSPROP_ENUMERATE) ||
          !JS_DefineProperty(cx_, item, "running", running, JSPROP_ENUMERATE) ||
          !JS_DefineProperty(cx_, item, "network", network, JSPROP_ENUMERATE) ||
          !JS_DefineElement(cx_, list, index++, item, JSPROP_ENUMERATE)) return false;
    }
    value.setObject(*list);
    return true;
  }

  bool Interrupt(JS::HandleObject request, JS::MutableHandleValue value) {
    double id = 0;
    if (!IntegerProperty(cx_, request, "session", &id)) return false;
    bool found = false;
    for (auto& record : sessions_) {
      if (double(record->id) != id) continue;
      found = true;
      record->abort = true;
      std::lock_guard<std::mutex> guard(record->lock);
      if (record->cx) JS_RequestInterruptCallback(record->cx);
    }
    value.setBoolean(found);
    return true;
  }

  // Joins every session thread whose session has ended and releases its
  // helpers; a named partition goes with its last session.
  void Reap() {
    for (auto it = sessions_.begin(); it != sessions_.end();) {
      if (!(*it)->done.load()) { ++it; continue; }
      threads_.join((*it)->thread);
      Released(**it);
      it = sessions_.erase(it);
    }
  }

  // Shutdown: running sessions are interrupted; each ends once its client has
  // closed its pipe (the Python Engine closes them before the control pipe).
  void Stop() {
    for (auto& record : sessions_) {
      if (record->done.load()) continue;
      record->abort = true;
      std::lock_guard<std::mutex> guard(record->lock);
      if (record->cx) JS_RequestInterruptCallback(record->cx);
    }
    for (auto& record : sessions_) {
      threads_.join(record->thread);
      Released(*record);
    }
    sessions_.clear();
  }

 private:
  void Released(SessionRecord& record) {
    if (record.network.empty()) {
      resource_network::ReleaseNeckoPartition(record.partition->key);
      return;
    }
    auto shared = networks_.find(record.network);
    if (shared == networks_.end() || --shared->second.sessions) return;
    resource_network::ReleaseNeckoPartition(shared->second.partition->key);
    networks_.erase(shared);
  }

  JSContext* cx_;
  SessionThreads threads_;
  size_t stack_;
  uint64_t next_ = 0;
  std::vector<std::unique_ptr<SessionRecord>> sessions_;
  std::map<std::string, Network> networks_;
};
}  // namespace

bool Host(JSContext* cx, js::cli::OptionParser* options, Checkpoint checkpoint,
          const SessionThreads& threads, size_t stack) {
  const char* name = options->getStringOption("ruyijs-host");
  if (!name) return false;
  if (!options->getMultiStringOption('e').empty() || !options->getMultiStringOption('f').empty() ||
      options->getStringArg("script") || options->getStringOption("ruyijs-rpc")) {
    JS_ReportErrorASCII(cx, "Host mode accepts sessions through the control pipe only");
    return false;
  }
  if (const int limit = options->getIntOption("ruyijs-max-helpers"); limit > 0)
    resource_network::SetNeckoHelperLimit(size_t(limit));
  // The control channel runs in the shell's global, as Run's codec does.
  JS::RootedObject codec(cx);
  if (!InitCodec(cx, &codec)) return false;
  Pipe pipe;
  if (!pipe.Connect(name)) {
    JS_ReportErrorASCII(cx, "ruyijs host pipe connection failed");
    return false;
  }
  Engine engine(cx, threads, stack);
  ExceptionCapture errors(cx);
  std::string frame;
  while (pipe.Read(&frame)) {
    engine.Reap();
    errors.BeginRequest();
    JS::RootedObject request(cx);
    JS::RootedValue id(cx, JS::NullValue()), value(cx);
    std::string operation;
    bool success = ParseRequest(cx, frame, &request) && JS_GetProperty(cx, request, "id", &id) &&
                   StringProperty(cx, request, "op", &operation);
    bool closing = false;
    if (success) {
      if (operation == "open") success = engine.Open(request, &value);
      else if (operation == "status") success = engine.Status(&value);
      else if (operation == "interrupt") success = engine.Interrupt(request, &value);
      else if (operation == "close") closing = true;
      else {
        JS_ReportErrorASCII(cx, "Unknown ruyijs host operation");
        success = false;
      }
    }
    if (!success && !FormatOperationFailure(cx, &value)) return false;
    if (closing) engine.Stop();
    if (!Reply(cx, codec, id, success, &value, checkpoint, errors, &frame)) return false;
    if (!pipe.Write(frame) || closing) return true;
  }
  return true;
}

void RunHostedSession(JSContext* cx, SessionRecord* record, Checkpoint checkpoint) {
  if (!cx) {
    fprintf(stderr, "ruyijs session %llu: the JSContext could not be set up\n",
            (unsigned long long)record->id);
    record->done = true;
    return;
  }
  hosted = record;
  SetCurrentPartition(record->partition);
  JS_AddInterruptCallback(cx, &AbortInterrupt);
  {
    std::lock_guard<std::mutex> guard(record->lock);
    record->cx = cx;
  }
  if (!RunSession(cx, record->options, nullptr, checkpoint) && JS_IsExceptionPending(cx)) {
    // A session that fails to start or serve ends alone; the host and the
    // other sessions go on. The reason goes to the engine's stderr.
    JS::ExceptionStack exception(cx);
    std::string message = "session failed";
    if (JS::StealPendingExceptionStack(cx, &exception)) {
      JS::RootedString text(cx, JS::ToString(cx, exception.exception()));
      std::string utf8;
      if (text && ToUTF8(cx, text, &utf8)) message = utf8;
    }
    JS_ClearPendingException(cx);
    fprintf(stderr, "ruyijs session %llu: %s\n", (unsigned long long)record->id, message.c_str());
  }
  {
    std::lock_guard<std::mutex> guard(record->lock);
    record->cx = nullptr;
  }
  SetCurrentPartition(nullptr);
  hosted = nullptr;
  record->done = true;
}
}

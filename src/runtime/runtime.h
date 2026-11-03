#ifndef RUYIJS_RUNTIME_H
#define RUYIJS_RUNTIME_H

#include <string>
#include "domain/runtime_context/eval_options.h"
#include "js/RootingAPI.h"
#include "js/TypeDecls.h"

namespace js::cli { class OptionParser; }

namespace ruyijs {
using EvalOptions = ruyijs::domain::runtime_context::EvalOptions;
using Checkpoint = void (*)(JSContext*);
// One Context session: the private pipe it serves and how it runs. A
// single-session process fills it from --ruyijs-rpc/--ruyijs-mode/
// --ruyijs-environment; a shared engine fills it from each `open` request
// (runtime/engine.cpp).
struct SessionThreads;
struct SessionOptions {
  std::string rpc;
  bool debug = false;
  std::string environment;
  // Necko helpers started in the background for the session's partition.
  // Linux and macOS start them on the first request. Two idle interpreters are
  // most of a Context's memory, and on those platforms they are their own
  // process group, so they keep running after the shell has exited. Windows
  // keeps 2. An engine session sets this from its open request and does not
  // use the default.
#if defined(_WIN32)
  unsigned prewarm = 2;
#else
  unsigned prewarm = 0;
#endif
  // A shared engine's session: the native stack a single-session process
  // leaves to RunSession (bytes below its entry), so recursion depth matches.
  size_t stack = 0;
  // Host mode (--ruyijs-host): RunSession, entered from the same statement of
  // Run as a single session, measures that stack and serves the engine.
  const SessionThreads* host = nullptr;
};
// Every product mode enters here from the shell: a single session
// (--ruyijs-rpc, --ruyijs-runtime) or a shared engine (--ruyijs-host).
bool Run(JSContext* cx, js::cli::OptionParser* options, Checkpoint checkpoint,
         const SessionThreads& threads);
// The session itself: builds the page and its owners, then serves `rpc` until
// the pipe closes. `cli` is the shell's parser for the -e/-f paths of a
// single-session process and nullptr for a session of a shared engine.
bool RunSession(JSContext* cx, const SessionOptions& session, js::cli::OptionParser* cli,
                Checkpoint checkpoint);
// Shared engine (--ruyijs-host PIPE): one process serves several independent
// sessions, each on its own thread with its own JSContext. The shell supplies
// how a session thread is created, because the context setup lives there.
struct SessionRecord;
struct SessionThreads {
  // Starts `record` on a new thread; nullptr when the thread cannot start.
  void* (*start)(JSContext* parent, SessionRecord* record);
  // Waits for a thread `start` returned and releases it.
  void (*join)(void* thread);
};
bool Host(JSContext* cx, js::cli::OptionParser* options, Checkpoint checkpoint,
          const SessionThreads& threads, size_t stack);
// The body of a session thread once the shell has set up its JSContext.
void RunHostedSession(JSContext* cx, SessionRecord* record, Checkpoint checkpoint);
// Worker process mode: the page Context spawned this process and connected
// the named pipe; it installs the restricted worker realm and serves the pipe.
bool RunWorker(JSContext* cx, js::cli::OptionParser* options, Checkpoint checkpoint);
// A same-origin iframe realm shares the compartment of sameOrigin.
JSObject* CreatePage(JSContext* cx, JS::HandleObject sameOrigin = nullptr);
// `host` marks code the embedder supplied (RPC eval, CLI); page scripts pass false.
bool Evaluate(JSContext* cx, JS::HandleObject page, JS::HandleString code,
              const char* filename, Checkpoint checkpoint,
              JS::MutableHandleValue value, bool* success, const EvalOptions& options = {},
              const char* introduction = nullptr, bool host = false);
// Compiles and runs a host target expression in the current realm, marked as host code.
bool EvaluateHostExpression(JSContext* cx, const char* filename, const char16_t* chars, size_t length,
                            JS::MutableHandleValue value);
bool ToUTF8(JSContext* cx, JS::HandleString input, std::string* output);
bool InitCodec(JSContext* cx, JS::MutableHandleObject codec);
bool ParseRequest(JSContext* cx, const std::string& input,
                  JS::MutableHandleObject request);
bool StringProperty(JSContext* cx, JS::HandleObject object, const char* key,
                    std::string* output, const char* fallback = nullptr);
bool EncodeReply(JSContext* cx, JS::HandleObject codec, JS::HandleValue id,
                 bool success, JS::HandleValue value, std::string* output, bool* delivered = nullptr);
}
#endif

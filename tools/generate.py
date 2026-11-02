"""Deterministic shell hooks; generated files retain the donor MPL header."""

import re
from provenance import write_if_changed


def replace_once(source, old, new):
    count = source.count(old)
    if count != 1:
        raise ValueError(f"expected one shell hook, found {count}: {old!r}")
    return source.replace(old, new, 1)


SESSION_THREADS = """\
#if defined(MOZ_MEMORY) && __has_include("mozmemory.h")
#  include "mozmemory.h"
#  define RUYIJS_SESSION_ARENA 1
#endif

struct RuyijsSessionInput {
  JSRuntime* parentRuntime;
  JS::ContextOptions options;
  ruyijs::SessionRecord* record;
};

static void RuyijsSessionMain(RuyijsSessionInput input) {
#ifdef RUYIJS_SESSION_ARENA
  // A session thread runs a page's DOM and scripts the way Firefox's main
  // thread does, and gets a jemalloc arena of its own as that thread does
  // (nsThreadManager::Init): sessions then do not contend on the lock of the
  // default arena (16 sessions building a DOM at once took 16 times the CPU
  // of one). mozjemalloc never frees an arena; declared before the context,
  // this scope exit runs after the context is gone and returns the session's
  // free pages to the system.
  jemalloc_thread_local_arena(true);
  auto releaseArena = MakeScopeExit([] { jemalloc_free_dirty_pages(); });
#endif
  JSContext* cx = JS_NewContext(JS::DefaultHeapMaxBytes, input.parentRuntime);
  auto destroyContext = MakeScopeExit([cx] {
    if (cx) JS_DestroyContext(cx);
  });
  UniquePtr<ShellContext> sc;
  bool ready = cx != nullptr;
  if (ready) {
    sc = MakeUnique<ShellContext>(cx, ShellContext::Worker);
    ready = sc && sc->registerWithCx(cx);
  }
  if (ready) {
    JS::ContextOptionsRef(cx) = input.options;
    JS_SetNativeStackQuota(cx, 128 * sizeof(size_t) * 1024);
    JS_SetTrustedPrincipals(cx, &ShellPrincipals::fullyTrusted);
    JS_SetSecurityCallbacks(cx, &ShellPrincipals::securityCallbacks);
    JS_SetWrapObjectCallbacks(cx, &ShellWrapObjectCallbacks);
    JS_AddInterruptCallback(cx, ShellInterruptCallback);
    JS::InitConsumeStreamCallback(cx, ConsumeBufferSource, ReportStreamError);
    JS::SetPromiseRejectionTrackerCallback(
        cx, ForwardingPromiseRejectionTrackerCallback);
    JS::dbg::SetDebuggerMallocSizeOf(cx, moz_malloc_size_of);
    ready = JS::InitSelfHostedCode(cx);
  }
  if (ready) {
    // As Shell() does for the main thread: the session runs inside a shell
    // global's realm, with the process-wide module load path.
    EnvironmentPreparer environmentPreparer(cx);
    JS::RealmOptions realmOptions;
    SetStandardRealmOptions(cx, realmOptions);
    RootedObject glob(cx, NewGlobalObject(cx, realmOptions, nullptr,
                                          ShellGlobalKind::WindowProxy,
                                          /* immutablePrototype = */ true));
    if (glob) {
      JSAutoRealm ar(cx, glob);
      JS::ConstUTF8CharsZ path(processWideModuleLoadPath.get(),
                               strlen(processWideModuleLoadPath.get()));
      RootedString moduleLoadPath(cx, JS_NewStringCopyUTF8Z(cx, path));
      sc->moduleLoader = js::MakeUnique<ModuleLoader>();
      if (moduleLoadPath && sc->moduleLoader &&
          sc->moduleLoader->init(cx, moduleLoadPath)) {
        {
          AutoReportException are(cx);
          ruyijs::RunHostedSession(cx, input.record, RunShellJobs);
        }
        RunShellJobs(cx);
        KillWatchdog(cx);
        return;
      }
    }
  }
  ruyijs::RunHostedSession(nullptr, input.record, RunShellJobs);
  if (cx) {
    KillWatchdog(cx);
  }
}

static void* RuyijsStartSession(JSContext* cx, ruyijs::SessionRecord* record) {
  // The main thread's 8 MiB stack, so native recursion has the same room.
  js::Thread* thread =
      js_new<js::Thread>(js::Thread::Options().setStackSize(8 * 1024 * 1024));
  if (!thread) {
    return nullptr;
  }
  RuyijsSessionInput input{JS_GetRuntime(cx), JS::ContextOptionsRef(cx), record};
  if (!thread->init(RuyijsSessionMain, std::move(input))) {
    js_delete(thread);
    return nullptr;
  }
  return thread;
}

static void RuyijsJoinSession(void* thread) {
  auto* session = static_cast<js::Thread*>(thread);
  session->join();
  js_delete(session);
}

static const ruyijs::SessionThreads kRuyijsSessionThreads = {RuyijsStartSession,
                                                             RuyijsJoinSession};

"""


def generate(donor, root, directory):
    original = donor.src / "js/src/shell/js.cpp"
    source = original.read_text(encoding="utf-8")
    source = replace_once(source, "/* JS shell. */",
                          '/* JS shell. */\n#include "runtime/runtime.h"\n'
                          '#include "runtime/exception_capture.h"\n'
                          '#include "runtime/arguments.h"')
    source = replace_once(source, "  sArgc = argc;",
                          "  ruyijs::Arguments productArguments;\n"
                          "  if (!productArguments.Prepare(&argc, &argv)) return 1;\n"
                          "  sArgc = argc;")
    source = replace_once(
        source, "!ProcessArgs(cx, op)",
        '!(op->getStringOption("ruyijs-worker") ? '
        'ruyijs::RunWorker(cx, op, RunShellJobs) : '
        'op->getBoolOption("ruyijs-runtime") || '
        'op->getStringOption("ruyijs-rpc") || '
        'op->getStringOption("ruyijs-host") ? '
        'ruyijs::Run(cx, op, RunShellJobs, kRuyijsSessionThreads) : ProcessArgs(cx, op))')
    # Shared engine: each session's thread sets up a child JSContext the way
    # the shell sets up its main one. A worker context would get the worker
    # stack quota; a session keeps the main context's options and SpiderMonkey's
    # default quota (InitDefaultStackQuota), so recursion depth and every option
    # a page can observe match a single-session process.
    source = replace_once(
        source, "static int Shell(JSContext* cx, OptionParser* op) {",
        SESSION_THREADS + "static int Shell(JSContext* cx, OptionParser* op) {")
    hook = '!op.addBoolOption(\'i\', "shell", "Enter prompt after running code")'
    options = ('!op.addBoolOption(\'\\0\', "ruyijs-runtime", '
               '"Run in the ruyijs page realm") ||\n      '
               '!op.addStringOption(\'\\0\', "ruyijs-rpc", "PIPE", '
               '"Serve ruyijs requests over a private pipe") ||\n      '
               '!op.addStringOption(\'\\0\', "ruyijs-environment", "JSON_FILE", '
               '"Configure the ruyijs environment before page execution") ||\n      '
               '!op.addStringOption(\'\\0\', "ruyijs-mode", "MODE", '
               '"Context mode; debug enables the API monitor") ||\n      '
               '!op.addStringOption(\'\\0\', "ruyijs-worker", "PIPE", '
               '"Run as a ruyijs worker connected to a private pipe") ||\n      '
               '!op.addStringOption(\'\\0\', "ruyijs-host", "PIPE", '
               '"Serve several ruyijs sessions over a private control pipe") ||\n      '
               '!op.addIntOption(\'\\0\', "ruyijs-max-helpers", "COUNT", '
               '"Necko helper budget across a host\'s sessions (0: none)", 0) ||\n      '
               '!op.addStringOption(\'\\0\', "ruyijs-worker-kind", "KIND", '
               '"Worker global kind: dedicated (default), service or shared") ||\n      ')
    source = replace_once(source, hook, options + hook)
    source = replace_once(
        source, "js::shell::AutoReportException::~AutoReportException() {",
        "js::shell::AutoReportException::~AutoReportException() {\n"
        "  if (ruyijs::CaptureShellException(cx)) return;")
    # The test shell always exposes FinalizationRegistry.prototype.cleanupSome;
    # Firefox ships javascript.options.experimental.weakrefs.expose_cleanupSome
    # off, so the product modes keep the browser default.
    source = replace_once(
        source, "  JS::Prefs::setAtStartup_experimental_weakrefs_expose_cleanupSome(true);",
        "  JS::Prefs::setAtStartup_experimental_weakrefs_expose_cleanupSome(\n"
        "      !(op.getBoolOption(\"ruyijs-runtime\") || op.getStringOption(\"ruyijs-rpc\") ||\n"
        "        op.getStringOption(\"ruyijs-worker\") || op.getStringOption(\"ruyijs-host\")));")
    rejection = ("static void ForwardingPromiseRejectionTrackerCallback(\n"
                 "    JSContext* cx, bool mutedErrors, JS::HandleObject promise,\n"
                 "    JS::PromiseRejectionHandlingState state, void* data) {")
    source = replace_once(
        source, rejection, rejection + "\n"
        "  if (ruyijs::CapturePromiseRejection(cx, promise, state)) return;")
    generated_js = directory / "js.cpp"
    # ``newline="\n"`` is rejected by the Windows text layer (EINVAL).  The
    # generated source already uses LF, so disable translation explicitly.
    write_if_changed(generated_js, source, newline="")
    unified = donor.shell / "Unified_cpp_js_src_shell0.cpp"
    includes = []

    def resolve_include(match):
        path = (donor.shell / match[1]).resolve()
        includes.append(path)
        if path == original.resolve():
            path = generated_js
        return f'#include "{path.as_posix()}"'

    unit = re.sub(r'^#include "([^"]+\.cpp)"', resolve_include,
                  unified.read_text(encoding="utf-8"), flags=re.MULTILINE)
    if includes.count(original.resolve()) != 1:
        raise ValueError("the unified shell must include js.cpp exactly once")
    generated_unit = directory / unified.name
    write_if_changed(generated_unit, unit, newline="")
    codec_path = root / "src/runtime/codec.js"
    codec = codec_path.read_text(encoding="utf-8")
    delimiter = "RUYIJS_CODEC"
    if f'){delimiter}"' in codec:
        raise ValueError("codec collides with its C++ raw-string delimiter")
    header = ('// Generated from src/runtime/codec.js. Do not edit.\n'
              '#pragma once\nnamespace ruyijs {\n'
              f'inline constexpr char kCodecSource[] = R"{delimiter}('
              f'{codec}){delimiter}";\n}}\n')
    write_if_changed(directory / "runtime_assets.h", header)
    return generated_unit, includes + [codec_path]

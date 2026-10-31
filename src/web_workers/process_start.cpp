#ifndef _WIN32
#include "web_workers/process.h"

#include <cerrno>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <spawn.h>
#include <string>
#include <sys/un.h>
#include <unistd.h>
#include <vector>

extern char** environ;

namespace ruyijs::web_workers {
namespace {
void* SlotOf(pid_t pid) { return reinterpret_cast<void*>(static_cast<intptr_t>(pid)); }

// WorkerExecutable stores each path byte in one wchar. This is the inverse.
std::string BytesOf(std::wstring_view wide) {
  std::string text;
  text.reserve(wide.size());
  for (wchar_t unit : wide) {
    const auto code = static_cast<uint32_t>(unit);
    if (code > 0xff) return {};
    text.push_back(static_cast<char>(code));
  }
  return text;
}

bool SocketName(std::string* name) {
  sockaddr_un limit{};
  const size_t cap = sizeof(limit.sun_path);
  std::string dir = "/tmp";
  if (const char* tmp = std::getenv("TMPDIR")) {
    std::string candidate(tmp);
    while (!candidate.empty() && candidate.back() == '/') candidate.pop_back();
    const size_t used = candidate.size() + sizeof("/ruyijs-") - 1 + 32 + sizeof(".sock") - 1;
    if (!candidate.empty() && candidate.front() == '/' && candidate.find("..") == std::string::npos &&
        used < cap)
      dir = std::move(candidate);
  }
  unsigned char random[16];
  arc4random_buf(random, sizeof(random));
  *name = dir + "/ruyijs-";
  static constexpr char hex[] = "0123456789abcdef";
  for (unsigned char byte : random) {
    name->push_back(hex[byte >> 4]);
    name->push_back(hex[byte & 15]);
  }
  *name += ".sock";
  return name->size() < cap;
}
}  // namespace

StartResult Process::Start(std::wstring_view executable, std::string initial, WorkerKind kind) noexcept {
  if (process_ || job_) return {StartError::Active};
  std::string program = BytesOf(executable);
  if (program.empty() || program.find('\0') != std::string::npos ||
      program.find_first_of("\r\n") != std::string::npos || access(program.c_str(), X_OK) != 0 ||
      !Pipe::ValidFrameSize(initial.size()))
    return {StartError::InvalidInput};
  auto fail = [this](StartError error, unsigned system = 0) {
    Stop();
    return StartResult{error, system};
  };
  try {
    std::string name;
    if (!SocketName(&name) || !pipe_.ListenAsync(name.c_str()) || !pipe_.Queue(std::move(initial)))
      return fail(StartError::Pipe, static_cast<unsigned>(errno));
    std::string workerFlag = "--ruyijs-worker";
    std::string kindFlag = "--ruyijs-worker-kind";
    std::string kindValue = kind == WorkerKind::Service ? "service" : kind == WorkerKind::Shared ? "shared" : "";
    std::vector<char*> argv;
    argv.push_back(program.data());
    argv.push_back(workerFlag.data());
    argv.push_back(name.data());
    if (!kindValue.empty()) {
      argv.push_back(kindFlag.data());
      argv.push_back(kindValue.data());
    }
    argv.push_back(nullptr);

    posix_spawn_file_actions_t actions;
    if (posix_spawn_file_actions_init(&actions) != 0) return fail(StartError::Job, static_cast<unsigned>(errno));
    // Match CreateProcess bInheritHandles=FALSE: keep stdio, close the RPC socket.
    // environ is still the parent's, so RUYIJS_HTTP_PROXY reaches the worker.
#if defined(__linux__)
    bool armed = posix_spawn_file_actions_addclosefrom_np(&actions, STDERR_FILENO + 1) == 0;
#else
    bool armed = posix_spawn_file_actions_addinherit_np(&actions, STDIN_FILENO) == 0 &&
                 posix_spawn_file_actions_addinherit_np(&actions, STDOUT_FILENO) == 0 &&
                 posix_spawn_file_actions_addinherit_np(&actions, STDERR_FILENO) == 0;
#endif
    posix_spawnattr_t attr;
    if (posix_spawnattr_init(&attr) != 0) {
      posix_spawn_file_actions_destroy(&actions);
      return fail(StartError::Job, static_cast<unsigned>(errno));
    }
#if defined(__linux__)
    constexpr short spawnFlags = POSIX_SPAWN_SETPGROUP;
#else
    constexpr short spawnFlags = POSIX_SPAWN_SETPGROUP | POSIX_SPAWN_CLOEXEC_DEFAULT;
#endif
    armed = armed && posix_spawnattr_setflags(&attr, spawnFlags) == 0 &&
                     posix_spawnattr_setpgroup(&attr, 0) == 0;
    pid_t child = 0;
    const int spawned = armed ? posix_spawn(&child, program.c_str(), &actions, &attr, argv.data(), environ) : -1;
    const int spawnError = spawned;
    posix_spawnattr_destroy(&attr);
    posix_spawn_file_actions_destroy(&actions);
    if (!armed) return fail(StartError::Job, static_cast<unsigned>(errno));
    if (spawned != 0 || child <= 0) return fail(StartError::Create, static_cast<unsigned>(spawnError));
    process_ = SlotOf(child);
    job_ = SlotOf(child);
    return {};
  } catch (...) { return fail(StartError::Allocation); }
}
}
#else
#include "web_workers/process.h"
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <bcrypt.h>

namespace ruyijs::web_workers {
StartResult Process::Start(std::wstring_view executable, std::string initial, WorkerKind kind) noexcept {
  if (process_ || job_) return {StartError::Active};
  if (executable.empty() || executable.size() > 32700 ||
      executable.find_first_of(L"\"\r\n") != std::wstring_view::npos ||
      executable.find(L'\0') != std::wstring_view::npos || !Pipe::ValidFrameSize(initial.size()))
    return {StartError::InvalidInput};
  auto fail = [this](StartError error, unsigned system = 0) {
    Stop();return StartResult{error, system};
  };
  try {
    unsigned char random[16];
    const auto status = BCryptGenRandom(nullptr, random, sizeof(random), BCRYPT_USE_SYSTEM_PREFERRED_RNG);
    if (status) return fail(StartError::Random, unsigned(status));
    std::string name = "\\\\.\\pipe\\ruyijs-";
    for (unsigned char byte : random) {
      name += "0123456789abcdef"[byte >> 4];name += "0123456789abcdef"[byte & 15];
    }
    if (!pipe_.ListenAsync(name.c_str()) || !pipe_.Queue(std::move(initial)))
      return fail(StartError::Pipe, GetLastError());
    job_ = CreateJobObjectW(nullptr, nullptr);
    JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits{};
    limits.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
    if (!job_ || !SetInformationJobObject(job_, JobObjectExtendedLimitInformation, &limits, sizeof(limits)))
      return fail(StartError::Job, GetLastError());
    // Construct all allocating values before creating the suspended process.
    // Only the selected executable and our random pipe name enter this command.
    std::wstring path(executable);
    std::wstring command = L"\"" + path + L"\" --ruyijs-worker ";
    command.append(name.begin(), name.end());
    if (kind == WorkerKind::Service) command += L" --ruyijs-worker-kind service";
    else if (kind == WorkerKind::Shared) command += L" --ruyijs-worker-kind shared";
    STARTUPINFOW startup{};startup.cb = sizeof(startup);
    PROCESS_INFORMATION information{};
    if (!CreateProcessW(path.c_str(), command.data(), nullptr, nullptr, FALSE,
                        CREATE_NO_WINDOW | CREATE_SUSPENDED, nullptr, nullptr, &startup, &information))
      return fail(StartError::Create, GetLastError());
    process_ = information.hProcess;
    if (!AssignProcessToJobObject(job_, process_)) {
      const auto error = GetLastError();CloseHandle(information.hThread);
      return fail(StartError::Assign, error);
    }
    const auto resumed = ResumeThread(information.hThread);
    const auto error = resumed == DWORD(-1) ? GetLastError() : 0;
    CloseHandle(information.hThread);
    if (resumed == DWORD(-1)) return fail(StartError::Resume, error);
    return {};
  } catch (...) { return fail(StartError::Allocation); }
}
}

#endif

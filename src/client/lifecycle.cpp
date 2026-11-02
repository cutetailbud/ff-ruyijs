#include "context.h"
#include <cstring>
#ifdef _WIN32
#include <bcrypt.h>
#else
#include <cerrno>
#include <cstdlib>
#include <signal.h>
#include <spawn.h>
#include <sys/random.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <sys/wait.h>
#include <unistd.h>
#include <vector>
extern char** environ;
#endif

ruyijs_context::~ruyijs_context() { Stop(); }

#ifdef _WIN32
void ruyijs_context::Stop() {
  active = false;
  if (process) {
    if (WaitForSingleObject(process, 0) == WAIT_TIMEOUT) {
      TerminateProcess(process, 1);
      WaitForSingleObject(process, 5000);
    }
    CloseHandle(process);
    process = nullptr;
  }
  if (pipe != INVALID_HANDLE_VALUE) {
    CloseHandle(pipe);
    pipe = INVALID_HANDLE_VALUE;
  }
}

void ruyijs_context::WaitProcess() {
  if (process) WaitForSingleObject(process, timeout);
}

bool ruyijs_context::Connect(const char* shell, const char* mode, std::string* error) {
  // Only the two modes the runtime accepts; the mode is never script-supplied.
  std::wstring wideMode;
  if (!mode || !strcmp(mode, "default")) wideMode.clear();
  else if (!strcmp(mode, "debug")) wideMode = L"debug";
  else { *error = "Context mode must be default or debug"; return false; }
  int count = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, shell, -1, nullptr, 0);
  if (!count) { *error = "Invalid UTF-8 executable path"; return false; }
  std::wstring executable(count, L'\0');
  MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, shell, -1, executable.data(), count);
  executable.pop_back();
  if (executable.find(L'"') != std::wstring::npos) {
    *error = "Executable path contains a quote";
    return false;
  }
  unsigned char random[16];
  if (BCryptGenRandom(nullptr, random, sizeof(random), BCRYPT_USE_SYSTEM_PREFERRED_RNG)) {
    *error = "Pipe name generation failed";
    return false;
  }
  std::wstring name = L"\\\\.\\pipe\\ruyijs-";
  for (unsigned char byte : random) {
    name += L"0123456789abcdef"[byte >> 4];
    name += L"0123456789abcdef"[byte & 15];
  }
  pipe = CreateNamedPipeW(name.c_str(), PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED |
      FILE_FLAG_FIRST_PIPE_INSTANCE, PIPE_TYPE_BYTE | PIPE_REJECT_REMOTE_CLIENTS,
      1, 65536, 65536, 0, nullptr);
  if (pipe == INVALID_HANDLE_VALUE) { *error = "Pipe creation failed"; return false; }
  // Same flags Python passes, so one runtime assembly serves both clients.
  std::wstring command = L"\"" + executable + L"\" --ruyijs-rpc " + name;
  if (!wideMode.empty()) command += L" --ruyijs-mode " + wideMode;
  STARTUPINFOW startup{};
  startup.cb = sizeof(startup);
  PROCESS_INFORMATION information{};
  // DETACHED_PROCESS, as Python starts it: no console and no conhost.exe.
  if (!CreateProcessW(executable.c_str(), command.data(), nullptr, nullptr, FALSE,
                       DETACHED_PROCESS, nullptr, nullptr, &startup, &information)) {
    *error = "Shell process creation failed: " + std::to_string(GetLastError());
    return false;
  }
  process = information.hProcess;
  CloseHandle(information.hThread);
  uint32_t ignored = 0;
  if (!Operation(false, nullptr, 0, &ignored, Now() + timeout, true)) {
    *error = "Shell pipe connection failed or timed out";
    return false;
  }
  active = true;
  return true;
}
#else
void ruyijs_context::Stop() {
  active = false;
  if (process > 0) {
    kill(process, SIGKILL);
    const uint64_t deadline = Now() + 5000;
    int status = 0;
    while (Now() < deadline) {
      const pid_t got = waitpid(process, &status, WNOHANG);
      if (got == process || (got < 0 && errno == ECHILD)) break;
      usleep(10000);
    }
    waitpid(process, &status, WNOHANG);
    process = 0;
  }
  if (pipe >= 0) {
    close(pipe);
    pipe = -1;
  }
  if (!bound_name_.empty()) {
    unlink(bound_name_.c_str());
    bound_name_.clear();
  }
}

void ruyijs_context::WaitProcess() {
  if (process <= 0) return;
  const uint64_t deadline = Now() + timeout;
  int status = 0;
  while (Now() < deadline) {
    const pid_t got = waitpid(process, &status, WNOHANG);
    if (got == process || (got < 0 && errno == ECHILD)) return;
    usleep(10000);
  }
}

bool ruyijs_context::Connect(const char* shell, const char* mode, std::string* error) {
  const char* modeArg = nullptr;
  if (!mode || !strcmp(mode, "default")) modeArg = nullptr;
  else if (!strcmp(mode, "debug")) modeArg = "debug";
  else { *error = "Context mode must be default or debug"; return false; }
  if (!shell || !*shell) { *error = "Invalid executable path"; return false; }

  unsigned char random[16];
  if (getentropy(random, sizeof(random)) != 0) {
    *error = "Pipe name generation failed";
    return false;
  }
  const char* root = std::getenv("TMPDIR");
  if (!root || !*root) root = "/tmp";
  bound_name_ = root;
  if (bound_name_.back() != '/') bound_name_.push_back('/');
  bound_name_ += "ruyijs-";
  for (unsigned char byte : random) {
    bound_name_ += "0123456789abcdef"[byte >> 4];
    bound_name_ += "0123456789abcdef"[byte & 15];
  }
  bound_name_ += ".sock";
  if (bound_name_.size() >= sizeof(sockaddr_un::sun_path)) {
    bound_name_.clear();
    *error = "Pipe path is too long";
    return false;
  }

  pipe = socket(AF_UNIX, SOCK_STREAM, 0);
  if (pipe < 0) { *error = "Pipe creation failed"; return false; }
  unlink(bound_name_.c_str());
  sockaddr_un address{};
  address.sun_family = AF_UNIX;
  memcpy(address.sun_path, bound_name_.c_str(), bound_name_.size() + 1);
  if (bind(pipe, reinterpret_cast<sockaddr*>(&address), sizeof(address)) != 0 ||
      listen(pipe, 1) != 0 || chmod(bound_name_.c_str(), 0600) != 0) {
    *error = "Pipe creation failed";
    return false;
  }

  std::string executable(shell);
  std::string rpcFlag = "--ruyijs-rpc";
  std::string modeFlag = "--ruyijs-mode";
  std::string modeValue = modeArg ? modeArg : "";
  std::vector<char*> argv;
  argv.push_back(executable.data());
  argv.push_back(rpcFlag.data());
  argv.push_back(bound_name_.data());
  if (modeArg) {
    argv.push_back(modeFlag.data());
    argv.push_back(modeValue.data());
  }
  argv.push_back(nullptr);

  posix_spawn_file_actions_t actions;
  if (posix_spawn_file_actions_init(&actions) != 0) {
    *error = "Shell process creation failed";
    return false;
  }
  const int closeListen = posix_spawn_file_actions_addclose(&actions, pipe);
  int spawned = -1;
  if (closeListen == 0) {
    spawned = posix_spawn(&process, executable.c_str(), &actions, nullptr,
                          argv.data(), environ);
  }
  posix_spawn_file_actions_destroy(&actions);
  if (closeListen != 0 || spawned != 0) {
    process = 0;
    *error = "Shell process creation failed: " + std::to_string(spawned != 0 ? spawned : closeListen);
    return false;
  }
  uint32_t ignored = 0;
  if (!Operation(false, nullptr, 0, &ignored, Now() + timeout, true)) {
    *error = "Shell pipe connection failed or timed out";
    return false;
  }
  active = true;
  return true;
}
#endif

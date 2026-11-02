#pragma once
#include "resource_network/necko_pipe.h"
#include "runtime/transport_partition.h"
#include <mutex>
#include <string>
#include <vector>

namespace ruyijs::resource_network {
#ifdef _WIN32
class NeckoHelper {
 public:
  ~NeckoHelper() {
    if (process_ && WaitForSingleObject(process_.Get(), 0) != WAIT_OBJECT_0) {
      TerminateProcess(process_.Get(), 1);
      job_.Reset();
      WaitForSingleObject(process_.Get(), 5000);
    }
  }
  // `partition` overrides the inherited environment for its helper (proxy,
  // TLS release); nullptr or no overrides keeps the process environment.
  bool Start(const std::wstring& python, NeckoPipe& input, NeckoPipe& output, const wchar_t* mode,
             const TransportPartition* partition = nullptr) {
    std::lock_guard<std::mutex> guard(lock_);
    job_.Reset(CreateJobObjectW(nullptr, nullptr));
    JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits{};
    limits.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
    if (!job_ || !SetInformationJobObject(job_.Get(), JobObjectExtendedLimitInformation,
        &limits, sizeof(limits))) return false;
    SECURITY_ATTRIBUTES inherit{sizeof(inherit), nullptr, TRUE};
    NeckoHandle errors(CreateFileW(L"NUL", GENERIC_WRITE, FILE_SHARE_WRITE | FILE_SHARE_READ,
        &inherit, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr));
    if (!errors) return false;
    HANDLE inherited[] = {input.Child(), output.Child(), errors.Get()};
    SIZE_T size = 0;
    InitializeProcThreadAttributeList(nullptr, 1, 0, &size);
    std::vector<unsigned char> storage(size);
    auto* attributes = reinterpret_cast<LPPROC_THREAD_ATTRIBUTE_LIST>(storage.data());
    if (!InitializeProcThreadAttributeList(attributes, 1, 0, &size)) return false;
    const BOOL restricted = UpdateProcThreadAttribute(attributes, 0,
        PROC_THREAD_ATTRIBUTE_HANDLE_LIST, inherited, sizeof(inherited), nullptr, nullptr);
    STARTUPINFOEXW startup{};
    startup.StartupInfo.cb = sizeof(startup);
    startup.StartupInfo.dwFlags = STARTF_USESTDHANDLES;
    startup.StartupInfo.hStdInput = input.Child();
    startup.StartupInfo.hStdOutput = output.Child();
    startup.StartupInfo.hStdError = errors.Get();
    startup.lpAttributeList = attributes;
    std::wstring command = L"\"" + python + L"\" -I -B -m ruyijs_necko.oneshot";
    if (mode && *mode) { command += L" "; command += mode; }
    PROCESS_INFORMATION process{};
    std::wstring environment;
    if (partition && Overrides(*partition)) environment = EnvironmentBlock(*partition);
    // DETACHED_PROCESS: no console, so no conhost.exe per helper (about
    // 6 MiB each); the three standard handles are the pipes and NUL above.
    const BOOL created = restricted && CreateProcessW(python.c_str(), command.data(), nullptr,
        nullptr, TRUE, DETACHED_PROCESS | CREATE_SUSPENDED | EXTENDED_STARTUPINFO_PRESENT |
            (environment.empty() ? 0 : CREATE_UNICODE_ENVIRONMENT),
        environment.empty() ? nullptr : environment.data(), nullptr, &startup.StartupInfo, &process);
    DeleteProcThreadAttributeList(attributes);
    if (!created) return false;
    process_.Reset(process.hProcess);
    NeckoHandle thread(process.hThread);
    return AssignProcessToJobObject(job_.Get(), process_.Get()) &&
        ResumeThread(thread.Get()) != DWORD(-1);
  }
  void Cancel() {
    std::lock_guard<std::mutex> guard(lock_);
    if (process_) TerminateProcess(process_.Get(), ERROR_OPERATION_ABORTED);
  }
  bool Finish(ULONGLONG deadline) {
    DWORD status = 1;
    return WaitForSingleObject(process_.Get(), NeckoRemaining(deadline)) == WAIT_OBJECT_0 &&
        GetExitCodeProcess(process_.Get(), &status) && status == 0;
  }
 private:
  std::mutex lock_;
  NeckoHandle process_, job_;
};
#else
#include <sys/types.h>
class NeckoHelper {
 public:
  NeckoHelper() = default;
  ~NeckoHelper();
  NeckoHelper(const NeckoHelper&) = delete;
  NeckoHelper& operator=(const NeckoHelper&) = delete;
  bool Start(const std::wstring& python, NeckoPipe& input, NeckoPipe& output, const wchar_t* mode,
             const TransportPartition* partition = nullptr);
  void Cancel();
  bool Finish(ULONGLONG deadline);
 private:
  void KillLocked();
  std::mutex lock_;
  pid_t pid_ = 0;
};
#endif
bool NeckoPythonPath(std::wstring*);
}

#include "context.h"
#include <algorithm>
#include <cstdlib>
#include <cstring>
#ifndef _WIN32
#include <cerrno>
#include <fcntl.h>
#include <poll.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>
#endif

uint64_t ruyijs_context::Now() const {
#ifdef _WIN32
  return GetTickCount64();
#else
  timespec ts{};
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return static_cast<uint64_t>(ts.tv_sec) * 1000u +
         static_cast<uint64_t>(ts.tv_nsec / 1000000);
#endif
}

bool ruyijs_context::Operation(bool writing, void* data, uint32_t length,
                               uint32_t* count, uint64_t deadline, bool connecting) {
#ifdef _WIN32
  DWORD transferred = 0;
  OVERLAPPED operation{};
  operation.hEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
  if (!operation.hEvent) return false;
  BOOL ok = connecting ? ConnectNamedPipe(pipe, &operation)
            : writing ? WriteFile(pipe, data, length, &transferred, &operation)
                      : ReadFile(pipe, data, length, &transferred, &operation);
  if (!ok) {
    DWORD error = GetLastError();
    if (connecting && error == ERROR_PIPE_CONNECTED) {
      ok = TRUE;
    } else if (error == ERROR_IO_PENDING) {
      ULONGLONG now = Now();
      DWORD remaining = now >= deadline ? 0 :
        static_cast<DWORD>(std::min<ULONGLONG>(deadline - now, MAXDWORD - 1));
      if (WaitForSingleObject(operation.hEvent, remaining) == WAIT_OBJECT_0) {
        ok = GetOverlappedResult(pipe, &operation, &transferred, FALSE);
      } else {
        CancelIoEx(pipe, &operation);
        GetOverlappedResult(pipe, &operation, &transferred, TRUE);
        ok = FALSE;
      }
    }
  }
  CloseHandle(operation.hEvent);
  if (count) *count = transferred;
  return ok != FALSE;
#else
  if (pipe < 0) return false;
  while (true) {
    const uint64_t current = Now();
    if (current >= deadline) return false;
    pollfd pfd{};
    pfd.fd = pipe;
    pfd.events = connecting || !writing ? POLLIN : POLLOUT;
    const int status = poll(&pfd, 1,
        static_cast<int>(std::min<uint64_t>(deadline - current, 0x7fffffff)));
    if (status == 0) return false;
    if (status < 0) {
      if (errno == EINTR) continue;
      return false;
    }
    if (connecting) {
      const int accepted = accept(pipe, nullptr, nullptr);
      if (accepted < 0) {
        if (errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK) continue;
        return false;
      }
      close(pipe);
      pipe = accepted;
#ifdef SO_NOSIGPIPE
      int one = 1;
      setsockopt(pipe, SOL_SOCKET, SO_NOSIGPIPE, &one, sizeof(one));
#endif
      const int flags = fcntl(pipe, F_GETFL, 0);
      if (flags < 0 || fcntl(pipe, F_SETFL, flags | O_NONBLOCK) != 0) return false;
      if (!bound_name_.empty()) {
        unlink(bound_name_.c_str());
        bound_name_.clear();
      }
      if (count) *count = 0;
      return true;
    }
    const ssize_t transferred = writing
        ? send(pipe, data, length,
#ifdef MSG_NOSIGNAL
               MSG_NOSIGNAL
#else
               0
#endif
          )
        : recv(pipe, data, length, 0);
    if (transferred < 0) {
      if (errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK) continue;
      return false;
    }
    if (count) *count = static_cast<uint32_t>(transferred);
    return transferred > 0;
  }
#endif
}

bool ruyijs_context::Transfer(bool writing, void* data, uint32_t length,
                              uint64_t deadline) {
  auto* bytes = static_cast<char*>(data);
  while (length) {
    if (Now() >= deadline) return false;
    uint32_t count = 0;
    if (!Operation(writing, bytes, std::min<uint32_t>(length, 65536), &count,
                   deadline) || count == 0) return false;
    bytes += count;
    length -= count;
  }
  return true;
}

bool ruyijs_context::Exchange(const char* data, uint32_t length,
                              std::string* response) {
  const uint64_t deadline = Now() + timeout;
  if (!WriteFrame(data, length, deadline)) return false;
  while (ReadFrame(response, deadline)) {
    constexpr char marker[] = "{\"event\":\"callback\",";
    if (response->compare(0, sizeof(marker) - 1, marker) != 0) return true;
    if (!callback) return false;
    char* returned = nullptr;
    uint32_t returned_length = 0;
    callback_active = true;
    int status;
    try {
      status = callback(response->data(), static_cast<uint32_t>(response->size()),
                        &returned, &returned_length, callback_user);
    } catch (...) {
      callback_active = false;
      free(returned);
      return false;
    }
    callback_active = false;
    bool ok = status == 0 && returned && returned_length &&
              returned_length <= kRuyijsMaxFrame &&
              WriteFrame(returned, returned_length, deadline);
    free(returned);
    if (!ok) return false;
  }
  return false;
}

bool ruyijs_context::WriteFrame(const char* data, uint32_t length,
                                uint64_t deadline) {
  if (!Transfer(true, &length, sizeof(length), deadline) ||
      !Transfer(true, const_cast<char*>(data), length, deadline)) return false;
  return true;
}

bool ruyijs_context::ReadFrame(std::string* response, uint64_t deadline) {
  uint32_t received = 0;
  if (!Transfer(false, &received, sizeof(received), deadline) || received == 0 ||
      received > kRuyijsMaxFrame) return false;
  response->resize(received);
  return Transfer(false, response->data(), received, deadline);
}

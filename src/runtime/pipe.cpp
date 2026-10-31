#include "runtime/pipe.h"
#include "runtime/pipe_async.h"
#include <cstdint>
#include <cstring>

namespace ruyijs {
Pipe::~Pipe() { Close(); }

namespace {
bool HexToken(const char* text, size_t length) {
  if (length < 16 || length > 64) return false;
  for (size_t i = 0; i < length; ++i) {
    const char c = text[i];
    if (!((c >= 'a' && c <= 'f') || (c >= '0' && c <= '9') || c == '-')) return false;
  }
  return true;
}
#ifdef _WIN32
bool ValidName(const char* name) {
  constexpr char prefix[] = "\\\\.\\pipe\\ruyijs-";
  if (strncmp(name, prefix, sizeof(prefix) - 1) != 0) return false;
  return HexToken(name + sizeof(prefix) - 1, strlen(name + sizeof(prefix) - 1));
}
#else
bool ValidName(const char* name) {
  if (!name || !name[0] || strstr(name, "..")) return false;
  const char* base = strrchr(name, '/');
  base = base ? base + 1 : name;
  constexpr char prefix[] = "ruyijs-";
  if (strncmp(base, prefix, sizeof(prefix) - 1) != 0) return false;
  const char* rest = base + sizeof(prefix) - 1;
  const char* dot = strrchr(rest, '.');
  if (!dot || strcmp(dot, ".sock") != 0) return false;
  if (!HexToken(rest, static_cast<size_t>(dot - rest))) return false;
  return strlen(name) < sizeof(sockaddr_un::sun_path);
}
#endif
}  // namespace

bool Pipe::Connect(const char* name) { return OpenClient(name, false); }
bool Pipe::Listen(const char* name) { return OpenServer(name, false); }

#ifdef _WIN32
bool Pipe::OpenClient(const char* name, bool asynchronous) {
  if (handle_ || !name || !ValidName(name)) return false;
  HANDLE handle = CreateFileA(name, GENERIC_READ | GENERIC_WRITE, 0, nullptr,
                              OPEN_EXISTING, asynchronous ? FILE_FLAG_OVERLAPPED : 0, nullptr);
  if (handle == INVALID_HANDLE_VALUE) return false;
  handle_ = handle;
  return true;
}

bool Pipe::OpenServer(const char* name, bool asynchronous) {
  if (handle_ || !name || !ValidName(name)) return false;
  HANDLE handle = CreateNamedPipeA(name, PIPE_ACCESS_DUPLEX | (asynchronous ? FILE_FLAG_OVERLAPPED : 0),
      PIPE_TYPE_BYTE | PIPE_REJECT_REMOTE_CLIENTS, 1, 65536, 65536, 0, nullptr);
  if (handle == INVALID_HANDLE_VALUE) return false;
  handle_ = handle;
  return true;
}

bool Pipe::Accept() {
  if (!handle_ || async_) return false;
  if (!ConnectNamedPipe(handle_, nullptr) && GetLastError() != ERROR_PIPE_CONNECTED)
    return false;
  return true;
}

bool Pipe::Transfer(void* data, unsigned length, bool writing) {
  if (!handle_ || async_) return false;
  auto* bytes = static_cast<char*>(data);
  while (length) {
    DWORD count = 0;
    BOOL ok = writing ? WriteFile(handle_, bytes, length, &count, nullptr)
                      : ReadFile(handle_, bytes, length, &count, nullptr);
    if (!ok || !count) return false;
    bytes += count;
    length -= count;
  }
  return true;
}
#else
#include <sys/stat.h>
namespace {
bool SetNonblock(int fd) {
  const int flags = fcntl(fd, F_GETFL, 0);
  return flags >= 0 && fcntl(fd, F_SETFL, flags | O_NONBLOCK) == 0;
}
void IgnoreSigPipe(int fd) {
#ifdef SO_NOSIGPIPE
  int one = 1;
  setsockopt(fd, SOL_SOCKET, SO_NOSIGPIPE, &one, sizeof(one));
#else
  (void)fd;
#endif
}
}  // namespace

bool Pipe::OpenClient(const char* name, bool asynchronous) {
  if (handle_ || !name || !ValidName(name)) return false;
  const int fd = socket(AF_UNIX, SOCK_STREAM, 0);
  if (fd < 0) return false;
  sockaddr_un address{};
  address.sun_family = AF_UNIX;
  strncpy(address.sun_path, name, sizeof(address.sun_path) - 1);
  if (connect(fd, reinterpret_cast<sockaddr*>(&address), sizeof(address)) != 0) {
    close(fd);
    return false;
  }
  IgnoreSigPipe(fd);
  if (asynchronous && !SetNonblock(fd)) {
    close(fd);
    return false;
  }
  handle_ = PipeFromFd(fd);
  return true;
}

bool Pipe::OpenServer(const char* name, bool asynchronous) {
  if (handle_ || !name || !ValidName(name)) return false;
  const int fd = socket(AF_UNIX, SOCK_STREAM, 0);
  if (fd < 0) return false;
  unlink(name);
  sockaddr_un address{};
  address.sun_family = AF_UNIX;
  strncpy(address.sun_path, name, sizeof(address.sun_path) - 1);
  if (bind(fd, reinterpret_cast<sockaddr*>(&address), sizeof(address)) != 0 ||
      listen(fd, 1) != 0 || chmod(name, 0600) != 0) {
    close(fd);
    unlink(name);
    return false;
  }
  IgnoreSigPipe(fd);
  if (asynchronous && !SetNonblock(fd)) {
    close(fd);
    unlink(name);
    return false;
  }
  bound_name_ = name;
  handle_ = PipeFromFd(fd);
  return true;
}

bool Pipe::Accept() {
  if (!handle_ || async_) return false;
  const int fd = PipeFd(handle_);
  const int conn = accept(fd, nullptr, nullptr);
  if (conn < 0) return false;
  close(fd);
  IgnoreSigPipe(conn);
  handle_ = PipeFromFd(conn);
  return true;
}

bool Pipe::Transfer(void* data, unsigned length, bool writing) {
  if (!handle_ || async_) return false;
  const int fd = PipeFd(handle_);
  auto* bytes = static_cast<char*>(data);
  while (length) {
    const ssize_t count = writing ? send(fd, bytes, length, kPipeSendFlags) : recv(fd, bytes, length, 0);
    if (count <= 0) return false;
    bytes += count;
    length -= static_cast<unsigned>(count);
  }
  return true;
}
#endif

bool Pipe::Read(std::string* frame) {
  uint32_t length = 0;
  if (!Transfer(&length, sizeof(length), false) || !ValidFrameSize(length))
    return false;
  frame->resize(length);
  return Transfer(frame->data(), length, false);
}

bool Pipe::Write(const std::string& frame) {
  if (!ValidFrameSize(frame.size())) return false;
  uint32_t length = static_cast<uint32_t>(frame.size());
  return Transfer(&length, sizeof(length), true) &&
         Transfer(const_cast<char*>(frame.data()), length, true);
}
}

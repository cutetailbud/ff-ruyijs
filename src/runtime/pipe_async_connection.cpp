#include "runtime/pipe_async.h"

#ifdef _WIN32
namespace ruyijs {
PipeResult PipeError(DWORD error) {
  return error == ERROR_BROKEN_PIPE || error == ERROR_NO_DATA ||
      error == ERROR_PIPE_NOT_CONNECTED || error == ERROR_OPERATION_ABORTED
      ? PipeResult::Closed : PipeResult::Failed;
}
void CancelPipeOperation(HANDLE pipe, PipeOperation& operation) {
  if (!operation.pending) return;
  DWORD ignored = 0;
  GetOverlappedResult(pipe, &operation.value, &ignored, TRUE);
  operation.pending = false;
}
bool Pipe::ListenAsync(const char* name) {
  if (async_ || !OpenServer(name, true)) return false;
  async_ = NewPipeAsync();
  if (!async_) { Close(); return false; }
  async_->server = true;
  return true;
}
bool Pipe::ConnectAsync(const char* name) {
  if (async_ || !OpenClient(name, true)) return false;
  async_ = NewPipeAsync();
  if (!async_) { Close(); return false; }
  async_->connected = true;
  return true;
}
PipeResult PipeAccept(HANDLE pipe, PipeAsync& state) {
  if (state.connected) return PipeResult::Ready;
  auto& op = state.accept;
  if (!op.pending) {
    if (ConnectNamedPipe(pipe, &op.value)) { state.connected = true; return PipeResult::Ready; }
    const auto error = GetLastError();
    if (error == ERROR_PIPE_CONNECTED) { state.connected = true; return PipeResult::Ready; }
    if (error != ERROR_IO_PENDING) return PipeError(error);
    op.pending = true;
  }
  DWORD transferred = 0;
  if (!GetOverlappedResult(pipe, &op.value, &transferred, FALSE)) {
    const auto error = GetLastError();
    if (error == ERROR_IO_INCOMPLETE) return PipeResult::Pending;
    op.pending = false;
    return PipeError(error);
  }
  op.pending = false; state.connected = true;
  return PipeResult::Ready;
}
PipeResult PipeIo(HANDLE pipe, PipeOperation& op, bool writing,
                    void* bytes, DWORD size, DWORD* transferred) {
  *transferred = 0;
  if (!op.pending) {
    const HANDLE event = op.value.hEvent;
    op.value = {}; op.value.hEvent = event;
    ResetEvent(event);
    const bool done = writing ? WriteFile(pipe, bytes, size, nullptr, &op.value)
                              : ReadFile(pipe, bytes, size, nullptr, &op.value);
    if (!done && GetLastError() != ERROR_IO_PENDING) return PipeError(GetLastError());
    op.pending = true;
  }
  if (!GetOverlappedResult(pipe, &op.value, transferred, FALSE)) {
    const auto error = GetLastError();
    if (error == ERROR_IO_INCOMPLETE) return PipeResult::Pending;
    op.pending = false;
    return PipeError(error);
  }
  op.pending = false;
  return *transferred ? PipeResult::Ready : PipeResult::Closed;
}
}
#else
#include <cerrno>
#include <poll.h>
namespace ruyijs {
void CancelPipeOperation(void*, PipeOperation& operation) { operation.pending = false; }
bool Pipe::ListenAsync(const char* name) {
  if (async_ || !OpenServer(name, true)) return false;
  async_ = NewPipeAsync();
  if (!async_) { Close(); return false; }
  async_->server = true;
  return true;
}
bool Pipe::ConnectAsync(const char* name) {
  if (async_ || !OpenClient(name, true)) return false;
  async_ = NewPipeAsync();
  if (!async_) { Close(); return false; }
  async_->connected = true;
  return true;
}
PipeResult PipeAccept(void*& pipe, PipeAsync& state) {
  if (state.connected) return PipeResult::Ready;
  const int fd = PipeFd(pipe);
  pollfd ready{fd, POLLIN, 0};
  const int waited = poll(&ready, 1, 0);
  if (waited == 0) return PipeResult::Pending;
  if (waited < 0) return PipeResult::Failed;
  const int conn = accept(fd, nullptr, nullptr);
  if (conn < 0) {
    if (errno == EAGAIN || errno == EWOULDBLOCK) return PipeResult::Pending;
    return PipeResult::Failed;
  }
  close(fd);
  const int flags = fcntl(conn, F_GETFL, 0);
  if (flags < 0 || fcntl(conn, F_SETFL, flags | O_NONBLOCK) != 0) {
    close(conn);
    return PipeResult::Failed;
  }
#ifdef SO_NOSIGPIPE
  int one = 1;
  setsockopt(conn, SOL_SOCKET, SO_NOSIGPIPE, &one, sizeof(one));
#endif
  pipe = PipeFromFd(conn);
  state.connected = true;
  return PipeResult::Ready;
}
PipeResult PipeIo(void* pipe, PipeOperation& op, bool writing,
                    void* bytes, unsigned size, unsigned* transferred) {
  *transferred = 0;
  const int fd = PipeFd(pipe);
  const ssize_t count = writing ? send(fd, bytes, size, kPipeSendFlags) : recv(fd, bytes, size, 0);
  op.pending = false;
  if (count < 0) {
    if (errno == EAGAIN || errno == EWOULDBLOCK) {
      op.pending = true;
      return PipeResult::Pending;
    }
    return errno == EPIPE || errno == ECONNRESET ? PipeResult::Closed : PipeResult::Failed;
  }
  if (!count) return PipeResult::Closed;
  *transferred = static_cast<unsigned>(count);
  return PipeResult::Ready;
}
}
#endif

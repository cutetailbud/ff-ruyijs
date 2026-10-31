#include "runtime/pipe.h"
#include <chrono>
#include <cstring>
#include <thread>

int main(int argc, char** argv) {
  if (argc != 4) return 2;
  const bool async = !std::strcmp(argv[1], "async");
  const bool server = !std::strcmp(argv[2], "listen");
  ruyijs::Pipe pipe;
  if (!(server ? (async ? pipe.ListenAsync(argv[3]) : pipe.Listen(argv[3]))
               : (async ? pipe.ConnectAsync(argv[3]) : pipe.Connect(argv[3])))) return 3;
  if (server && !async && !pipe.Accept()) return 4;
  const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
  while (std::chrono::steady_clock::now() < deadline) {
    std::string frame;
    if (!async) {
      if (!pipe.Read(&frame)) return 0;
      if (!pipe.Write(frame)) return 5;
      continue;
    }
    bool received = false;
    const auto result = pipe.Poll(&frame, &received);
    if (received && !pipe.Queue(std::move(frame))) return 6;
    if (result == ruyijs::PipeResult::Closed) return 0;
    if (result == ruyijs::PipeResult::Failed) return 7;
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  return 8;
}

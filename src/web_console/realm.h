#ifndef RUYIJS_WEB_CONSOLE_REALM_H
#define RUYIJS_WEB_CONSOLE_REALM_H
#include <cstddef>
#include <cstdint>
#include <deque>
#include <memory>
#include <string>
#include <vector>
#include "js/RootingAPI.h"
struct JSContext;
namespace ruyijs { class PageHelpers; }
namespace ruyijs::web_console {
struct State;

// One recorded console call. The owner renders the arguments to text at call
// time and keeps no live value, so the host reads exactly what the page wrote
// and nothing is re-evaluated later.
struct Record {
  uint32_t realm = 0;              // Window realm that called, in creation order
  std::string method;              // the WebIDL member name, e.g. "assert"
  std::string level;               // devtools severity: log/info/debug/warn/error/trace
  std::string text;                // the rendered arguments (bounded)
  std::vector<std::string> stack;  // `trace()` frames, innermost first (bounded)
  int indent = 0;                  // console.group() nesting when the call was made
};

// The Context's one bounded, ordered log. This is the domain state source: the
// host reads it through one port, and a realm-scoped `clear()` drops only that
// realm's records, so one store serves every Window without a second owner.
class Log final {
 public:
  static constexpr size_t kCapacity = 1024;
  static constexpr size_t kTextLimit = 4096;
  static constexpr size_t kStackLimit = 16;
  // A fresh realm id, in creation order. Ids are never recycled, so a record
  // stays attributable after its Window is retired.
  uint32_t NewRealm() { return realms_++; }
  void Append(Record record);
  void ClearRealm(uint32_t realm);
  void ClearAll() { records_.clear(); }
  const std::deque<Record>& records() const { return records_; }

 private:
  std::deque<Record> records_;
  uint32_t realms_ = 0;
};

// Console owner for one Window realm. The main page creates the Context's one
// Log; an iframe child shares it (one ordered host stream) and takes the next
// realm id, while each realm installs its own `window.console` namespace object.
class Realm final {
 public:
  // Reads the Context's single clock owner (execution_scheduling) in seconds.
  // Bound by the shared coordinator once that owner exists; a realm without it
  // reports 0 instead of opening a second time source.
  using ClockPort = double (*)(void* context);
  // |shared| is the parent realm's log for an iframe child, null for the main
  // page, which then owns the Context's one log.
  Realm(JSContext* cx, JS::HandleObject page, PageHelpers& helpers, Log* shared);
  ~Realm();
  Realm(const Realm&) = delete;
  Realm& operator=(const Realm&) = delete;
  bool Install();
  void SetClockPort(ClockPort clock, void* context);
  void CopyClockFrom(const Realm& other);
  Log& log() { return *log_; }
  // Appends a record the runtime itself reports for this Window (the
  // document-start script's failure, issue #53), stamped with this realm's id
  // so it reads like the page's own records; no page code runs.
  void Report(Record record);

 private:
  std::unique_ptr<State> state_;
  Log* log_ = nullptr;
};
}
#endif

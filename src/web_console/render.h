#ifndef RUYIJS_WEB_CONSOLE_RENDER_H
#define RUYIJS_WEB_CONSOLE_RENDER_H
#include <string>
#include <vector>
#include "js/CallAndConstruct.h"
#include "js/RootingAPI.h"
#include "js/TypeDecls.h"

namespace ruyijs::web_console {
// Argument rendering shared by the console members. Every entry point is
// bounded and never leaves a pending exception behind, so a page cannot break
// the console by logging a value whose conversion throws.
std::string Render(JSContext* cx, JS::HandleValue value);
// WebIDL DOMString conversion (count/time labels): ToString, as Firefox does.
std::string LabelText(JSContext* cx, JS::HandleValue value);
std::string Join(JSContext* cx, const JS::CallArgs& args, unsigned first);
std::string Label(JSContext* cx, const JS::CallArgs& args, const char* fallback);
// The donor devtools frontend strings for the stateful members, including the
// typographic quotes and the `Math.round(duration * 100) / 100` duration form.
std::string Counted(const std::string& label, int count);
std::string CounterMissing(const std::string& label);
std::string TimerExists(const std::string& label);
std::string TimerMissing(const std::string& label);
std::string Timed(const std::string& label, double duration, bool ended);
// The `trace()` stack, innermost frame first, at most Log::kStackLimit frames.
std::vector<std::string> Capture(JSContext* cx);
// The saved stack of the first Error among `args`: where that Error was
// constructed, read from the engine's Error object, so no page code runs.
// Empty when nothing logged is an Error or it carries no stack.
std::vector<std::string> ArgumentStack(JSContext* cx, const JS::CallArgs& args);
}
#endif

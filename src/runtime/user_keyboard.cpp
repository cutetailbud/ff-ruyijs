// Keyboard part of host user input (ARCHITECTURE.md, host user input): per
// character keydown -> keypress -> beforeinput -> input -> keyup with NumLock
// set, as Firefox 158 records them; named keys and their default actions.
#include "runtime/user_input_internal.h"
#include "runtime/resource_schema.h"
#include "document_model/realm.h"
#include <cstring>

namespace ruyijs::user_input {
namespace {
struct Key { std::u16string key, code; uint32_t keyCode = 0; bool shift = false; };
// Firefox's US-layout key for one character (WidgetKeyboardEvent key codes).
Key KeyFor(char16_t c) {
  static const char16_t* plain = u"-=;',./`[]\\";
  static const char16_t* shifted = u"_+:\"<>?~{}|";
  static const char* codes[] = {"Minus", "Equal", "Semicolon", "Quote", "Comma", "Period",
                                "Slash", "Backquote", "BracketLeft", "BracketRight", "Backslash"};
  static const uint32_t keyCodes[] = {173, 61, 59, 222, 188, 190, 191, 192, 219, 221, 220};
  static const char16_t* digitShift = u")!@#$%^&*(";
  Key k{std::u16string(1, c), u"", 0, false};
  auto ascii = [](const char* text) { return std::u16string(text, text + strlen(text)); };
  if (c >= u'a' && c <= u'z') { k.code = u"Key" + std::u16string(1, char16_t(c - 32)); k.keyCode = c - 32; }
  else if (c >= u'A' && c <= u'Z') { k.code = u"Key" + std::u16string(1, c); k.keyCode = c; k.shift = true; }
  else if (c >= u'0' && c <= u'9') { k.code = u"Digit" + std::u16string(1, c); k.keyCode = c; }
  else if (c == u' ') { k.code = u"Space"; k.keyCode = 32; }
  for (int i = 0; i < 10; ++i)
    if (digitShift[i] == c) { k.code = u"Digit" + std::u16string(1, char16_t(u'0' + i)); k.keyCode = u'0' + i; k.shift = true; }
  for (int i = 0; i < 11; ++i) {
    if (plain[i] == c) { k.code = ascii(codes[i]); k.keyCode = keyCodes[i]; }
    if (shifted[i] == c) { k.code = ascii(codes[i]); k.keyCode = keyCodes[i]; k.shift = true; }
  }
  return k;
}
Init& KeyFields(Init& init, const Key& k) {
  return init.Text("key", k.key).Text("code", k.code).Flag("shiftKey", k.shift)
      .Flag("modifierNumLock", true).Flag("bubbles", true).Flag("cancelable", true).Flag("composed", true);
}
}  // namespace
bool Character(JSContext* cx, JS::HandleObject target, char16_t c, SchedulingControl& clock) {
  const Key k = KeyFor(c);
  bool down = true, press = true, before = true, inserted = false;
  Init keydown(cx);
  if (!KeyFields(keydown, k).Number("keyCode", k.keyCode).Number("charCode", 0)
          .Number("which", k.keyCode).Send(target, "keydown", clock, &down)) return false;
  if (down) {
    Init keypress(cx);
    if (!KeyFields(keypress, k).Number("keyCode", 0).Number("charCode", c).Number("which", c)
            .Send(target, "keypress", clock, &press)) return false;
  }
  if (down && press) {
    const std::u16string data(1, c);
    Init beforeinput(cx);
    if (!beforeinput.Text("inputType", u"insertText").Text("data", data).Flag("bubbles", true)
            .Flag("cancelable", true).Flag("composed", true).Send(target, "beforeinput", clock, &before))
      return false;
    if (before && !document_model::InsertText(cx, target, data, &inserted)) return false;
    Init input(cx);
    if (inserted && !input.Text("inputType", u"insertText").Text("data", data).Flag("bubbles", true)
            .Flag("cancelable", false).Flag("composed", true).Send(target, "input", clock)) return false;
  }
  Init keyup(cx);
  return KeyFields(keyup, k).Number("keyCode", k.keyCode).Number("charCode", 0)
      .Number("which", k.keyCode).Send(target, "keyup", clock);
}
// A named key: Enter (keypress, then line break or implicit submission),
// Backspace (deleteContentBackward), Tab, Escape and the arrow keys.
bool Press(JSContext* cx, JS::HandleObject target, const std::string& name, SchedulingControl& clock) {
  static const struct { const char* name; uint32_t code; } keys[] = {
      {"Enter", 13}, {"Backspace", 8}, {"Tab", 9}, {"Escape", 27}, {"ArrowLeft", 37},
      {"ArrowUp", 38}, {"ArrowRight", 39}, {"ArrowDown", 40}};
  uint32_t code = 0;
  for (const auto& key : keys) if (name == key.name) code = key.code;
  if (!code) return resource_wire::Error(cx, "Unsupported key name");
  Key k{std::u16string(name.begin(), name.end()), std::u16string(name.begin(), name.end()), code, false};
  bool down = true, press = true, before = true, changed = false;
  Init keydown(cx);
  if (!KeyFields(keydown, k).Number("keyCode", code).Number("charCode", 0).Number("which", code)
          .Send(target, "keydown", clock, &down)) return false;
  // Firefox dispatches keypress to content for Enter only among these keys.
  if (down && code == 13) {
    Init keypress(cx);
    if (!KeyFields(keypress, k).Number("keyCode", 13).Number("charCode", 0).Number("which", 13)
            .Send(target, "keypress", clock, &press)) return false;
  }
  if (down && press && (code == 13 || code == 8)) {
    const bool lineBreak = code == 13;
    std::u16string inputType = u"insertLineBreak";
    bool multiline = false;
    if (lineBreak && !document_model::Multiline(cx, target, &multiline)) return false;
    if (lineBreak && !multiline) {
      // Enter in a single-line control submits its form (no beforeinput).
      if (!document_model::ImplicitSubmit(cx, target, clock.ClockMilliseconds())) return false;
    } else if (lineBreak) {
      // A textarea inserts a line break; an editing host splits the paragraph.
      bool inserted = false, editing = false;
      if (!document_model::Editing(cx, target, &editing)) return false;
      if (editing) inputType = u"insertParagraph";
      Init probe(cx);
      if (!probe.Text("inputType", inputType).Flag("bubbles", true).Flag("cancelable", true)
              .Flag("composed", true).Send(target, "beforeinput", clock, &before)) return false;
      if (before && !document_model::InsertText(cx, target, u"\n", &inserted)) return false;
      changed = inserted;
    } else {
      Init probe(cx);
      if (!probe.Text("inputType", u"deleteContentBackward").Flag("bubbles", true).Flag("cancelable", true)
              .Flag("composed", true).Send(target, "beforeinput", clock, &before)) return false;
      if (before && !document_model::DeleteBackward(cx, target, &changed)) return false;
    }
    Init input(cx);
    if (changed && !input.Text("inputType", lineBreak ? inputType : std::u16string(u"deleteContentBackward"))
            .Flag("bubbles", true).Flag("cancelable", false).Flag("composed", true).Send(target, "input", clock))
      return false;
  }
  Init keyup(cx);
  return KeyFields(keyup, k).Number("keyCode", code).Number("charCode", 0).Number("which", code)
      .Send(target, "keyup", clock);
}
}  // namespace ruyijs::user_input

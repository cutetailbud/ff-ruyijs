// Shared helpers for the Styling / CSSOM owner: ASCII conversion, the
// declaration-block codec every read-only view uses, and the lazy named
// accessors that expose CSS properties as JS properties.
#include "web_styling/internal.h"
#include "domain/styling/css_declarations.h"
#include "domain/styling/css_font_value.h"
#include "domain/styling/css_box_value.h"

#include <cstring>
#include "js/GCVector.h"
#include "js/PropertyAndElement.h"
#include "js/String.h"
#include "jsfriendapi.h"

namespace ruyijs::web_styling::detail {
bool AsciiEquals(JSContext* cx, JS::HandleString text, const char* ascii, bool* match) {
  return JS_StringEqualsAscii(cx, text, ascii, match);
}
bool AsciiEqualsCI(JSContext* cx, JS::HandleString text, const char* ascii, bool* match) {
  JS::UniqueChars utf8 = JS_EncodeStringToUTF8(cx, text);
  if (!utf8) return false;
  size_t length = std::strlen(ascii);
  *match = std::strlen(utf8.get()) == length;
  for (size_t index = 0; *match && index < length; ++index) {
    char a = utf8.get()[index], b = ascii[index];
    if (a >= 'A' && a <= 'Z') a = char(a + 32);
    if (b >= 'A' && b <= 'Z') b = char(b + 32);
    *match = a == b;
  }
  return true;
}
bool StringToAscii(JSContext* cx, JS::HandleString text, std::string* out) {
  JS::UniqueChars utf8 = JS_EncodeStringToUTF8(cx, text);
  if (!utf8) return false;
  out->assign(utf8.get());
  return true;
}
bool ToAscii(JSContext* cx, JS::HandleValue value, std::string* out) {
  JS::RootedString text(cx, JS::ToString(cx, value));
  if (!text) return false;
  return StringToAscii(cx, text, out);
}
std::string Trim(const std::string& text) {
  size_t begin = 0, end = text.size();
  while (begin < end && static_cast<unsigned char>(text[begin]) <= ' ') ++begin;
  while (end > begin && static_cast<unsigned char>(text[end - 1]) <= ' ') --end;
  return text.substr(begin, end - begin);
}
std::string Lower(const std::string& text) {
  std::string out = text;
  for (char& c : out)
    if (c >= 'A' && c <= 'Z') c = char(c + 32);
  return out;
}
std::string Dashed(const std::string& idl_name) {
  std::string out;
  for (char c : idl_name) {
    if (c >= 'A' && c <= 'Z') { out.push_back('-'); c = char(c + 32); }
    out.push_back(c);
  }
  return out;
}
bool Lookup(const std::string& block, const std::string& name, std::string* value,
            bool* important) {
  namespace css = ruyijs::domain::styling;
  const auto wanted = Lower(name);
  const auto& parsed = css::CssDeclarationsCached(block);
  *value = css::CssLookupDeclaration(parsed, wanted, important);
  if (!value->empty() || css::CssShorthandName(wanted)) return !value->empty();
  if (!parsed.index.empty()) return parsed.index.count(wanted) != 0;
  // A custom property may hold the empty value.
  return std::any_of(parsed.rows.begin(), parsed.rows.end(), [&](const auto& row) { return row.name == wanted; });
}
size_t CountProperties(const std::string& block) {
  return ruyijs::domain::styling::CssDeclarationsCached(block).rows.size();
}
bool PropertyNameAt(const std::string& block, size_t index, std::string* name) {
  const auto& rows = ruyijs::domain::styling::CssDeclarationsCached(block).rows;
  if (index < rows.size()) { *name = rows[index].name; return true; }
  name->clear();
  return false;
}
std::string Serialize(const std::string& name, const std::string& value, bool important) {
  std::string out = name + ": " + value;
  if (important) out += " !important";
  out += ";";
  return out;
}

}  // namespace ruyijs::web_styling::detail

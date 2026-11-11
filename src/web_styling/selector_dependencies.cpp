// What a selector reads beyond its subject's own attributes, for Web
// Geometry's restyle reuse (computed_source.h SelectorDependencies): style
// attribute selectors, structural pseudo-classes counted from the end or over
// children, and the keys read inside :has() arguments.
#include "web_styling/computed_source.h"

namespace ruyijs::web_styling {
namespace {
bool IdentChar(char c) {
  return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '-' || c == '_' ||
         (static_cast<unsigned char>(c) >= 0x80);
}
std::string Lower(std::string text) {
  for (char& c : text) if (c >= 'A' && c <= 'Z') c = char(c - 'A' + 'a');
  return text;
}
// An attribute selector on the style attribute: "[" , optional namespace
// prefix ("*|", "ns|", "|"), "style" in any case, then "]", space or an operator.
bool StyleAttributeSelector(const std::string& selector) {
  for (size_t at = selector.find('['); at != std::string::npos; at = selector.find('[', at + 1)) {
    size_t i = at + 1;
    while (i < selector.size() && (selector[i] == ' ' || selector[i] == '\t' || selector[i] == '\n')) ++i;
    const size_t bar = selector.find('|', i);
    const size_t close = selector.find(']', i);
    if (bar != std::string::npos && bar < close && (bar + 1 >= selector.size() || selector[bar + 1] != '='))
      i = bar + 1;
    if (i + 5 > selector.size()) continue;
    const std::string name = Lower(selector.substr(i, 5));
    const char next = i + 5 < selector.size() ? selector[i + 5] : ']';
    if (name == "style" && (next == ']' || next == ' ' || next == '=' || next == '~' || next == '|' ||
                            next == '^' || next == '$' || next == '*' || next == '\t' || next == '\n')) return true;
  }
  return false;
}
// One :has() argument, compound by compound: each compound's class, id,
// attribute-name and type keys enter the :has key sets. A compound with a
// pseudo-class (:only-child, :hover ...) also depends on its siblings: its
// keys enter the sibling sets, or with no key at all (or a universal type)
// any tree change may change the argument's match.
void HasArgument(const std::string& argument, SelectorDependencies* out) {
  struct Compound { std::vector<std::string> classes, ids, tags; bool pseudo = false, universal = false; };
  Compound current;
  auto flush = [&] {
    const bool keyed = !current.classes.empty() || !current.ids.empty() || !current.tags.empty();
    for (const auto& c : current.classes) out->hasClasses.insert(c);
    for (const auto& i : current.ids) out->hasIds.insert(i);
    for (const auto& t : current.tags) out->hasTags.insert(t);
    if (current.pseudo || current.universal) {
      if (!keyed) out->hasStructural = true;
      for (const auto& c : current.classes) out->hasSiblingClasses.insert(c);
      for (const auto& i : current.ids) out->hasSiblingIds.insert(i);
      for (const auto& t : current.tags) out->hasSiblingTags.insert(t);
    }
    current = Compound();
  };
  for (size_t k = 0; k < argument.size();) {
    const char c = argument[k];
    if (c == ' ' || c == '>' || c == '+' || c == '~' || c == ',' || c == '(' || c == ')' || c == '\t' || c == '\n') {
      flush(); ++k; continue;
    }
    if (c == '*') { current.universal = true; ++k; continue; }
    if (c == '.' || c == '#' || c == '[' || c == ':' || IdentChar(c)) {
      size_t j = c == '.' || c == '#' || c == '[' || c == ':' ? k + 1 : k;
      if (c == ':' && j < argument.size() && argument[j] == ':') ++j;
      if (c == '[') {
        while (j < argument.size() && argument[j] == ' ') ++j;
        const size_t bar = argument.find('|', j), end = argument.find_first_of("]=~^$*", j);
        if (bar != std::string::npos && bar < end) j = bar + 1;
      }
      size_t e = j;
      while (e < argument.size() && (IdentChar(argument[e]) || argument[e] == '\\')) e += argument[e] == '\\' ? 2 : 1;
      const std::string name = argument.substr(j, e - j);
      if (c == '.') current.classes.push_back(name);
      else if (c == '#') current.ids.push_back(name);
      else if (c == '[') { out->hasAttributes.insert(Lower(name)); e = argument.find(']', e); e = e == std::string::npos ? argument.size() : e + 1; }
      else if (c == ':') {
        const std::string pseudo = Lower(name);
        // Functional :not/:is/:where/:has only group compounds read below.
        if (pseudo != "not" && pseudo != "is" && pseudo != "where" && pseudo != "has" && pseudo != "matches")
          current.pseudo = true;
      } else if (!name.empty()) current.tags.push_back(Lower(name));
      k = e > k ? e : k + 1;
      continue;
    }
    ++k;
  }
  flush();
}
void CollectHas(const std::string& selector, SelectorDependencies* out) {
  for (size_t at = selector.find(":has("); at != std::string::npos; at = selector.find(":has(", at + 5)) {
    size_t i = at + 5, depth = 1;
    const size_t begin = i;
    while (i < selector.size() && depth) {
      if (selector[i] == '(') ++depth; else if (selector[i] == ')') --depth;
      ++i;
    }
    HasArgument(selector.substr(begin, i - begin - (depth ? 0 : 1)), out);
  }
}
size_t SkipQuoted(const std::string& s, size_t i) {
  const char quote = s[i++];
  while (i < s.size() && s[i] != quote) i += (s[i] == '\\' && i + 1 < s.size()) ? 2 : 1;
  return i < s.size() ? i + 1 : i;
}
bool Space(char c) { return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f'; }
// Scans for sibling combinators (+, ~) outside attribute brackets, strings,
// escapes and :nth-*(An+B) arithmetic, and handles :nth-*(An+B of S).
void ScanSiblingDependencies(const std::string& s, SelectorDependencies* out) {
  for (size_t i = 0; i < s.size();) {
    const char c = s[i];
    if (c == '\\') { i += i + 1 < s.size() ? 2 : 1; continue; }
    if (c == '\'' || c == '"') { i = SkipQuoted(s, i); continue; }
    if (c == '[') {
      ++i;
      while (i < s.size() && s[i] != ']') {
        if (s[i] == '\\') i += i + 1 < s.size() ? 2 : 1;
        else if (s[i] == '\'' || s[i] == '"') i = SkipQuoted(s, i);
        else ++i;
      }
      if (i < s.size()) ++i;
      continue;
    }
    if (c == ':' && i + 5 < s.size() && Lower(s.substr(i + 1, 4)) == "nth-") {
      size_t j = i + 5;
      while (j < s.size() && IdentChar(s[j])) ++j;
      const std::string pseudo = Lower(s.substr(i + 1, j - i - 1));
      if (j < s.size() && s[j] == '(') {
        size_t k = j + 1, depth = 1, of_end = std::string::npos;
        while (k < s.size() && depth) {
          if (s[k] == '\\') { k += k + 1 < s.size() ? 2 : 1; continue; }
          if (s[k] == '\'' || s[k] == '"') { k = SkipQuoted(s, k); continue; }
          if (s[k] == '(') ++depth;
          else if (s[k] == ')') --depth;
          else if (depth == 1 && of_end == std::string::npos && Space(s[k]) && k + 4 <= s.size()) {
            size_t m = k + 1;
            while (m < s.size() && Space(s[m])) ++m;
            if (m + 3 <= s.size() && Lower(s.substr(m, 2)) == "of" && Space(s[m + 2])) of_end = m + 3;
          }
          ++k;
        }
        if (of_end != std::string::npos && of_end < k) {
          const std::string list = s.substr(of_end, k - of_end - (depth ? 0 : 1));
          out->siblingCombinator = true;
          ScanSiblingDependencies(list, out);
          if (pseudo.find("last") != std::string::npos) HasArgument(list, out);
        }
        i = k;
        continue;
      }
    }
    if (c == '+' || c == '~') out->siblingCombinator = true;
    ++i;
  }
}
}  // namespace
void CollectSelectorDependencies(const std::string& selector, SelectorDependencies* out) {
  if (StyleAttributeSelector(selector)) out->styleAttribute = true;
  ScanSiblingDependencies(selector, out);
  const std::string lower = Lower(selector);
  if (lower.find(":last-") != std::string::npos || lower.find(":nth-last-") != std::string::npos ||
      lower.find(":only-") != std::string::npos || lower.find("-of-type") != std::string::npos)
    out->fromEnd = true;
  if (lower.find(":empty") != std::string::npos || lower.find(":blank") != std::string::npos) out->empty = true;
  if (lower.find(":has(") != std::string::npos) CollectHas(selector, out);
}
}  // namespace ruyijs::web_styling

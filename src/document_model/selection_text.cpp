// Selection.toString(): Firefox's plain-text serializer (nsPlainTextSerializer)
// over the selected range, measured against Firefox 160 (ci/selection_check.py).
//   - whitespace collapses outside <pre>/<textarea>, a no-break space reads as
//     a space, a line's trailing space is dropped at its break;
//   - opening a block forces its vertical space at once (one break, two for a
//     paragraph-like block); closing one only defers it to the next text;
//   - <br> breaks the line, a table cell after the first is a tab, <img> is
//     its alt text, list and quote levels indent a line by four spaces;
//   - script/style/template content never serializes;
//   - the line break is the platform's ("\r\n" for a Windows profile).
// Every position is compared against the live range boundaries.
#include "document_model/selection.h"

#include <algorithm>

#include "document_model/state.h"
#include "browser_environment/environment.h"

namespace ruyijs::document_model {
namespace {
DomString LowerTag(const NodeSnapshot& value) {
  DomString tag = value.is_html_element && value.local_name ? *value.local_name : DomString();
  for (auto& c : tag) if (c >= u'A' && c <= u'Z') c = char16_t(c - u'A' + u'a');
  return tag;
}
bool Skip(const DomString& tag) {
  return tag == u"script" || tag == u"style" || tag == u"template" || tag == u"head" ||
         tag == u"title" || tag == u"noscript" || tag == u"select" || tag == u"input";
}
bool Block(const DomString& tag) {
  static const char16_t* const names[] = {u"div", u"p", u"h1", u"h2", u"h3", u"h4", u"h5", u"h6",
      u"ul", u"ol", u"li", u"dl", u"dt", u"dd", u"table", u"tr", u"blockquote", u"section",
      u"article", u"header", u"footer", u"nav", u"aside", u"main", u"form", u"fieldset",
      u"figure", u"figcaption", u"pre", u"address", u"hr", u"details", u"summary", u"dialog",
      u"center", u"body", u"html"};
  for (const char16_t* name : names) if (tag == name) return true;
  return false;
}
// Gecko's paragraph-like blocks: a blank line on each side.
bool Paragraph(const DomString& tag) {
  return tag == u"p" || tag == u"h1" || tag == u"h2" || tag == u"h3" || tag == u"h4" ||
         tag == u"h5" || tag == u"h6" || tag == u"ul" || tag == u"ol" || tag == u"dl" ||
         tag == u"blockquote" || tag == u"pre";
}
bool Indenting(const DomString& tag) {
  return tag == u"ul" || tag == u"ol" || tag == u"dl" || tag == u"blockquote";
}
uint32_t Length(const NodeSnapshot& value) {
  return value.kind == NodeKind::Text || value.kind == NodeKind::Comment ||
         value.kind == NodeKind::ProcessingInstruction ? uint32_t(value.data_length)
                                                       : uint32_t(value.child_count);
}

struct Writer {
  DomString line_break, text;
  size_t breaks = 0;    // Line breaks since the last visible character.
  size_t floating = 0;  // Breaks a closed block asks for before the next text.
  size_t indent = 0;    // List/quote levels of the current position.
  bool line_start = true, space = false;
  // A collapsible space is held in `space` until visible text follows, so a
  // break drops it and the text never ends a line with one.
  void Break(size_t count) {
    while (breaks < count) { text += line_break; ++breaks; }
    line_start = true;
    space = false;
  }
  // A block opens: its vertical space now, unless nothing was written yet.
  void Open(size_t count) {
    floating = 0;
    if (!text.empty()) Break(count);
  }
  void Close(size_t count) { if (!text.empty() && floating < count) floating = count; }
  void Visible(char16_t c) {
    if (floating) { Break(floating); floating = 0; }
    if (line_start) text.append(indent * 4, u' ');
    if (space) { text += u' '; space = false; }
    text += c;
    breaks = 0;
    line_start = false;
  }
  void Append(DomStringView data, bool pre) {
    for (char16_t c : data) {
      if (pre) {
        if (c == u'\n') { Break(breaks + 1); continue; }
        if (c == u'\r') continue;
        Visible(c == 0x00A0 ? u' ' : c);
        continue;
      }
      if (c == u' ' || c == u'\t' || c == u'\n' || c == u'\r' || c == u'\f') {
        // Collapsible space: one, and none right after a line break.
        if (!(line_start && breaks)) space = true;
        continue;
      }
      if (c == 0x200B) continue;
      Visible(c == 0x00A0 ? u' ' : c);
    }
  }
};

struct TextWalker {
  State& state;
  const BoundaryPoint& start;
  const BoundaryPoint& end;
  NodeKey root;  // The range's common ancestor.
  Writer out;
  // -1, 0, 1: the point against the start / end boundary.
  bool AfterStart(const BoundaryPoint& point, bool* result) {
    int order = 0;
    if (!RangeComparePoints(state, start, point, &order)) return false;
    *result = order <= 0;
    return true;
  }
  bool BeforeEnd(const BoundaryPoint& point, bool inclusive, bool* result) {
    int order = 0;
    if (!RangeComparePoints(state, point, end, &order)) return false;
    *result = inclusive ? order <= 0 : order < 0;
    return true;
  }
  bool Inside(const BoundaryPoint& point, bool inclusive, bool* result) {
    bool after = false, before = false;
    if (!AfterStart(point, &after)) return false;
    if (!after) { *result = false; return true; }
    if (!BeforeEnd(point, inclusive, &before)) return false;
    *result = before;
    return true;
  }
  bool Visit(NodeKey node, bool pre, int cell) {
    NodeSnapshot value;
    if (!state.Check(state.model.Inspect(node, &value))) return false;
    const uint32_t length = Length(value);
    // Subtrees wholly before the start or after the end are skipped.
    bool reaches = false, begins = false;
    if (!AfterStart({node, length}, &reaches) || !BeforeEnd({node, 0}, true, &begins)) return false;
    if (!reaches || !begins) return true;
    if (value.kind == NodeKind::Text) {
      DomString data;
      if (!state.Check(state.model.SubstringData(node, 0, length, &data))) return false;
      uint32_t from = 0, to = length;
      // Clamped: character data that shrank leaves a boundary past its end.
      if (Same(start.node, node)) from = std::min(start.offset, length);
      if (Same(end.node, node)) to = std::min(end.offset, length);
      if (from < to) out.Append(DomStringView(data).substr(from, to - from), pre);
      return true;
    }
    if (value.kind != NodeKind::Element && value.kind != NodeKind::Document &&
        value.kind != NodeKind::DocumentFragment) return true;
    const DomString tag = value.kind == NodeKind::Element ? LowerTag(value) : DomString();
    if (Skip(tag)) return true;
    // Hidden content (the hidden attribute, an inline display:none) has no box.
    if (value.kind == NodeKind::Element) {
      std::optional<DomString> hidden, style;
      if (!state.Check(state.model.GetAttribute(node, DomString(u"hidden"), &hidden)) ||
          !state.Check(state.model.GetAttribute(node, DomString(u"style"), &style))) return false;
      if (hidden) return true;
      if (style) {
        DomString compact;
        for (char16_t c : *style)
          if (c != u' ' && c != 9) compact += c >= u'A' && c <= u'Z' ? char16_t(c + 32) : c;
        if (compact.find(u"display:none") != DomString::npos) return true;
      }
    }
    bool here = false;
    if (tag == u"br" || tag == u"img" || ((tag == u"td" || tag == u"th") && cell > 0)) {
      if (!Inside({node, 0}, false, &here)) return false;
      if (here && tag == u"br") out.Break(out.breaks + 1);
      if (here && tag == u"img") {
        std::optional<DomString> alt;
        if (!state.Check(state.model.GetAttribute(node, DomString(u"alt"), &alt))) return false;
        if (alt) out.Append(*alt, pre);
      }
      if (here && tag != u"br" && tag != u"img") out.Visible(u'\t');
      if (tag == u"br" || tag == u"img") return true;
    }
    // The common ancestor holds the whole range: its own edges are outside it
    // and force no break (a trailing collapsible space of the range stays).
    const bool block = Block(tag) && !Same(node, root), paragraph = Paragraph(tag);
    if (block) {
      if (!Inside({node, 0}, true, &here)) return false;
      if (here) out.Open(paragraph ? 2 : 1);
    }
    const size_t indent = out.indent;
    if (Indenting(tag)) ++out.indent;
    std::vector<NodeKey> children;
    if (!state.Check(state.model.Children(node, &children))) return false;
    int cells = 0;
    for (NodeKey child : children) {
      NodeSnapshot kid;
      if (!state.Check(state.model.Inspect(child, &kid))) return false;
      const DomString kidTag = kid.kind == NodeKind::Element ? LowerTag(kid) : DomString();
      const bool isCell = kidTag == u"td" || kidTag == u"th";
      if (!Visit(child, pre || tag == u"pre" || tag == u"textarea", isCell ? cells : 0)) return false;
      if (isCell) ++cells;
    }
    out.indent = indent;
    if (block) {
      if (!Inside({node, length}, true, &here)) return false;
      if (here) out.Close(paragraph ? 2 : 1);
    }
    return true;
  }
};
}  // namespace

bool SelectionRangeText(State& state, Range& range, DomString* result) {
  State& tree = RangeTree(range);
  NodeKey common;
  if (!RangeCommonAncestor(tree, range.start, range.end, &common)) return false;
  TextWalker walker{tree, range.start, range.end, common, {}};
  std::string platform;
  if (!state.environment.ReadPlatform(&platform)) platform.clear();
  walker.out.line_break = platform.rfind("Win", 0) == 0 ? u"\r\n" : u"\n";
  // The common ancestor's own ancestors set the starting pre state and indent.
  bool pre = false;
  for (NodeKey node = common; node.node;) {
    NodeSnapshot value;
    if (!tree.Check(tree.model.Inspect(node, &value))) return false;
    const DomString tag = value.kind == NodeKind::Element ? LowerTag(value) : DomString();
    if (!Same(node, common) && Indenting(tag)) ++walker.out.indent;
    pre = pre || tag == u"pre" || tag == u"textarea";
    node = value.parent;
  }
  NodeSnapshot value;
  if (!tree.Check(tree.model.Inspect(common, &value))) return false;
  const DomString tag = value.kind == NodeKind::Element ? LowerTag(value) : DomString();
  if (tag == u"pre" || tag == u"textarea") pre = false;  // re-entered by Visit
  for (NodeKey node = value.parent; node.node;) {
    NodeSnapshot parent;
    if (!tree.Check(tree.model.Inspect(node, &parent))) return false;
    const DomString parentTag = parent.kind == NodeKind::Element ? LowerTag(parent) : DomString();
    if (parentTag == u"pre" || parentTag == u"textarea") { pre = true; break; }
    node = parent.parent;
  }
  if (!walker.Visit(common, pre, 0)) return false;
  // The range's last collapsible space stays (Firefox serializes "  a  b  " as " a b ").
  if (walker.out.space && !walker.out.floating) walker.out.text += u' ';
  *result += walker.out.text;
  return true;
}
bool SelectionText(State& state, DomString* result) {
  result->clear();
  std::vector<Range*> ranges;
  if (!SelectionRanges(state, &ranges)) return false;
  for (Range* range : ranges)
    if (!SelectionRangeText(state, *range, result)) return false;
  return true;
}
}  // namespace ruyijs::document_model

// contenteditable editing at the window Selection, measured against Firefox
// 160's HTMLEditor (ci/selection_check.py rows "edit.*"):
//   - typed text joins the Text node at the caret (a point between nodes
//     joins the Text before it, else after it, else gets a new Text node); an
//     empty block's <br> placeholder and an invisible trailing <br> give way;
//   - a run of typed spaces alternates no-break and normal spaces so it stays
//     visible, with a no-break space at a block edge;
//   - Backspace removes the character before the caret (a surrogate pair at
//     once), a <br>/<img> before it, or joins its block with the one before;
//   - Enter splits the caret's block (wrapping the host's loose inline content
//     into a <div> first; a heading split at its end continues as a <div>);
//   - an emptied block keeps a <br> placeholder.
// Every change goes through the Model, so MutationObservers, live collections
// and layout see it as they see a script's own edit.
#include "document_model/editing.h"

#include <algorithm>

#include "document_model/focus_internal.h"
#include "document_model/selection.h"
#include "document_model/state.h"

namespace ruyijs::document_model {
using namespace focus_detail;
namespace {
bool BlockTag(const DomString& tag) {
  static const char16_t* const names[] = {u"div", u"p", u"h1", u"h2", u"h3", u"h4", u"h5", u"h6",
      u"ul", u"ol", u"li", u"dl", u"dt", u"dd", u"table", u"tbody", u"thead", u"tfoot", u"tr",
      u"td", u"th", u"caption", u"blockquote", u"pre", u"section", u"article", u"header",
      u"footer", u"nav", u"aside", u"main", u"form", u"fieldset", u"figure", u"figcaption",
      u"address", u"hr", u"details", u"summary", u"dialog", u"center", u"body", u"html"};
  for (const char16_t* name : names) if (tag == name) return true;
  return false;
}
bool AtomicTag(const DomString& tag) {
  static const char16_t* const names[] = {u"br", u"img", u"input", u"button", u"select",
      u"textarea", u"canvas", u"video", u"audio", u"iframe", u"embed", u"object", u"svg",
      u"math", u"wbr", u"meter", u"progress"};
  for (const char16_t* name : names) if (tag == name) return true;
  return false;
}
bool Heading(const DomString& tag) {
  return tag.size() == 2 && tag[0] == u'h' && tag[1] >= u'1' && tag[1] <= u'6';
}
bool Space(char16_t c) { return c == u' ' || c == 0x00A0; }

struct Editor {
  State& s;
  NodeKey host, document;
  BoundaryPoint caret;

  bool Snap(NodeKey node, NodeSnapshot* value) { return s.Check(s.model.Inspect(node, value)); }
  bool Kids(NodeKey node, std::vector<NodeKey>* kids) { return s.Check(s.model.Children(node, kids)); }
  bool TagOf(NodeKey node, DomString* tag, NodeKind* kind = nullptr) {
    NodeSnapshot value;
    if (!Snap(node, &value)) return false;
    *tag = value.kind == NodeKind::Element ? Lower(Tag(value)) : DomString();
    if (kind) *kind = value.kind;
    return true;
  }
  bool IsText(NodeKey node, bool* text, uint32_t* length = nullptr) {
    NodeSnapshot value;
    if (!Snap(node, &value)) return false;
    *text = value.kind == NodeKind::Text;
    if (length) *length = uint32_t(value.data_length);
    return true;
  }
  bool Data(NodeKey node, DomString* data) {
    NodeSnapshot value;
    return Snap(node, &value) &&
           s.Check(s.model.SubstringData(node, 0, uint32_t(value.data_length), data));
  }
  bool Parent(NodeKey node, NodeKey* parent) {
    NodeSnapshot value;
    if (!Snap(node, &value)) return false;
    *parent = value.parent;
    return true;
  }
  bool Index(NodeKey parent, NodeKey child, uint32_t* index) { return RangeIndex(s, parent, child, index); }
  bool Sibling(NodeKey node, bool next, NodeKey* sibling) {
    NodeSnapshot value;
    if (!Snap(node, &value)) return false;
    *sibling = next ? value.next_sibling : value.previous_sibling;
    return true;
  }
  bool Block(NodeKey node, bool* block) {
    DomString tag;
    NodeKind kind;
    if (!TagOf(node, &tag, &kind)) return false;
    *block = kind == NodeKind::Element && BlockTag(tag);
    return true;
  }
  bool Atomic(NodeKey node, bool* atomic, DomString* tagOut = nullptr) {
    DomString tag;
    NodeKind kind;
    if (!TagOf(node, &tag, &kind)) return false;
    *atomic = kind == NodeKind::Element && AtomicTag(tag);
    if (tagOut) *tagOut = tag;
    return true;
  }
  // The caret's block: its nearest block ancestor-or-self below the host
  // (a null key when the caret is in the host's own inline content).
  bool BlockOf(NodeKey node, NodeKey* block) {
    *block = {};
    for (NodeKey at = node; at.node && !Same(at, host);) {
      bool is = false;
      if (!Block(at, &is)) return false;
      if (is) { *block = at; return true; }
      if (!Parent(at, &at)) return false;
    }
    return true;
  }
  // Text, or an element other than <br> with a box of its own (img, input...).
  bool Visible(NodeKey node, bool* visible) {
    NodeSnapshot value;
    if (!Snap(node, &value)) return false;
    if (value.kind == NodeKind::Text) { *visible = value.data_length > 0; return true; }
    if (value.kind != NodeKind::Element) { *visible = false; return true; }
    const DomString tag = Lower(Tag(value));
    if (tag == u"br") { *visible = false; return true; }
    if (AtomicTag(tag)) { *visible = true; return true; }
    std::vector<NodeKey> kids;
    if (!Kids(node, &kids)) return false;
    *visible = false;
    for (NodeKey kid : kids) {
      if (!Visible(kid, visible)) return false;
      if (*visible) return true;
    }
    return true;
  }
  bool NewNode(NodeKind kind, DomStringView name, DomStringView data, NodeKey parent, NodeKey before,
               NodeKey* out) {
    Draft draft;
    if (!s.Check(s.model.Create(document, {kind, name, data}, &draft))) return false;
    if (!s.Check(s.model.Insert(parent, draft.root, before))) { s.model.Rollback(draft.token); return false; }
    s.model.Publish(draft.token);
    *out = draft.root;
    return true;
  }
  // A shallow copy of `node` (attributes but its id), inserted at parent/before.
  bool CloneShallow(NodeKey node, NodeKey parent, NodeKey before, NodeKey* out) {
    Draft draft;
    if (!s.Check(s.model.Clone(node, false, &draft))) return false;
    if (!s.Check(s.model.Insert(parent, draft.root, before))) { s.model.Rollback(draft.token); return false; }
    s.model.Publish(draft.token);
    *out = draft.root;
    std::optional<DomString> id;
    if (!s.Check(s.model.GetAttribute(draft.root, u"id", &id))) return false;
    return !id || s.Check(s.model.RemoveAttribute(draft.root, u"id"));
  }
  bool Move(NodeKey node, NodeKey parent, NodeKey before) { return s.Check(s.model.Insert(parent, node, before)); }
  bool Remove(NodeKey node) {
    NodeKey parent;
    return Parent(node, &parent) && (!parent.node || s.Check(s.model.Remove(parent, node)));
  }
  bool Replace(NodeKey text, uint32_t offset, uint32_t count, DomStringView data) {
    return s.Check(s.model.ReplaceData(text, offset, count, data));
  }
  // A point on an atomic element moves before it (its parent and index).
  bool Normalize(BoundaryPoint* point) {
    bool atomic = false;
    if (!Atomic(point->node, &atomic)) return false;
    if (!atomic) return true;
    NodeKey parent;
    uint32_t index = 0;
    if (!Parent(point->node, &parent) || !parent.node || !Index(parent, point->node, &index)) return false;
    *point = {parent, index};
    return true;
  }
  bool IsBr(NodeKey node, bool* br) {
    DomString tag;
    if (!TagOf(node, &tag)) return false;
    *br = tag == u"br";
    return true;
  }

  // Where typed text goes: a Text node and offset (see the file comment).
  bool TextPoint(BoundaryPoint point, BoundaryPoint* out) {
    if (!Normalize(&point)) return false;
    bool text = false;
    if (!IsText(point.node, &text)) return false;
    if (text) { *out = point; return true; }
    std::vector<NodeKey> kids;
    if (!Kids(point.node, &kids)) return false;
    uint32_t at = std::min<uint32_t>(point.offset, uint32_t(kids.size()));
    NodeKey prev = at ? kids[at - 1] : NodeKey{}, next = at < kids.size() ? kids[at] : NodeKey{};
    bool br = false;
    if (next.node && Same(next, kids.back()) && IsBr(next, &br) && br) {
      if (!Remove(next)) return false;
      next = {};
    } else if (prev.node && !next.node && IsBr(prev, &br) && br) {
      // An invisible trailing <br>: the caret after it is at the line's end.
      if (!Remove(prev)) return false;
      --at;
      prev = at ? kids[at - 1] : NodeKey{};
    } else if (!br && JS_IsExceptionPending(s.cx)) return false;
    uint32_t length = 0;
    if (prev.node && IsText(prev, &text, &length) && text) { *out = {prev, length}; return true; }
    if (JS_IsExceptionPending(s.cx)) return false;
    if (next.node && IsText(next, &text) && text) { *out = {next, 0}; return true; }
    if (JS_IsExceptionPending(s.cx)) return false;
    NodeKey created;
    if (!NewNode(NodeKind::Text, {}, {}, point.node, next, &created)) return false;
    *out = {created, 0};
    return true;
  }
  // Whether nothing visible precedes (follows) `node` on its line.
  bool Edge(NodeKey node, bool next, bool* edge) {
    for (NodeKey at = node; at.node && !Same(at, host);) {
      NodeKey sibling;
      if (!Sibling(at, next, &sibling)) return false;
      if (sibling.node) {
        bool block = false, br = false;
        if (!Block(sibling, &block) || !IsBr(sibling, &br)) return false;
        *edge = block || br;
        return true;
      }
      NodeKey parent;
      if (!Parent(at, &parent)) return false;
      bool block = false;
      if (!parent.node || Same(parent, host) || (Block(parent, &block) && block)) { *edge = true; return true; }
      if (JS_IsExceptionPending(s.cx)) return false;
      at = parent;
    }
    *edge = true;
    return true;
  }
  // Firefox's white-space sequence for a run of `count` spaces between `from`
  // edges: no-break and normal spaces alternate from a no-break one; a lone
  // space between visible text stays normal; a run at a line's end ends in a
  // no-break space.
  static DomString SpaceRun(size_t count, bool startEdge, bool endEdge) {
    DomString run;
    for (size_t i = 0; i < count; ++i) run += i % 2 ? u' ' : char16_t(0x00A0);
    if (count == 1 && !startEdge && !endEdge) run[0] = u' ';
    if (endEdge && run.back() == u' ') run.back() = 0x00A0;
    return run;
  }
  // Re-derives the space runs of `text` touching [from, to].
  bool Spaces(NodeKey text, uint32_t from, uint32_t to) {
    DomString data;
    if (!Data(text, &data)) return false;
    uint32_t a = std::min<uint32_t>(from, uint32_t(data.size()));
    uint32_t b = std::min<uint32_t>(to, uint32_t(data.size()));
    while (a && Space(data[a - 1])) --a;
    while (b < data.size() && Space(data[b])) ++b;
    bool left = false, right = false;
    if (!Edge(text, false, &left) || !Edge(text, true, &right)) return false;
    for (uint32_t i = a; i < b;) {
      if (!Space(data[i])) { ++i; continue; }
      uint32_t j = i;
      while (j < b && Space(data[j])) ++j;
      const DomString run = SpaceRun(j - i, i == 0 && left, j == data.size() && right);
      if (data.compare(i, j - i, run) && !Replace(text, i, j - i, run)) return false;
      i = j;
    }
    return true;
  }
  // The end of `node`'s last Text (or the point after its last child).
  bool EndOf(NodeKey node, BoundaryPoint* point) {
    for (;;) {
      bool text = false;
      uint32_t length = 0;
      if (!IsText(node, &text, &length)) return false;
      if (text) { *point = {node, length}; return true; }
      std::vector<NodeKey> kids;
      if (!Kids(node, &kids)) return false;
      bool atomic = false;
      if (kids.empty() || (Atomic(kids.back(), &atomic) && atomic)) {
        if (JS_IsExceptionPending(s.cx)) return false;
        bool br = false;
        if (!kids.empty() && IsBr(kids.back(), &br) && br) { *point = {node, uint32_t(kids.size() - 1)}; return true; }
        *point = {node, uint32_t(kids.size())};
        return !JS_IsExceptionPending(s.cx);
      }
      node = kids.back();
    }
  }
  // The first Text (or empty element) of `node`, offset 0.
  bool StartOf(NodeKey node, BoundaryPoint* point) {
    for (;;) {
      bool text = false;
      if (!IsText(node, &text)) return false;
      if (text) { *point = {node, 0}; return true; }
      std::vector<NodeKey> kids;
      if (!Kids(node, &kids)) return false;
      bool atomic = false;
      if (kids.empty() || (Atomic(kids.front(), &atomic) && atomic)) { *point = {node, 0}; return !JS_IsExceptionPending(s.cx); }
      node = kids.front();
    }
  }
  // An emptied block (or host) keeps a <br> so its line stays.
  bool Placeholder(NodeKey container) {
    bool visible = false;
    if (!Visible(container, &visible)) return false;
    if (visible) return true;
    std::vector<NodeKey> kids;
    if (!Kids(container, &kids)) return false;
    bool br = false;
    for (NodeKey kid : kids) {
      bool is = false;
      if (!IsBr(kid, &is)) return false;
      if (is && !br) { br = true; continue; }
      if (!Remove(kid)) return false;
    }
    NodeKey created;
    if (!br && !NewNode(NodeKind::Element, u"br", {}, container, {}, &created)) return false;
    caret = {container, 0};
    return true;
  }
  // Removes empty Text nodes and inline elements left empty, up to `stop`.
  bool Prune(NodeKey node, NodeKey stop) {
    while (node.node && !Same(node, stop) && !Same(node, host)) {
      bool visible = false, block = false, atomic = false;
      if (!Visible(node, &visible) || !Block(node, &block) || !Atomic(node, &atomic)) return false;
      if (visible || block || atomic) return true;
      NodeKey parent;
      if (!Parent(node, &parent) || !Remove(node)) return false;
      node = parent;
    }
    return true;
  }
  bool MergeTexts(NodeKey left, NodeKey right, BoundaryPoint* join) {
    bool a = false, b = false;
    uint32_t length = 0;
    if (!IsText(left, &a, &length) || !IsText(right, &b)) return false;
    if (!a || !b) return true;
    DomString data;
    if (!Data(right, &data) || !Replace(left, length, 0, data) || !Remove(right)) return false;
    *join = {left, length};
    return true;
  }

  // ---- commands ----
  bool DeleteSelection(Range& range, bool* changed) {
    if (Same(range.start.node, range.end.node) && range.start.offset == range.end.offset) return true;
    NodeKey startBlock, endBlock;
    if (!BlockOf(range.start.node, &startBlock) || !BlockOf(range.end.node, &endBlock)) return false;
    if (!RangeDeleteContents(s, range)) return false;
    *changed = true;
    caret = range.start;
    if (endBlock.node && !Same(startBlock, endBlock)) {
      NodeSnapshot value;
      if (!Snap(endBlock, &value)) return false;
      if (value.is_connected) {
        // The blocks the deletion spanned become one (Firefox joins them).
        NodeKey target = startBlock.node ? startBlock : NodeKey{};
        std::vector<NodeKey> kids;
        if (!Kids(endBlock, &kids)) return false;
        if (target.node) {
          std::vector<NodeKey> before;
          if (!Kids(target, &before)) return false;
          NodeKey last = before.empty() ? NodeKey{} : before.back();
          for (NodeKey kid : kids) if (!Move(kid, target, {})) return false;
          if (!Remove(endBlock)) return false;
          // The end block's first Text joins the start block's last one; a
          // caret there keeps its offset (the text is appended after it).
          BoundaryPoint join;
          if (last.node && !kids.empty() && !MergeTexts(last, kids.front(), &join)) return false;
          if (join.node.node && Same(caret.node, kids.front())) caret = join;
        }
      }
    }
    bool text = false;
    uint32_t length = 0;
    if (!IsText(caret.node, &text, &length)) return false;
    if (text && caret.offset > length) caret.offset = length;
    return true;
  }
  bool InsertText(DomStringView data, bool* changed) {
    if (data.empty()) return true;
    BoundaryPoint point;
    if (!TextPoint(caret, &point)) return false;
    if (!Replace(point.node, point.offset, 0, data)) return false;
    *changed = true;
    if (!Spaces(point.node, point.offset, point.offset + uint32_t(data.size()))) return false;
    caret = {point.node, point.offset + uint32_t(data.size())};
    return true;
  }
  bool DeleteBackward(bool* changed) {
    if (!Normalize(&caret)) return false;
    for (unsigned guard = 0; guard < 4096; ++guard) {
      bool text = false;
      uint32_t length = 0;
      if (!IsText(caret.node, &text, &length)) return false;
      if (text && caret.offset > 0) return DeleteChar(changed);
      // The node before the caret, climbing out of inline ancestors.
      NodeKey current, previous, container;
      if (text) {
        current = caret.node;
        if (!Sibling(current, false, &previous)) return false;
      } else {
        std::vector<NodeKey> kids;
        if (!Kids(caret.node, &kids)) return false;
        if (caret.offset) {
          previous = kids[std::min<size_t>(caret.offset, kids.size()) - 1];
          current = caret.offset < kids.size() ? kids[caret.offset] : NodeKey{};
          container = caret.node;
        } else {
          bool block = false;
          if (Same(caret.node, host)) return true;
          if (!Block(caret.node, &block)) return false;
          if (block) return JoinBlock(caret.node, changed);
          current = caret.node;
          if (!Sibling(current, false, &previous)) return false;
        }
      }
      while (!previous.node) {
        NodeKey parent;
        if (!Parent(current, &parent)) return false;
        if (!parent.node || Same(parent, host)) return true;  // the host's start
        bool block = false;
        if (!Block(parent, &block)) return false;
        if (block) return JoinBlock(parent, changed);
        current = parent;
        if (!Sibling(current, false, &previous)) return false;
      }
      if (!container.node && !Parent(previous, &container)) return false;
      uint32_t previousLength = 0;
      bool block = false, atomic = false;
      DomString tag;
      if (!IsText(previous, &text, &previousLength) || !Block(previous, &block) ||
          !Atomic(previous, &atomic, &tag)) return false;
      if (text) {
        if (!previousLength) { if (!Remove(previous)) return false; continue; }
        caret = {previous, previousLength};
        continue;
      }
      if (atomic) {
        NodeKey before;
        uint32_t index = 0;
        if (!Sibling(previous, false, &before) || !Index(container, previous, &index) || !Remove(previous)) return false;
        *changed = true;
        caret = {container, index};
        if (tag == u"br" && current.node) {
          // A <br> left trailing after the line's text is invisible: it goes too.
          bool br = false;
          NodeKey after;
          if (!IsBr(current, &br) || !Sibling(current, true, &after)) return false;
          if (br && !after.node && before.node && !Remove(current)) return false;
        }
        if (before.node && current.node) {
          BoundaryPoint join;
          if (!MergeTexts(before, current, &join)) return false;
          if (join.node.node) caret = join;
        }
        return true;
      }
      if (block) return JoinInline(previous, current, container, changed);
      // An inline element: continue at the end of its content.
      bool visible = false;
      if (!Visible(previous, &visible)) return false;
      if (!visible) { if (!Remove(previous)) return false; continue; }
      if (!EndOf(previous, &caret)) return false;
    }
    return true;
  }
  // Delete's default action inside a Text node: the character after the caret
  // goes as Backspace takes the one before it. At a Text node's end Firefox
  // reaches into the next node or block; that is not modeled (no change).
  bool DeleteForward(bool* changed) {
    if (!Normalize(&caret)) return false;
    bool text = false;
    uint32_t length = 0;
    if (!IsText(caret.node, &text, &length)) return false;
    if (!text || caret.offset >= length) return true;
    DomString data;
    if (!Data(caret.node, &data)) return false;
    uint32_t count = 1;
    if (caret.offset + 1 < length && data[caret.offset] >= 0xD800 && data[caret.offset] <= 0xDBFF &&
        data[caret.offset + 1] >= 0xDC00 && data[caret.offset + 1] <= 0xDFFF) count = 2;
    caret.offset += count;
    return DeleteChar(changed);
  }
  bool DeleteChar(bool* changed) {
    DomString data;
    if (!Data(caret.node, &data)) return false;
    uint32_t count = 1;
    if (caret.offset >= 2 && data[caret.offset - 1] >= 0xDC00 && data[caret.offset - 1] <= 0xDFFF &&
        data[caret.offset - 2] >= 0xD800 && data[caret.offset - 2] <= 0xDBFF) count = 2;
    if (!Replace(caret.node, caret.offset - count, count, u"")) return false;
    *changed = true;
    caret.offset -= count;
    if (data.size() > count) return Spaces(caret.node, caret.offset, caret.offset);
    // The emptied Text goes, with inline elements it leaves empty; the caret
    // moves to the end of the text before it.
    NodeKey text = caret.node, block, previous, parent;
    if (!BlockOf(text, &block) || !Parent(text, &parent)) return false;
    BoundaryPoint before{};
    for (NodeKey at = text; at.node && !Same(at, block) && !Same(at, host);) {
      if (!Sibling(at, false, &previous)) return false;
      if (previous.node) {
        bool atomic = false;
        if (!Atomic(previous, &atomic)) return false;
        if (!atomic && !EndOf(previous, &before)) return false;
        break;
      }
      if (!Parent(at, &at)) return false;
    }
    uint32_t index = 0;
    if (!Index(parent, text, &index)) return false;
    caret = {parent, index};
    if (!Remove(text) || !Prune(parent, block)) return false;
    if (before.node.node) caret = before;
    NodeSnapshot value;
    if (!Snap(caret.node, &value)) return false;
    if (!value.is_connected) caret = {block.node ? block : host, 0};
    return Placeholder(block.node ? block : host);
  }
  // Backspace at the start of `block`: it joins what precedes it.
  bool JoinBlock(NodeKey block, bool* changed) {
    NodeKey current = block, previous;
    for (;;) {
      if (!Sibling(current, false, &previous)) return false;
      while (previous.node) {  // whitespace-only Text between blocks is not content
        bool text = false;
        DomString data;
        if (!IsText(previous, &text)) return false;
        if (!text || !Data(previous, &data)) break;
        bool blank = true;
        for (char16_t c : data) blank = blank && (c == u' ' || c == u'\n' || c == u'\t' || c == u'\r');
        if (!blank) break;
        if (!Sibling(previous, false, &previous)) return false;
      }
      if (previous.node) break;
      NodeKey parent;
      if (!Parent(current, &parent)) return false;
      if (!parent.node || Same(parent, host)) return true;
      current = parent;
    }
    bool blockEmpty = false, previousBlock = false;
    if (!Visible(block, &blockEmpty) || !Block(previous, &previousBlock)) return false;
    blockEmpty = !blockEmpty;
    *changed = true;
    if (previousBlock) {
      // The deepest last block of the previous one receives the content.
      NodeKey target = previous;
      for (;;) {
        std::vector<NodeKey> kids;
        bool is = false;
        if (!Kids(target, &kids)) return false;
        if (kids.empty() || !Block(kids.back(), &is) || !is) break;
        target = kids.back();
      }
      if (JS_IsExceptionPending(s.cx)) return false;
      bool previousVisible = false;
      if (!Visible(previous, &previousVisible)) return false;
      if (!previousVisible) {
        if (!Remove(previous)) return false;
        caret = {block, 0};
        return true;
      }
      if (blockEmpty) {
        if (!Remove(block) || !EndOf(target, &caret)) return false;
        return true;
      }
      std::vector<NodeKey> targetKids, moved;
      if (!Kids(target, &targetKids)) return false;
      bool br = false;
      if (!targetKids.empty() && IsBr(targetKids.back(), &br) && br) {
        if (!Remove(targetKids.back())) return false;
        targetKids.pop_back();
      }
      if (JS_IsExceptionPending(s.cx)) return false;
      if (!EndOf(target, &caret) || !Kids(block, &moved)) return false;
      for (NodeKey kid : moved) if (!Move(kid, target, {})) return false;
      if (!Remove(block)) return false;
      if (!targetKids.empty() && !moved.empty()) {
        BoundaryPoint join;
        if (!MergeTexts(targetKids.back(), moved.front(), &join)) return false;
        if (join.node.node) caret = join;
      }
      return true;
    }
    // Inline content precedes the block: the block's content follows it.
    BoundaryPoint end;
    if (!EndOf(previous, &end)) return false;
    NodeKey parent;
    if (!Parent(block, &parent)) return false;
    if (!blockEmpty) {
      std::vector<NodeKey> moved;
      if (!Kids(block, &moved)) return false;
      for (NodeKey kid : moved) if (!Move(kid, parent, block)) return false;
    }
    if (!Remove(block)) return false;
    caret = end;
    return true;
  }
  // Backspace after a block: the inline run starting at `current` joins it.
  bool JoinInline(NodeKey previousBlock, NodeKey current, NodeKey container, bool* changed) {
    *changed = true;
    bool visible = false;
    if (!Visible(previousBlock, &visible)) return false;
    if (!visible) {
      uint32_t index = 0;
      if (!Index(container, previousBlock, &index) || !Remove(previousBlock)) return false;
      caret = {container, index};
      return true;
    }
    NodeKey target = previousBlock;
    std::vector<NodeKey> kids;
    if (!Kids(target, &kids)) return false;
    bool br = false;
    if (!kids.empty() && IsBr(kids.back(), &br) && br && !Remove(kids.back())) return false;
    if (JS_IsExceptionPending(s.cx)) return false;
    if (!EndOf(target, &caret)) return false;
    if (!Kids(target, &kids)) return false;
    NodeKey last = kids.empty() ? NodeKey{} : kids.back(), first;
    for (NodeKey at = current; at.node;) {
      NodeKey next;
      bool block = false, isBr = false;
      if (!Sibling(at, true, &next) || !Block(at, &block) || !IsBr(at, &isBr)) return false;
      if (block) break;
      if (isBr) { if (!Remove(at)) return false; break; }
      if (!Move(at, target, {})) return false;
      if (!first.node) first = at;
      at = next;
    }
    if (last.node && first.node) {
      BoundaryPoint join;
      if (!MergeTexts(last, first, &join)) return false;
      if (join.node.node) caret = join;
    }
    return true;
  }
  // Moves everything after `point` inside `block` into `into`, copying the
  // inline elements between them.
  bool SplitInto(NodeKey block, BoundaryPoint point, NodeKey into) {
    NodeKey parent;
    uint32_t index = 0;
    bool text = false;
    uint32_t length = 0;
    if (!IsText(point.node, &text, &length)) return false;
    if (text) {
      if (!Parent(point.node, &parent) || !Index(parent, point.node, &index)) return false;
      if (point.offset >= length) ++index;
      else if (point.offset > 0) {
        NodeKey tail;
        Draft draft;
        if (!s.Check(s.model.Create(document, {NodeKind::Text, {}, {}}, &draft))) return false;
        s.model.Publish(draft.token);
        tail = draft.root;
        if (!s.Check(s.model.SplitTextInto(point.node, tail, point.offset))) return false;
        ++index;
      }
    } else {
      parent = point.node;
      index = point.offset;
    }
    NodeKey carry;  // the copy of the previous level, first of its parent's tail
    while (!Same(parent, block)) {
      std::vector<NodeKey> kids;
      NodeKey grand, copy, after;
      uint32_t at = 0;
      if (!Kids(parent, &kids) || !Parent(parent, &grand) || !Index(grand, parent, &at) ||
          !Sibling(parent, true, &after)) return false;
      if (index < kids.size()) {
        if (!CloneShallow(parent, grand, after, &copy)) return false;
        for (size_t i = index; i < kids.size(); ++i) if (!Move(kids[i], copy, {})) return false;
      }
      (void)carry;
      parent = grand;
      index = at + 1;
    }
    std::vector<NodeKey> kids;
    if (!Kids(block, &kids)) return false;
    for (size_t i = index; i < kids.size(); ++i) if (!Move(kids[i], into, {})) return false;
    return true;
  }
  bool InsertParagraph(bool* changed) {
    if (!Normalize(&caret)) return false;
    NodeKey block;
    if (!BlockOf(caret.node, &block)) return false;
    *changed = true;
    if (!block.node) {
      // The host's loose inline line around the caret becomes a <div>.
      NodeKey container = host, child;
      uint32_t offset = caret.offset;
      if (!Same(caret.node, host)) {
        for (child = caret.node;;) {
          NodeKey parent;
          if (!Parent(child, &parent)) return false;
          if (Same(parent, host)) break;
          child = parent;
        }
        if (!Index(host, child, &offset)) return false;
      }
      std::vector<NodeKey> kids;
      if (!Kids(container, &kids)) return false;
      size_t first = offset, last = child.node ? offset + 1 : offset;
      while (first > 0) {
        bool is = false;
        if (!Block(kids[first - 1], &is)) return false;
        if (is) break;
        --first;
      }
      while (last < kids.size()) {
        bool is = false;
        if (!Block(kids[last], &is)) return false;
        if (is) break;
        ++last;
      }
      NodeKey wrapper;
      if (!NewNode(NodeKind::Element, u"div", {}, host, first < kids.size() ? kids[first] : NodeKey{}, &wrapper))
        return false;
      for (size_t i = first; i < last; ++i) if (!Move(kids[i], wrapper, {})) return false;
      if (!child.node) caret = {wrapper, uint32_t(offset - first)};
      block = wrapper;
    }
    DomString tag;
    if (!TagOf(block, &tag)) return false;
    BoundaryPoint end;
    if (!EndOf(block, &end)) return false;
    bool atEnd = Same(end.node, caret.node) && end.offset <= caret.offset;
    NodeKey parent, after, next;
    if (!Parent(block, &parent) || !Sibling(block, true, &after)) return false;
    if (Heading(tag) && atEnd) {
      if (!NewNode(NodeKind::Element, u"div", {}, parent, after, &next)) return false;
    } else if (!CloneShallow(block, parent, after, &next)) return false;
    if (!SplitInto(block, caret, next)) return false;
    if (!Prune2(block) || !Prune2(next)) return false;
    if (!Placeholder(block)) return false;
    bool visible = false;
    if (!Visible(next, &visible)) return false;
    if (!visible) {
      if (!Placeholder(next)) return false;
      caret = {next, 0};
      return true;
    }
    return StartOf(next, &caret);
  }
  // Empty Text nodes and empty inline elements inside a split half go.
  bool Prune2(NodeKey node) {
    std::vector<NodeKey> kids;
    if (!Kids(node, &kids)) return false;
    for (NodeKey kid : kids) {
      bool visible = false, block = false, atomic = false;
      if (!Visible(kid, &visible) || !Block(kid, &block) || !Atomic(kid, &atomic)) return false;
      if (!visible && !block && !atomic) { if (!Remove(kid)) return false; continue; }
      if (!atomic && !Prune2(kid)) return false;
    }
    return true;
  }
  bool InsertLineBreak(bool* changed) {
    if (!Normalize(&caret)) return false;
    bool text = false;
    uint32_t length = 0;
    if (!IsText(caret.node, &text, &length)) return false;
    NodeKey parent, before, br;
    if (text) {
      if (!Parent(caret.node, &parent)) return false;
      if (caret.offset == 0) before = caret.node;
      else if (caret.offset >= length) { if (!Sibling(caret.node, true, &before)) return false; }
      else {
        Draft draft;
        if (!s.Check(s.model.Create(document, {NodeKind::Text, {}, {}}, &draft))) return false;
        s.model.Publish(draft.token);
        if (!s.Check(s.model.SplitTextInto(caret.node, draft.root, caret.offset))) return false;
        before = draft.root;
      }
    } else {
      parent = caret.node;
      std::vector<NodeKey> kids;
      if (!Kids(parent, &kids)) return false;
      if (caret.offset < kids.size()) before = kids[caret.offset];
    }
    if (!NewNode(NodeKind::Element, u"br", {}, parent, before, &br)) return false;
    *changed = true;
    uint32_t index = 0;
    if (!Index(parent, br, &index)) return false;
    bool following = false;
    if (before.node && !IsText(before, &following)) return false;
    if (following) { caret = {before, 0}; return true; }
    // A <br> ending its block needs a second one for the new line to show.
    NodeKey next;
    if (!Sibling(br, true, &next)) return false;
    if (!next.node && !NewNode(NodeKind::Element, u"br", {}, parent, {}, &next)) return false;
    caret = {parent, index + 1};
    return true;
  }
};

bool Editable(State& s, NodeKey node, NodeKey* host) {
  *host = {};
  for (NodeKey at = node; at.node;) {
    NodeSnapshot value;
    if (!s.Check(s.model.Inspect(at, &value))) return false;
    if (value.kind == NodeKind::Element && value.is_html_element) {
      std::optional<DomString> editable;
      if (!s.Check(s.model.GetAttribute(at, u"contenteditable", &editable))) return false;
      if (editable) {
        const DomString state = Lower(*editable);
        if (state == u"false") break;
        if (state.empty() || state == u"true" || state == u"plaintext-only") *host = at;
      }
    }
    at = value.parent;
  }
  if (host->node) return true;
  NodeSnapshot value;
  if (!s.Check(s.model.Inspect(node, &value))) return false;
  const NodeKey document = value.kind == NodeKind::Document ? node : value.owner_document;
  if (!document.node) return true;
  DomString design;
  if (!s.Check(s.model.ReadDocumentMetadata(document, DocumentMetadataField::DesignMode, DomStringView(), &design)))
    return false;
  if (design != u"on") return true;
  DocumentSnapshot snapshot;
  if (!s.Check(s.model.InspectDocument(document, &snapshot))) return false;
  *host = snapshot.body.node ? snapshot.body : snapshot.document_element;
  return true;
}
}  // namespace

bool EditHost(State& s, NodeKey node, NodeKey* host) { return Editable(s, node, host); }

bool EditFocusCaret(State& s, NodeKey host) {
  std::vector<Range*> ranges;
  if (!SelectionRanges(s, &ranges)) return false;
  Range* primary = SelectionPrimary(s);
  if (primary) {
    bool inside = false;
    if (!s.Check(s.model.AncestorOrSelf(host, primary->start.node, &inside))) return false;
    if (inside) return true;
  }
  NodeSnapshot value;
  if (!s.Check(s.model.Inspect(host, &value))) return false;
  Editor editor{s, host, value.owner_document, {}};
  BoundaryPoint start;
  if (!editor.StartOf(host, &start)) return false;
  return SelectionCollapse(s, start);
}

bool EditRun(State& s, NodeKey host, EditCommand command, DomStringView text, bool* changed) {
  *changed = false;
  if (!EditFocusCaret(s, host)) return false;
  Range* primary = SelectionPrimary(s);
  if (!primary) return true;
  NodeSnapshot value;
  if (!s.Check(s.model.Inspect(host, &value))) return false;
  // Boundaries are not moved by tree or character-data changes: one that a
  // shrunken node left past its end is brought back to that end first.
  for (BoundaryPoint* point : {&primary->start, &primary->end}) {
    NodeSnapshot at;
    if (!s.Check(s.model.Inspect(point->node, &at))) return false;
    const uint32_t length = at.kind == NodeKind::Text || at.kind == NodeKind::Comment ||
        at.kind == NodeKind::ProcessingInstruction ? uint32_t(at.data_length) : uint32_t(at.child_count);
    point->offset = std::min(point->offset, length);
  }
  Editor editor{s, host, value.owner_document, primary->start};
  JSAutoRealm realm(s.cx, s.page);
  if (!editor.DeleteSelection(*primary, changed)) return false;
  const bool deleted = *changed;
  switch (command) {
    case EditCommand::InsertText: if (!editor.InsertText(text, changed)) return false; break;
    case EditCommand::DeleteBackward: case EditCommand::DeleteForward:
      if (deleted) {
        NodeKey block;
        if (!editor.BlockOf(editor.caret.node, &block) || !editor.Placeholder(block.node ? block : host)) return false;
      } else if (command == EditCommand::DeleteForward ? !editor.DeleteForward(changed)
                                                       : !editor.DeleteBackward(changed)) return false;
      break;
    case EditCommand::InsertParagraph: if (!editor.InsertParagraph(changed)) return false; break;
    case EditCommand::InsertLineBreak: if (!editor.InsertLineBreak(changed)) return false; break;
  }
  return SelectionCollapse(s, editor.caret);
}
}  // namespace ruyijs::document_model

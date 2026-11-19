#include "document_model/model_internal.h"

namespace ruyijs::document_model {
Status Model::OuterHtml(NodeKey key, DomString* result) const noexcept {
  return Protect([&] {
    const auto& owner = State().owner(key);
    owner.require(key.node);
    auto value = State().serialize(key, true);
    result->swap(value);
  });
}
Status Model::ParseFragment(NodeKey context, DomStringView markup, NodeKey* root,
                            std::vector<NodeKey>* nodes) noexcept {
  return Protect([&] {
    auto& owner = State().owner(context);
    const html::Budget charge = [&](size_t count, size_t units) { State().budget(count,units); };
    html::TemplateSink sink;
    BindTemplateSink(sink,State(),context.document);
    const domain::NodeId fragment =
        ParseFragmentRoot(charge,State().limits,owner,ContextTag(owner,context.node),markup,sink,
                          ContextSpace(owner,context.node));
    std::vector<NodeKey> created;
    for (auto child : owner.require(fragment).child_ids())
      created.push_back(Key(context.document,child));
    *root = Key(context.document,fragment);
    nodes->swap(created);
  });
}
Status Model::ParseContextualFragment(NodeKey context, NodeKey fragment, DomStringView markup) noexcept {
  return Protect([&] {
    auto& owner = State().owner(fragment);
    DomString tag = u"body";
    html::TagSpace space = html::TagSpace::Html;
    if (!Null(context)) {
      const auto* element = State().owner(context).get_element(context.node);
      const bool rootHtml = element && element->tag_name().html() && element->tag_name().local16() == u"html" &&
                            State().owner(context).mode() == domain::DocumentMode::Html;
      if (element && !rootHtml) {
        tag = ContextTag(State().owner(context), context.node);
        space = ContextSpace(State().owner(context), context.node);
      }
    }
    const html::Budget charge = [&](size_t count, size_t units) { State().budget(count,units); };
    html::TemplateSink sink;
    BindTemplateSink(sink,State(),fragment.document);
    const domain::NodeId root = ParseFragmentRoot(charge,State().limits,owner,tag,markup,sink,space);
    try {
      for (auto id : owner.descendants(root)) {
        auto* element = owner.get_element(id);
        if (element && element->tag_name().is_script()) {
          element->script_state().started = false;
          element->script_state().parser_inserted = false;
        }
      }
      const std::vector<domain::NodeId> parsed = owner.require(root).child_ids();
      for (auto child : parsed) owner.append_child(fragment.node, child);
    } catch (...) { owner.remove_node(root); throw; }
    owner.remove_node(root);
    State().Touch();
  });
}
void Model::DiscardFragment(NodeKey root) noexcept {
  if (!impl_ || Null(root)) return;
  auto found = State().documents.find(root.document);
  if (found != State().documents.end()) found->second->remove_node(root.node);
}
Status Model::ReplaceOuter(NodeKey node, DomStringView markup) noexcept {
  return Protect([&] {
    auto& owner = State().owner(node);
    const domain::NodeId parent = owner.require(node.node).parent_id();
    if (!parent) throw domain::DomError(domain::DomErrorCode::NoModification,"outerHTML needs a parent node");
    // The replaced node leaves the tree BEFORE the parsed nodes enter it, so a
    // Document parent never holds two element children at once.
    const auto siblings = owner.require(parent).child_ids();
    const size_t index = size_t(owner.require(parent).index_of_child(node.node));
    const domain::NodeId following = index + 1 < siblings.size() ? siblings[index + 1] : 0;
    const html::Budget charge = [&](size_t count, size_t units) { State().budget(count,units); };
    html::TemplateSink sink;
    BindTemplateSink(sink,State(),node.document);
    const domain::NodeId fragment =
        ParseFragmentRoot(charge,State().limits,owner,ContextTag(owner,parent),markup,sink,
                          ContextSpace(owner,parent));
    // A copy, because each insert detaches the node from the fragment root.
    const std::vector<domain::NodeId> parsed = owner.require(fragment).child_ids();
    try {
      owner.remove_child(parent,node.node);
      State().NoteRangeRemove(Key(node.document, parent), node.node, uint32_t(index));
      for (auto child : parsed) owner.insert_before(parent,child,following);
      if (!parsed.empty())
        State().NoteRangeInsert(Key(node.document, parent), uint32_t(index), uint32_t(parsed.size()));
      State().NoteReplace(Key(node.document, parent), parsed, {node.node});
    } catch (...) { owner.remove_node(fragment); throw; }
    owner.remove_node(fragment);
    State().Touch();
  });
}
Status Model::InsertAdjacent(NodeKey node, int32_t position, DomStringView markup) noexcept {
  return Protect([&] {
    if (position < 0 || position > 3)
      throw domain::DomError(domain::DomErrorCode::Syntax,"Invalid insertAdjacent position");
    auto& owner = State().owner(node);
    const bool child = position == 1 || position == 2;  // afterbegin / beforeend
    const domain::NodeId parent = child ? node.node : owner.require(node.node).parent_id();
    if (!parent) throw domain::DomError(domain::DomErrorCode::NoModification,"insertAdjacent needs a parent node");
    const html::Budget charge = [&](size_t count, size_t units) { State().budget(count,units); };
    const DomString tag = child ? ContextTag(owner,node.node) : ContextTag(owner,parent);
    html::TemplateSink sink;
    BindTemplateSink(sink,State(),node.document);
    const domain::NodeId fragment = ParseFragmentRoot(charge,State().limits,owner,tag,markup,sink,
                          ContextSpace(owner,child ? node.node : parent));
    domain::NodeId anchor = 0;
    if (position == 0) anchor = node.node;
    else if (position == 1) anchor = owner.require(node.node).first_child_id();
    else if (position == 3) {
      const auto siblings = owner.require(parent).child_ids();
      const size_t index = size_t(owner.require(parent).index_of_child(node.node));
      anchor = index + 1 < siblings.size() ? siblings[index + 1] : 0;
    }
    // A copy, because each insert detaches the node from the fragment root.
    const std::vector<domain::NodeId> parsed = owner.require(fragment).child_ids();
    try {
      const uint32_t ins_idx = anchor
          ? uint32_t(owner.require(parent).index_of_child(anchor))
          : uint32_t(owner.require(parent).child_count());
      for (auto item : parsed) owner.insert_before(parent,item,anchor);
      if (!parsed.empty())
        State().NoteRangeInsert(Key(node.document, parent), ins_idx, uint32_t(parsed.size()));
      State().NoteInserted(Key(node.document, parent), parsed);
    } catch (...) { owner.remove_node(fragment); throw; }
    owner.remove_node(fragment);
    State().Touch();
  });
}
}


#include "document_model/model_internal.h"
#include "document_model/model_tree_builder.h"
#include <algorithm>
#include <vector>

extern "C" void jemalloc_free_dirty_pages();

namespace ruyijs::document_model {
namespace {
void RequireHtml(const domain::DocumentAggregate& owner) {
  if (!owner.uses_html_syntax())
    throw domain::DomError(domain::DomErrorCode::NotSupported, "XML markup parsing is not implemented");
}
// A large innerHTML replacement frees a detached tree. Release it and hand the
// dirty pages back. Smaller edits do not purge.
void PurgeLargeRemoval(void (*reclaim)(void*), void* context, domain::DocumentAggregate& owner,
                       const std::vector<domain::NodeId>& removed) {
  size_t count = 0;
  std::vector<domain::NodeId> pending;
  for (domain::NodeId id : removed) {
    pending.push_back(id);
    while (!pending.empty()) {
      const domain::NodeId current = pending.back();
      pending.pop_back();
      ++count;
      if (count >= 256) break;
      const auto& children = owner.require(current).child_ids();
      pending.insert(pending.end(), children.begin(), children.end());
    }
    if (count >= 256) break;
  }
  if (count < 256) return;
  if (reclaim) reclaim(context);
  jemalloc_free_dirty_pages();
}
}
Status Model::InnerHtml(NodeKey key, DomString* result) const noexcept {
  return Protect([&] {
    const auto& owner = State().owner(key);
    if (!owner.shadow_node(key.node)) State().element(key);
    auto value = State().serialize(key, false);
    result->swap(value);
  });
}
Status Model::SetInnerHtml(NodeKey key, DomStringView markup) noexcept {
  return Protect([&] {
    auto& owner = State().owner(key);
    const auto* shadow = owner.shadow_node(key.node);
    const auto& context = State().element(shadow ? Key(key.document, shadow->shadow_host_id()) : key);
    RequireHtml(owner);
    const auto space = shadow ? html::TagSpace::Html : ContextSpace(owner, key.node);
    if (!context.tag_name().html() && space == html::TagSpace::Html)
      throw domain::DomError(domain::DomErrorCode::NotSupported, "Markup parsing needs an HTML element");
    const html::Budget charge = [&](size_t nodes, size_t units) { State().budget(nodes, units); };
    const DomString tag = context.tag_name().local16();
    if (!shadow && space == html::TagSpace::Html && tag == u"template") {
      // The template contents are the context: parse inside the inert
      // template document and replace the content fragment's children.
      const NodeKey content = State().template_content(key);
      auto& inert = State().owner(content);
      html::TemplateSink sink;
      BindTemplateSink(sink, State(), content.document);
      const domain::NodeId root = ParseFragmentRoot(charge, State().limits, inert, tag, markup, sink);
      const auto old = inert.require(content.node).child_ids();
      try {
        const auto parsed = inert.require(root).child_ids();
        for (auto child : old) inert.remove_child(content.node, child);
        for (auto child : parsed) inert.append_child(content.node, child);
        State().NoteReplace(content, parsed, old);
      } catch (...) {
        inert.remove_node(root);
        throw;
      }
      inert.remove_node(root);
      State().Touch();
      PurgeLargeRemoval(State().reclaim, State().reclaimContext, inert, old);
      return;
    }
    html::TemplateSink sink;
    BindTemplateSink(sink, State(), key.document);
    // Fragment parsing: an unpublished html root collects the new nodes, so a
    // failure destroys only them and the old children stay in place.
    const domain::NodeId root = ParseFragmentRoot(charge, State().limits, owner, tag, markup, sink, space);
    const auto old = owner.require(key.node).child_ids();
    try {
      // Copies: the fragment root is emptied as each child moves into place.
      const auto parsed = owner.require(root).child_ids();
      for (auto child : old) owner.remove_child(key.node, child);
      for (auto child : parsed) owner.append_child(key.node, child);
      // The replacement is one record with both sides, taken at the same
      // change point that moved the nodes.
      State().NoteReplace(key, parsed, old);
    } catch (...) {
      owner.remove_node(root);
      throw;
    }
    owner.remove_node(root);
    State().Touch();
    PurgeLargeRemoval(State().reclaim, State().reclaimContext, owner, old);
  });
}
// Nodes a parser inserts are real changes too, so the session reports them into
// this owner's notice log; the sink lives in the session and its context is the
// Model impl that outlives every load.
void Model::BindParserNotice(html::Session& session, Impl& impl, uint64_t document) {
  session.notice.context = &impl;
  session.notice.document = document;
  session.notice.inserted = [](void* context, uint64_t document, uint64_t parent,
                               const std::vector<uint64_t>& added) {
    static_cast<Model::Impl*>(context)->NoteInserted(Key(document, parent), added);
  };
  session.notice.attributes = [](void* context, uint64_t document, uint64_t target,
                                 DomStringView name, const DomString* previous) {
    static_cast<Model::Impl*>(context)->NoteAttributes(
        Key(document, target), name, DomStringView(), previous);
  };
  session.notice.data = [](void* context, uint64_t document, uint64_t target,
                           const DomString* previous) {
    static_cast<Model::Impl*>(context)->NoteData(Key(document, target), previous);
  };
  session.tree.notice = &session.notice;
  BindTemplateSink(session.templates, impl, document);
  session.tree.template_sink = &session.templates;
}
void Model::BindTemplateSink(html::TemplateSink& sink, Impl& impl, uint64_t document) {
  sink.context = &impl;
  sink.document = document;
  sink.closed = [](void* context, uint64_t document, uint64_t element) {
    static_cast<Model::Impl*>(context)->MoveTemplateChildren(Key(document, element));
  };
}
html::TagSpace ContextSpace(const domain::DocumentAggregate& owner, domain::NodeId node) {
  const auto* element = owner.get_element(node);
  const auto space = element ? element->tag_name().namespace16() : DomStringView();
  return space == html::SpaceUri(html::TagSpace::Svg) ? html::TagSpace::Svg :
         space == html::SpaceUri(html::TagSpace::MathMl) ? html::TagSpace::MathMl : html::TagSpace::Html;
}
DomString ContextTag(const domain::DocumentAggregate& owner, domain::NodeId node) {
  const auto* element = owner.get_element(node);
  return element ? element->tag_name().local16() : DomString(u"html");
}
domain::NodeId ParseFragmentRoot(const html::Budget& charge, const Limits& limits,
                                 domain::DocumentAggregate& owner, DomStringView tag,
                                 DomStringView markup, const html::TemplateSink& templates,
                                 html::TagSpace space) {
  RequireHtml(owner);
  if (markup.size() > limits.text_units)
    throw domain::DomError(domain::DomErrorCode::LimitExceeded, "Markup exceeds the DOM text limit");
  charge(1, 4);
  const domain::NodeId root = owner.create_element16(u"html");
  try {
    const DomString input = html::Normalize(markup);
    // A foreign context starts in foreign content (data state, no raw text).
    html::Tokenizer tokenizer(input, space == html::TagSpace::Html ? html::ContentFor(tag) : html::Content::Data);
    html::TreeState tree;
    tree.root = tree.html = root;
    tree.template_sink = &templates;
    tree.context_space = space;
    tree.foreign = space != html::TagSpace::Html;
    tokenizer.Follow(&tree.foreign);
    tree.open.push_back(root);
    tree.mode = tag == u"html" ? html::Mode::BeforeHead : html::Mode::InBody;
    // "Reset the insertion mode appropriately" from the context element, so a
    // table, tbody, tr or td context starts in its table mode.
    if (space == html::TagSpace::Html && tag != u"html") {
      tree.context = DomString(tag);
      html::detail::Builder{owner, tree, charge}.ResetMode();
    }
    for (bool end = false; !end;) {
      html::Token token = tokenizer.Next();
      end = token.kind == html::Token::Kind::End;
      html::Build(owner, tree, token, charge);
    }
  } catch (...) { owner.remove_node(root); throw; }
  return root;
}
Status Model::BeginParse(NodeKey key, DomStringView markup) noexcept {
  return Protect([&] {
    auto& owner = State().document(key);
    RequireHtml(owner);
    if (markup.size() > State().limits.text_units)
      throw domain::DomError(domain::DomErrorCode::LimitExceeded, "Markup exceeds the DOM text limit");
    auto session = std::make_unique<html::Session>(html::Normalize(markup));
    session->tree.root = owner.id();
    session->tree.scripts = true;
    BindParserNotice(*session, State(), key.document);
    State().parsers[key.document] = std::move(session);  // Replaces an unfinished load.
    try {
      owner.set_parsed_doctype({});
      const auto children = owner.require(owner.id()).child_ids();
      for (auto child : children) owner.remove_child(owner.id(), child);  // Detach, never destroy.
      // Replacing the whole document is one removal record.
      State().NoteRemoved(key, children, {}, {});
    } catch (...) {
      State().parsers.erase(key.document);
      throw;
    }
    owner.set_ready_state(domain::ReadyState::Loading);
    owner.set_current_script(0);
    State().Touch();
  });
}
Status Model::ResumeParse(NodeKey key, NodeKey* script) noexcept {
  return Protect([&] {
    auto& owner = State().document(key);
    auto found = State().parsers.find(key.document);
    if (found == State().parsers.end() || !found->second)
      throw domain::DomError(domain::DomErrorCode::InvalidState, "Document has no active parser");
    auto& session = *found->second;
    session.insertion = html::Session::kNoInsertion;
    if (session.pending_script) {
      *script = Key(key.document, session.pending_script);
      session.pending_script = 0;
      session.insertion = session.tokenizer.Position();
      State().Touch();
      return;
    }
    const html::Budget budget = [&](size_t nodes, size_t units) { State().budget(nodes, units); };
    try {
      for (;;) {
        html::Token token = session.tokenizer.Next();
        const bool end = token.kind == html::Token::Kind::End;
        if (auto element = html::Build(owner, session.tree, token, budget)) {
          *script = Key(key.document, element);
          session.insertion = session.tokenizer.Position();
          State().Touch();
          return;
        }
        if (end) break;
      }
    } catch (...) {
      State().parsers.erase(found);  // A failed load cannot resume half-way.
      throw;
    }
    State().parsers.erase(found);
    *script = {};
    State().Touch();
  });
}
Status Model::ParserWrite(NodeKey key, DomStringView text, bool* written) noexcept {
  return Protect([&] {
    *written = false;
    State().document(key);
    auto found = State().parsers.find(key.document);
    if (found == State().parsers.end() || !found->second ||
        found->second->insertion == html::Session::kNoInsertion) return;
    auto& session = *found->second;
    const DomString normalized = html::Normalize(text);
    if (normalized.size() > State().limits.text_units - std::min(State().limits.text_units, session.input.size()))
      throw domain::DomError(domain::DomErrorCode::LimitExceeded, "Markup exceeds the DOM text limit");
    session.Write(normalized);
    *written = true;
    // The written text is parsed now, up to the insertion point, so the
    // writer sees the nodes it wrote (HTML document.write step "run the
    // tokenizer"). A script it closes waits for the parser to resume.
    auto& owner = State().document(key);
    const html::Budget budget = [&](size_t nodes, size_t units) { State().budget(nodes, units); };
    // The tokenizer sees only the input before the insertion point, so a text
    // run cannot reach into the document's own markup (a later write inserts
    // there). A tag the written text leaves unfinished is dropped (D7).
    session.tokenizer.Rebase(DomStringView(session.input).substr(0, session.insertion));
    try {
      while (!session.pending_script && session.tokenizer.Position() < session.insertion) {
        html::Token token = session.tokenizer.Next();
        if (token.kind == html::Token::Kind::End) break;
        if (auto element = html::Build(owner, session.tree, token, budget)) session.pending_script = element;
      }
    } catch (...) {
      State().parsers.erase(found);
      throw;
    }
    session.tokenizer.Rebase(session.input);
    State().Touch();
  });
}
Status Model::SetReadyState(NodeKey key, ReadyState state) noexcept {
  return Protect([&] {
    State().document(key).set_ready_state(state == ReadyState::Loading ? domain::ReadyState::Loading :
        state == ReadyState::Interactive ? domain::ReadyState::Interactive : domain::ReadyState::Complete);
  });
}
Status Model::SetCurrentScript(NodeKey key, NodeKey script) noexcept {
  return Protect([&] {
    auto& owner = State().document(key);
    if (!Null(script)) {
      const auto* element = script.document == key.document ? owner.get_element(script.node) : nullptr;
      if (!element || !element->tag_name().is_script())
        throw domain::DomError(domain::DomErrorCode::InvalidState, "currentScript must be a script of this document");
    }
    owner.set_current_script(script.node);
  });
}
}

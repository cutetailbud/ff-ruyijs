#include "document_model/model_internal.h"

namespace ruyijs::document_model {
Model::~Model() noexcept { delete impl_; }
Model::Impl& Model::State() const {
  if (!impl_) throw domain::DomError(domain::DomErrorCode::InvalidState,"DOM model is not initialized");
  return *impl_;
}
Status Model::Initialize(const Limits& limits) noexcept {
  return Protect([&] {
    if (impl_) throw domain::DomError(domain::DomErrorCode::InvalidState,"DOM model is already initialized");
    impl_ = new Impl(limits);
  });
}
Status Model::CreateDocument(DocumentInit init, Draft* result) noexcept {
  return Protect([&] {
    bool html = init == DocumentInit::MainHtml;
    State().budget(html ? 4 : 1,html ? 12 : 11);
    DocumentId identity = State().allocate_document();
    auto document = std::make_unique<domain::DocumentAggregate>(1,
        html ? domain::DocumentMode::Html : domain::DocumentMode::Xml, html);
    document->set_ready_state(domain::ReadyState::Complete);
    if (html) document->use_external_url(); else document->set_url16(u"about:blank");
    if (html) {
      auto root = document->create_element16(u"html");
      document->append_child(1,root);
      document->append_child(root,document->create_element16(u"head"));
      document->append_child(root,document->create_element16(u"body"));
    }
    State().documents.emplace(identity,std::move(document));
    *result = State().remember({identity,1},true);
  });
}
Status Model::Create(NodeKey document, const CreateSpec& spec, Draft* result) noexcept {
  return Protect([&] {
    auto& owner = State().document(document);
    State().budget(1,spec.kind == NodeKind::Element || spec.kind == NodeKind::DocumentType ? spec.name.size()
        : spec.kind == NodeKind::ProcessingInstruction ? spec.name.size() + spec.data.size() : spec.data.size());
    domain::NodeId node = 0;
    switch (spec.kind) {
      case NodeKind::Element: node = owner.create_element16(spec.name); break;
      case NodeKind::Text: node = owner.create_text_node16(spec.data); break;
      case NodeKind::Comment: node = owner.create_comment16(spec.data); break;
      case NodeKind::ProcessingInstruction:
        // Document.createProcessingInstruction: the target is an XML Name and
        // the data cannot close the instruction (Firefox's one message for both).
        if (!domain::valid_name(spec.name) || spec.data.find(u"?>") != DomString::npos)
          throw domain::DomError(domain::DomErrorCode::InvalidCharacter, "String contains an invalid character");
        node = owner.create_processing_instruction16(spec.name, spec.data); break;
      case NodeKind::DocumentType: node = owner.create_document_type16(spec.name, spec.doctype_cloneable); break;
      default: throw domain::DomError(domain::DomErrorCode::NotSupported,"Node kind requires its dedicated factory");
    }
    *result = State().remember(Key(document.document,node),false);
  });
}
Status Model::Title(NodeKey document, DomString* result) const noexcept {
  return Protect([&] { DomString value = State().document(document).title16(); result->swap(value); });
}
Status Model::SetTitle(NodeKey document, DomStringView value) noexcept {
  return Protect([&] {
    State().document(document).set_title16(value);
    State().Touch();
  });
}
Status Model::SetFocusedElement(NodeKey document, NodeKey element) noexcept {
  return Protect([&] {
    State().document(document).set_focused(element.node);
    State().TouchStyle();
  });
}
Status Model::FocusedElement(NodeKey document, NodeKey* element) const noexcept {
  return Protect([&] {
    *element = NodeKey{document.document, State().document(document).focused_id()};
  });
}
Status Model::Release(NodeKey node) noexcept {
  return Protect([&] {
    auto found = State().documents.find(node.document);
    if (found == State().documents.end() || !found->second->get_node(node.node)) return;
    found->second->remove_node(node.node);
    State().Touch();
  });
}
Status Model::Subtree(NodeKey root, std::vector<NodeKey>* result) const noexcept {
  return Protect([&] {
    auto found = State().documents.find(root.document);
    if (found == State().documents.end() || !found->second->get_node(root.node)) return;
    std::vector<NodeKey> keys{root};
    for (domain::NodeId id : found->second->owned_descendants(root.node, false))
      keys.push_back(Key(root.document, id));
    result->swap(keys);
  });
}
Status Model::Root(NodeKey node, NodeKey* result, bool* connected) const noexcept {
  return Protect([&] {
    const auto& owner = State().owner(node);
    owner.require(node.node);
    const domain::NodeId top = owner.composed_root_id(node.node);
    *result = Key(node.document, top);
    *connected = top == owner.id();
  });
}
Status Model::CreateFragment(NodeKey document, Draft* result) noexcept {
  return Protect([&] {
    auto& owner = State().document(document);
    State().budget(1,0);
    *result = State().remember(Key(document.document,owner.create_fragment()),false);
  });
}
void Model::Publish(DraftId draft) noexcept { if (impl_) State().drafts.erase(draft); }
uint64_t Model::Revision() const noexcept { return impl_ ? impl_->revision : 0; }
uint64_t Model::StructureRevision() const noexcept { return impl_ ? impl_->structure : 0; }
uint64_t Model::TreeRevision() const noexcept { return impl_ ? impl_->tree : 0; }
uint64_t Model::ClassRevision() const noexcept { return impl_ ? impl_->classes : 0; }
void Model::Rollback(DraftId draft) noexcept {
  if (!impl_) return;
  auto found = State().drafts.find(draft); if (found == State().drafts.end()) return;
  auto pending = found->second;
  if (pending.document) State().documents.erase(pending.root.document);
  else {
    auto owner = State().documents.find(pending.root.document);
    if (owner != State().documents.end()) owner->second->remove_node(pending.root.node);
  }
  State().drafts.erase(found);
}
}

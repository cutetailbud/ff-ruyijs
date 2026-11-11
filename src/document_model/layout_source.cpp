#include "document_model/state.h"
#include "document_model/layout_source.h"
#include "js/Realm.h"

namespace ruyijs::document_model {
bool Realm::ReadLayoutNode(JS::HandleValue value, LayoutNodeSource* out,
    JS::MutableHandleObject parent, JS::MutableHandleObjectVector children) {
  NodeSource source;
  if (!ResolveNodeSource(value, &source)) return false;
  State& owner = *source.owner->state_;
  LayoutNodeSource result;
  std::vector<NodeKey> keys;
  NodeKey parentKey;
  {
    JSAutoRealm realm(owner.cx, owner.page);
    NodeSnapshot node;
    if (!owner.Check(owner.model.InspectLayout(source.key, &node))) return false;
    result.element = node.kind == NodeKind::Element;
    result.text = node.kind == NodeKind::Text;
    result.tag = node.local_name.value_or(u"");
    result.data = node.node_value.value_or(u"");
    result.factory = source.factory->geometryFactory_;
    result.layoutOwner = source.owner->geometryFactory_;
    result.renderedDocument = node.is_connected && node.composed_root.document == owner.main.document &&
        node.composed_root.node == owner.main.node;
    parentKey = node.parent;
    result.key = source.key;
    result.revision = owner.model.Revision();
    result.structure = owner.model.StructureRevision();
    result.tree = owner.model.TreeRevision();
    if (!owner.Check(owner.model.FocusedElement(owner.main, &result.focused))) return false;
    if (parentKey.node) {
      NodeSnapshot p;
      if (!owner.Check(owner.model.InspectLayout(parentKey, &p))) return false;
      if (p.shadow_host.node) parentKey = p.shadow_host;
      else if (p.kind != NodeKind::Element) parentKey = {};
    }
    if (result.element && !owner.Check(owner.model.Attributes(source.key, &result.attributes))) return false;
    // A host lays out its shadow children; the original light tree stays intact.
    const auto container = node.shadow_root.node ? node.shadow_root : source.key;
    if (!owner.Check(owner.model.Children(container, &keys))) return false;
  }
  parent.set(nullptr);
  if (parentKey.node && !source.owner->WrapNode(parentKey, parent)) return false;
  children.clear();
  for (const auto key : keys) {
    NodeSnapshot child;
    if (!owner.Check(owner.model.InspectLayout(key, &child))) return false;
    if (child.kind != NodeKind::Element && child.kind != NodeKind::Text) continue;
    JS::RootedObject object(owner.cx);
    if (!source.owner->WrapNode(key, &object) || !children.append(object)) return false;
  }
  *out = std::move(result);
  return true;
}
bool Realm::ReadLayoutHead(JS::HandleValue value, LayoutHeadSource* out, JS::MutableHandleObject root) {
  NodeSource source;
  if (!ResolveNodeSource(value, &source)) return false;
  State& owner = *source.owner->state_;
  LayoutHeadSource result;
  result.factory = source.factory->geometryFactory_;
  result.layoutOwner = source.owner->geometryFactory_;
  {
    JSAutoRealm realm(owner.cx, owner.page);
    NodeKey composed;
    bool connected = false;
    if (!owner.Check(owner.model.InspectLayoutRoot(
            source.key, &result.root, &composed, &connected, &result.ancestorBudgetExceeded)))
      return false;
    result.key = source.key;
    result.renderedDocument = connected && composed.document == owner.main.document &&
        composed.node == owner.main.node;
    result.revision = owner.model.Revision();
    result.structure = owner.model.StructureRevision();
    result.tree = owner.model.TreeRevision();
    if (!owner.Check(owner.model.FocusedElement(owner.main, &result.focused))) return false;
  }
  root.set(nullptr);
  if (result.renderedDocument && !result.ancestorBudgetExceeded && result.root.node &&
      !source.owner->WrapNode(result.root, root))
    return false;
  *out = result;
  return true;
}
bool Realm::ReadChangedElements(JS::HandleValue value, const std::vector<NodeKey>& keys,
                                const std::vector<char>& elements,
                                const std::vector<uint64_t>& previous_attributes,
                                const std::vector<DomString>& previous_styles, bool style_only,
                                std::vector<ChangedElement>* changed) {
  NodeSource source;
  if (!ResolveNodeSource(value, &source)) return false;
  State& owner = *source.owner->state_;
  JSAutoRealm realm(owner.cx, owner.page);
  return owner.Check(owner.model.ChangedElements(
      keys, elements, previous_attributes, previous_styles, style_only, changed));
}
bool Realm::WrapLayoutNode(JS::HandleValue value, NodeKey key, JS::MutableHandleObject out) {
  NodeSource source;
  if (!ResolveNodeSource(value, &source)) return false;
  return source.owner->WrapNode(key, out);
}
}

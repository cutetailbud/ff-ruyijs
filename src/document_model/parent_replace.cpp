// ParentNode's convert-nodes-into-a-node and replace-all use the original node
// factories and adoption publication. No JavaScript member dispatch is involved.
#include "document_model/foreign_adoption.h"
#include "document_model/range_removal.h"
#include "document_model/parent_removal.h"
#include "js/Realm.h"

namespace ruyijs::document_model {
namespace {
bool Place(Wrapper& parent, Wrapper& child, bool replace) {
  State& target = *parent.owner;
  JSAutoRealm realm(target.cx, target.page);
  if (!target.Check(target.model.CheckChildInsertion(parent.key, child.owner->model, child.key)))
    return false;
  NodeSnapshot original;
  if (!child.owner->Check(child.owner->model.Inspect(child.key, &original))) return false;
  ParentRemovalPlan removal;
  if (original.kind == NodeKind::DocumentFragment) {
    if (!removal.Add(*child.owner, child.key)) return false;
  } else if (original.parent.node &&
      (!replace || child.owner != &target || !Same(original.parent, parent.key))) {
    if (!removal.Add(*child.owner, original.parent, child.key)) return false;
  }
  if (replace && !removal.Add(target, parent.key)) return false;
  ChildListChange change;
  if (child.owner == &target && child.key.document == parent.key.document) {
    if (!target.Check(target.model.PrepareChildListChange(parent.key, child.key, replace, &change))) return false;
    if (!removal.Commit()) return false;
    return target.Check(target.model.CommitChildListChange(change));
  }
  NodeSnapshot destination;
  if (!target.Check(target.model.Inspect(parent.key, &destination))) return false;
  const NodeKey document = destination.kind == NodeKind::Document ? parent.key : destination.owner_document;
  ForeignAdoption transfer(target);
  if (!transfer.Prepare(child, document, original.kind == NodeKind::DocumentFragment) ||
      !target.Check(target.model.PrepareChildListChange(parent.key, transfer.root(), replace, &change)))
    return false;
  {
    AdoptionPublication publishing(target);
    if (!removal.Commit()) return false;
    if (!transfer.Commit() || !target.Check(target.model.CommitChildListChange(change))) return false;
  }
  target.FinishAdoptionPublication();
  return target.Check({});
}

bool Text(State& state, NodeKey document, DomStringView value, JS::MutableHandleValue result) {
  Draft draft;
  return state.Check(state.model.Create(document, {NodeKind::Text, {}, value}, &draft)) &&
         state.Commit(draft, result);
}
}

bool ReplaceNodeChildren(Wrapper& receiver, const JS::CallArgs& args) {
  JSContext* cx = receiver.owner->cx;
  std::vector<Item> items;
  std::vector<Wrapper*> nodes;
  if (!ConvertNodeItems(cx, args, &items, &nodes)) return false;
  // A conversion can adopt receiver or an earlier argument. Read their current
  // owner/key only after every WebIDL DOMString conversion has finished.
  State& state = *receiver.owner;
  JSAutoRealm realm(cx, state.page);
  NodeSnapshot target;
  if (!state.Check(state.model.Inspect(receiver.key, &target))) return false;
  const NodeKey document = target.kind == NodeKind::Document ? receiver.key : target.owner_document;
  JS::RootedValue value(cx);
  if (items.size() == 1) {
    if (nodes[0]) return Place(receiver, *nodes[0], true);
    if (!Text(state, document, items[0].text, &value)) return false;
  } else {
    Draft draft;
    if (!state.Check(state.model.CreateFragment(document, &draft)) || !state.Commit(draft, &value))
      return false;
    Wrapper* fragment = NodeReceiver(cx, value);
    if (!fragment) return false;
    for (size_t i = 0; i < items.size(); ++i) {
      JS::RootedValue text(cx);
      Wrapper* node = nodes[i];
      if (!node) {
        if (!Text(state, document, items[i].text, &text)) return false;
        node = NodeReceiver(cx, text);
      }
      if (!node || !Place(*fragment, *node, false)) return false;
    }
  }
  Wrapper* node = NodeReceiver(cx, value);
  return node && Place(receiver, *node, true);
}
}

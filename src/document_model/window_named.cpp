// Read-only document port for the existing WindowProperties factory.
#include "document_model/state.h"
#include "document_model/collections.h"
#include "browser_environment/environment.h"
#include "js/Realm.h"
#include "js/Wrapper.h"

namespace ruyijs::document_model {
namespace {
bool Resolve(State& state, DomStringView name, JS::MutableHandleValue result) {
  result.setUndefined();
  if (name.empty()) return true;
  std::vector<NodeKey> nodes;
  // Child contexts take precedence over elements: the first child navigable in
  // tree order whose browsing context name (its window.name) is |name|. Walk
  // the original tree for ordering and exclude shadow trees; FrameLink
  // supplies the existing identity, and the WindowProxy is the one
  // contentWindow returns (a cross-origin child is its cross-origin view).
  if (!state.Check(state.model.Query(state.main, QueryKind::TagName, u"iframe", &nodes))) return false;
  for (const auto key : nodes) {
    for (auto* link = state.frames.get(); link; link = link->next.get()) {
      if (!Same(link->frame, key)) continue;
      JS::RootedObject window(state.cx, link->window);
      JS::RootedString own(state.cx);
      if (!browser_environment::Environment::BrowsingContextName(state.cx, window, &own)) return false;
      if (!own) break;
      JS::RootedValue text(state.cx, JS::StringValue(own));
      DomString value;
      if (!String(state.cx, text, &value)) return false;
      if (DomStringView(value) != name) break;
      return browser_environment::Environment::WindowView(state.cx, window, result);
    }
  }
  DocumentSnapshot document;
  if (!state.Check(state.model.InspectDocument(state.main, &document))) return false;
  if (document.mode != DocumentMode::Html) return true;
  if (!state.Check(state.model.Query(state.main, QueryKind::Id, name, &nodes))) return false;
  if (!nodes.empty()) return state.Wrap(nodes.front(), result);
  if (!state.Check(state.model.WindowNamed(state.main, name, &nodes))) return false;
  if (nodes.empty()) return true;
  if (nodes.size() == 1) return state.Wrap(nodes.front(), result);
  auto* collection = state.FindCollection(CollectionKind::HTMLCollection, state.main,
                                           CollectionFilter::WindowNamed, name);
  if (!collection) collection = state.NewCollection(CollectionKind::HTMLCollection, state.main,
                                                     CollectionFilter::WindowNamed, name, true);
  if (!collection || !state.SyncCollections()) return false;
  result.setObject(*collection->object);
  return true;
}
}
bool Realm::ResolveWindowName(JS::HandleString name, JS::MutableHandleValue result) {
  State& state = *state_;
  JS::RootedValue value(state.cx, JS::StringValue(name));
  DomString text;
  if (!String(state.cx, value, &text)) return false;
  {
    JSAutoRealm realm(state.cx, state.page);
    if (!Resolve(state, text, &value)) return false;
  }
  if (!JS_WrapValue(state.cx, &value)) return false;
  result.set(value);
  return true;
}
}

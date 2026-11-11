// Document Model side of the style-source port: it reads the unique DOM tree,
// never a copy, so getComputedStyle and the cascade see exactly the element and
// <style> sources the parser built.
#include "document_model/realm.h"

#include "document_model/state.h"
#include "document_model/style_source_values.h"
#include "js/Realm.h"
#include "js/Object.h"
#include "js/Wrapper.h"

namespace ruyijs::document_model {
namespace {
bool CollectSources(State& state, NodeKey scope, std::vector<StyleSheetSource>* out) {
  NodeSnapshot snapshot;
  if (!state.Check(state.model.Inspect(scope, &snapshot))) return false;
  const bool shadow = snapshot.shadow_host.node != 0;
  if (shadow && !snapshot.is_connected) return true;
  std::vector<NodeKey> styles;
  if (!state.Check(state.model.SheetElements(scope, &styles))) return false;
  for (NodeKey key : styles) {
    NodeSnapshot node;
    if (!state.Check(state.model.Inspect(key, &node))) return false;
    if (std::string(node.interface_name) == "HTMLLinkElement") {
      StyleSheetSource source;
      if (!ReadLinkStyleSource(state, key, scope, node, &source)) return false;
      out->push_back(std::move(source));
      continue;
    }
    if (!node.local_name || *node.local_name != u"style") continue;
    std::optional<DomString> type, title, media, text;
    if (!state.Check(state.model.GetAttribute(key, u"type", &type)) ||
        !state.Check(state.model.GetAttribute(key, u"title", &title)) ||
        !state.Check(state.model.GetAttribute(key, u"media", &media)) ||
        !state.Check(state.model.TextContent(key, &text))) return false;
    std::string mime = StyleSourceText(type ? DomStringView(*type) : DomStringView());
    for (char& c : mime) if (c >= 'A' && c <= 'Z') c += 'a' - 'A';
    if (!mime.empty() && mime != "text/css") continue;
    StyleSheetSource source;
    source.document = key.document;source.key = key.node;
    source.scopeDocument = scope.document;source.scopeNode = scope.node;
    source.mainDocument = Same(scope, state.main);
    source.text = StyleSourceText(text ? DomStringView(*text) : DomStringView());
    source.titleAttribute = StyleSourceText(title ? DomStringView(*title) : DomStringView());
    source.typeAttribute = StyleSourceText(type ? DomStringView(*type) : DomStringView());
    source.title = shadow ? std::string() : StyleSourceTitle(source.titleAttribute);
    source.media = StyleSourceText(media ? DomStringView(*media) : DomStringView());
    out->push_back(std::move(source));
  }
  return true;
}
bool ReadSources(State& state, NodeKey scope, std::vector<StyleSheetSource>* out) {
  std::string url;
  if (!StyleDocumentURL(state, scope, &url)) return false;
  const uint64_t revision = state.model.StructureRevision();
  auto& memo = state.styleSourceMemo;
  for (const auto& entry : memo)
    if (Same(entry.scope, scope) && Same(entry.main, state.main) && entry.revision == revision &&
        entry.url == url) {
      out->insert(out->end(), entry.sources.begin(), entry.sources.end());
      return true;
    }
  std::vector<StyleSheetSource> sources;
  if (!CollectSources(state, scope, &sources)) return false;
  if (memo.size() >= 8) memo.erase(memo.begin());
  memo.push_back({scope, state.main, revision, url, sources});
  out->insert(out->end(), sources.begin(), sources.end());
  return true;
}
}  // namespace

bool Realm::ReadStyleChain(JS::HandleObject element, std::vector<StyleFrame>* chain,
                           bool* is_element) {
  *is_element = false;
  JS::RootedObject receiver(state_->cx, js::CheckedUnwrapStatic(element));
  if (!receiver || JS::GetClass(receiver) != &kNodeClass) return true;
  auto* entry = static_cast<Wrapper*>(JS::GetReservedSlot(receiver, 0).toPrivate());
  if (!entry || !entry->owner || entry->dead) return true;
  State& state = *entry->owner;
  JSAutoRealm realm(state.cx, state.page);
  NodeSnapshot snapshot;
  if (!state.Check(state.model.Inspect(entry->key, &snapshot))) return false;
  if (snapshot.kind != NodeKind::Element) return true;
  *is_element = true;
  if (!chain) return true;  // Receiver-only check; relation input owns the actual cascade read.
  for (NodeKey current = entry->key; current.node;) {
    NodeSnapshot node;
    if (!state.Check(state.model.Inspect(current, &node))) return false;
    StyleFrame frame;
    frame.tag = StyleSourceText(node.local_name ? DomStringView(*node.local_name)
                                     : DomStringView(node.node_name));
    std::vector<AttributeSnapshot> attributes;
    if (!state.Check(state.model.Attributes(current, &attributes))) return false;
    for (const AttributeSnapshot& attribute : attributes) {
      std::string name = StyleSourceText(attribute.name);
      std::string value = StyleSourceText(attribute.value);
      if (name == "id") {
        frame.id = value;
      } else if (name == "class") {
        size_t index = 0;
        while (index < value.size()) {
          while (index < value.size() && StyleSourceSpace(value[index])) ++index;
          size_t start = index;
          while (index < value.size() && !StyleSourceSpace(value[index])) ++index;
          if (index > start) frame.classes.push_back(value.substr(start, index - start));
        }
      } else if (name == "style") {
        frame.inline_style = value;
      }
      frame.attributes.push_back({std::move(name), std::move(value)});
    }
    chain->push_back(std::move(frame));
    if (!node.parent_element.node) break;
    current = node.parent_element;
  }
  return true;
}

bool Realm::ReadStyleSheets(std::vector<StyleSheetSource>* out) {
  return ReadSources(*state_, state_->main, out);
}
bool Realm::ReadStyleScope(uint64_t document, uint64_t key, std::vector<StyleSheetSource>* out) {
  State& state = *state_;
  NodeKey node{document, key};
  NodeSnapshot snapshot;
  if (!state.Check(state.model.Inspect(node, &snapshot))) return false;
  return !snapshot.is_connected || ReadSources(state, snapshot.root, out);
}
bool Realm::ReadStyleNode(uint64_t document, uint64_t key, JS::MutableHandleValue out) {
  return state_->Wrap(NodeKey{document, key}, out);
}
bool Realm::ReadDocumentStyleSources(JS::HandleObject document, StyleSheetScope* scope,
                                      std::vector<StyleSheetSource>* out) {
  JSContext* cx = state_->cx;
  JS::RootedObject receiver(cx, js::CheckedUnwrapStatic(document));
  JS::RootedValue branded(cx, receiver ? JS::ObjectValue(*receiver) : JS::NullValue());
  Wrapper* entry = NodeReceiver(cx, branded);
  if (!entry) return false;
  State& state = *entry->owner;
  JSAutoRealm realm(cx, state.page);
  NodeSnapshot snapshot;
  if (!state.Check(state.model.Inspect(entry->key, &snapshot))) return false;
  if (snapshot.kind != NodeKind::Document && !snapshot.shadow_host.node)
    return TypeError(cx, "CSS list source requires a Document or ShadowRoot");
  scope->document = entry->key.document;scope->key = entry->key.node;
  return ReadSources(state, entry->key, out);
}
}  // namespace ruyijs::document_model

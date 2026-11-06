#include "document_model/state.h"
#include "runtime/web_kit.h"
#include "js/Object.h"
#include "js/Realm.h"
#include "js/Wrapper.h"

namespace ruyijs::document_model {
// Brand failure wording: Cloudflare Turnstile fingerprints the TypeError
// message of `Node.prototype.<m>.call(badThis)`, so emit Firefox's exact
// "'<m>' called on an object that does not implement interface Node." when the
// call args (hence the callee's member name) are available.
static void NodeBrandFail(JSContext* cx, const JS::CallArgs* args) {
  std::string text;
  if (args && WebKit::BrandText(cx, *args, "Node", &text)) { TypeError(cx, text.c_str()); return; }
  TypeError(cx, "Node receiver is incompatible");
}
Wrapper* NodeReceiver(JSContext* cx, JS::HandleValue value, const JS::CallArgs* args) {
  JSObject* object = value.isObject() ? js::CheckedUnwrapStatic(&value.toObject()) : nullptr;
  auto* entry = object && JS::GetClass(object) == &kNodeClass
      ? static_cast<Wrapper*>(JS::GetReservedSlot(object, 0).toPrivate()) : nullptr;
  if (entry && entry->owner && !entry->dead) return entry;
  NodeBrandFail(cx, args); return nullptr;
}
Wrapper* NodeReceiver(JSContext* cx, JS::HandleValue value, NodeKind expected) {
  Wrapper* entry = NodeReceiver(cx, value);
  if (!entry) return nullptr;
  State& owner = *entry->owner;
  NodeKind kind = NodeKind::Document;
  {
    JSAutoRealm realm(cx, owner.page);
    if (!owner.Check(owner.model.Kind(entry->key, &kind))) return nullptr;
  }
  // CharacterData's existing Text-kind check also accepts Comment nodes.
  if (kind == expected || (expected == NodeKind::Text &&
      (kind == NodeKind::Comment || kind == NodeKind::ProcessingInstruction))) return entry;
  TypeError(cx, "DOM interface receiver is incompatible"); return nullptr;
}
}

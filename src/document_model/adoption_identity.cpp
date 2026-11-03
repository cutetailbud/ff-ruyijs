// Move the original caches; keys never resolve through a second identity map.
#include "document_model/state.h"
#include "runtime/weak_object.h"
#include <algorithm>

namespace ruyijs::document_model {
namespace {
using Identities = std::vector<std::pair<NodeKey, NodeKey>>;
NodeKey Destination(const Identities& pairs, NodeKey key) {
  const auto found = std::lower_bound(pairs.begin(), pairs.end(), key,
      [](const auto& pair, NodeKey value) { return NodeKeyLess{}(pair.first, value); });
  return found != pairs.end() && Same(found->first, key) ? found->second : NodeKey{};
}
}

void State::TakeAdoptedIdentities(State& source, const Identities& pairs) {
  auto* wrapperSlot = &source.wrappers;
  while (*wrapperSlot) {
    Wrapper& entry = **wrapperSlot;
    const NodeKey key = Destination(pairs, entry.key);
    if (!key.node || entry.dead || !WeakPeek(entry.object)) { wrapperSlot = &entry.next; continue; }
    auto indexed = source.wrapperIndex.extract(entry.key);
    entry.key = key;
    entry.owner = this;
    entry.listed = false;
    indexed.key() = key;
    wrapperIndex.insert(std::move(indexed));
    auto moved = std::move(*wrapperSlot);
    *wrapperSlot = std::move(moved->next);
    moved->next = std::move(wrappers);
    wrappers = std::move(moved);
    entry.listed = true;
  }
  auto* reflectedSlot = &source.reflected;
  while (*reflectedSlot) {
    ReflectedLink& entry = **reflectedSlot;
    const NodeKey key = Destination(pairs, entry.key);
    if (!key.node) { reflectedSlot = &entry.next; continue; }
    entry.key = key;
    auto moved = std::move(*reflectedSlot);
    *reflectedSlot = std::move(moved->next);
    moved->next = std::move(reflected);
    reflected = std::move(moved);
  }
  auto* collectionSlot = &source.collections;
  while (*collectionSlot) {
    Collection& entry = **collectionSlot;
    auto& record = *entry.record;
    const NodeKey key = Destination(pairs, record.scope);
    if (!key.node) { collectionSlot = &entry.next; continue; }
    record.scope = key;
    record.dataOwner = this;
    record.revision = 0;
    for (NodeKey& item : record.items) {
      NodeKey target = Destination(pairs, item);
      if (target.node) item = target;
    }
    auto moved = std::move(*collectionSlot);
    *collectionSlot = std::move(moved->next);
    moved->next = std::move(collections);
    collections = std::move(moved);
  }
  for (auto* record = source.tokenViews; record;) {
    auto* next = record->weakNext;
    const NodeKey key = Destination(pairs, record->scope);
    if (key.node) {
      UnlinkTokenView(*record);
      record->scope = key;
      record->dataOwner = record->weakOwner = this;
      record->revision = 0;
      record->weakNext = tokenViews;
      if (tokenViews) tokenViews->weakPrevious = record;
      tokenViews = record;
    }
    record = next;
  }
  for (const auto& pair : pairs) source.detached.erase(pair.first);
  source.syncedRevision = syncedRevision = 0;
}

void State::FinishAdoptionPublication() {
  FlushMutationNotification();
  auto scopes = std::move(deferredStyleScopes);
  deferredStyleScopes.clear();
  for (NodeKey scope : scopes) SyncMembership(scope, false);
  FlushFrameLifecycle();
  FlushScriptChanges();
  FlushCustomElementChanges(*this);
}
}  // namespace ruyijs::document_model

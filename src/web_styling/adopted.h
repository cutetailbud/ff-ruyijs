/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
// ObservableArray trap algorithms adapted from Firefox's binding handler;
// the original Styling owner supplies the unique backing and sheet factory.
#pragma once
#include "web_styling/cssom.h"
#include "js/Wrapper.h"
#include "js/PropertyDescriptor.h"
namespace ruyijs::domain::styling { class StyleSheetAggregate; }

namespace ruyijs::web_styling {
enum AdoptedSlot { AdoptedOwner, AdoptedScope, AdoptedDocument, AdoptedRows };
extern const JSClass kAdoptedClass;
inline State& AdoptedOwnerOf(JS::HandleObject proxy) {
  return *static_cast<State*>(js::GetProxyReservedSlot(proxy, AdoptedOwner).toPrivate());
}
bool AdoptedKey(JSContext*, JS::HandleId, uint32_t*, bool* indexed, bool* length);
bool AdoptedBacking(JSContext*, JS::HandleObject, JS::MutableHandleObject);
bool AdoptedLength(JSContext*, JS::HandleObject, uint32_t*);
bool AdoptedSetLength(JSContext*, JS::HandleObject, JS::HandleValue, JS::ObjectOpResult&);
bool AdoptedPut(JSContext*, JS::HandleObject, uint32_t, JS::HandleValue, JS::ObjectOpResult&);
bool NewAdoptedArray(State&, JS::HandleObject, JS::MutableHandleObject);
bool InstallAdoptedStyles(State&);
bool ReadCascadeSources(State&, JS::HandleObject,
    std::shared_ptr<const std::vector<ruyijs::domain::styling::StyleSheetAggregate>>*,
    std::shared_ptr<const SelectorDependencies>* dependencies,
    std::shared_ptr<ruyijs::domain::styling::RuleIndexStore>* indexes);
bool CascadeStamp(State&, JS::HandleObject, uint64_t*);
bool CascadeStamp(State&, JS::HandleObject, uint64_t*, bool includeEnvironment);

class AdoptedHandler final : public js::ForwardingProxyHandler {
 public:
  static const char family;
  constexpr AdoptedHandler() : ForwardingProxyHandler(&family) {}
  bool getOwnPropertyDescriptor(JSContext*, JS::HandleObject, JS::HandleId,
      JS::MutableHandle<mozilla::Maybe<JS::PropertyDescriptor>>) const override;
  bool defineProperty(JSContext*, JS::HandleObject, JS::HandleId,
      JS::Handle<JS::PropertyDescriptor>, JS::ObjectOpResult&) const override;
  bool ownPropertyKeys(JSContext*, JS::HandleObject, JS::MutableHandleIdVector) const override;
  bool delete_(JSContext*, JS::HandleObject, JS::HandleId, JS::ObjectOpResult&) const override;
  bool get(JSContext*, JS::HandleObject, JS::HandleValue, JS::HandleId,
      JS::MutableHandleValue) const override;
  bool set(JSContext*, JS::HandleObject, JS::HandleId, JS::HandleValue,
      JS::HandleValue, JS::ObjectOpResult&) const override;
  bool has(JSContext*, JS::HandleObject, JS::HandleId, bool*) const override;
  bool preventExtensions(JSContext*, JS::HandleObject, JS::ObjectOpResult&) const override;
};
}

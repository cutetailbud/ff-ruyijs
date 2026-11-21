#include "browser_environment/state.h"
#include "js/Array.h"
#include "js/PropertyAndElement.h"
#include "js/Realm.h"
#include "js/String.h"
#include "js/StructuredClone.h"

namespace ruyijs::browser_environment {
Environment::Environment(JSContext* cx, JS::HandleObject page, Environment& parent,
                         JS::HandleObject frameElement)
    : state_(new State(cx, page)) {
  State* owner = parent.state_;
  state_->shared = owner->shared ? owner->shared : owner;
  state_->topWindow = owner->topWindow;
  state_->parentWindow = owner->page;
  state_->frameElement = frameElement;
  state_->agentClusters = owner->agentClusters;
  // The initial about:blank document has its creator's origin and agent.
  state_->originKeyed = owner->originKeyed;
  // The frame's own WebGL, WebGPU, Media... owners read the creator's
  // configured groups when they bind (HTML: one Context, one hardware profile).
  state_->groupSource = owner;
}
// A later top-level document of the same traversable: a new Window over the
// first document's Model. The buffered profile groups are plain data the
// owners of the new realm read when they bind (State::InheritGroups).
Environment::Environment(JSContext* cx, JS::HandleObject page, Environment& previous,
                         Succession)
    : state_(new State(cx, page, &previous.state_->model)) {
  State& old = *previous.state_;
  state_->adopted = true;
  state_->groupSource = &old;
  state_->agentClusters = old.agentClusters;
}
// HTML "obtain a similar-origin window agent": an origin already in the
// group's map keeps its key; otherwise a secure document whose response
// carries the structured-field boolean ?1 makes its origin origin-keyed.
// An opaque origin is always origin-keyed.
bool Environment::CommitAgentCluster(const std::string& header) {
  OriginValue value;
  bool secure = false;
  if (!state_->ReadOriginValue(&value) || !state_->ReadSecureContext(&secure)) return false;
  if (value.kind == domain::browser_environment::OriginKind::Opaque) { state_->originKeyed = true; return true; }
  const std::string origin = value.tuple;
  auto& map = *state_->agentClusters;
  const auto found = map.find(origin);
  if (found != map.end()) { state_->originKeyed = found->second; return true; }
  size_t begin = 0, end = header.size();
  while (begin < end && (header[begin] == ' ' || header[begin] == '\t')) ++begin;
  while (end > begin && (header[end - 1] == ' ' || header[end - 1] == '\t')) --end;
  const bool requested = secure && header.compare(begin, end - begin, "?1") == 0;
  map.emplace(origin, requested);
  state_->originKeyed = requested;
  return true;
}
// The child realm (or a later top-level document) builds its own frozen
// languages array and applies the same locale and timezone to its Intl/Date;
// the values come from the one owner.
bool State::Inherit() {
  FieldValue list, language, timezone;
  const Model& source = shared ? shared->model : model;
  if (!source.Read(Kind::Navigator, 10, &list) ||
      !source.Read(Kind::Locale, 1, &language) ||
      !source.Read(Kind::Locale, 0, &timezone)) {
    JS_ReportOutOfMemory(cx);
    return false;
  }
  JSAutoRealm realm(cx, page);
  JS::RootedObject array(cx, JS::NewArrayObject(cx, list.strings.size()));
  if (!array) return false;
  for (size_t index = 0; index < list.strings.size(); ++index) {
    const auto& source = list.strings[index];
    JS::RootedString text(cx, JS_NewStringCopyUTF8N(cx, JS::UTF8Chars(source.data(), source.size())));
    if (!text || !JS_DefineElement(cx, array, index, text, JSPROP_ENUMERATE)) return false;
  }
  if (!JS_FreezeObject(cx, array)) return false;
  languages = array;
  JS::Realm* target = JS::GetObjectRealmOrNull(page);
  // xpc_LocalizeRuntime: the Intl default locale is the OS regional-prefs
  // locale; navigator.language only feeds Accept-Language. "" keeps the
  // engine's own default, which is the host OS locale.
  if (!language.text.empty()) JS::SetRealmLocaleOverride(target, language.text.c_str());
  if (!timezone.text.empty()) JS::SetRealmTimezoneOverride(target, timezone.text.c_str());
  const State* groups = groupSource;
  groupSource = nullptr;
  if (groups && !InheritGroups(*groups)) return false;
  ready = true;
  return true;
}
// Every buffered owner group of |source|, structured-cloned into this realm.
// Inherit() runs before any owner of this realm binds its port, so each owner
// receives the creator's (or predecessor's) configuration on binding, and a
// Worker this document starts exports the same groups (worker_groups.cpp).
// The clones live in this realm's compartment: no group points into another
// window's realm or keeps it alive after that document goes away.
bool State::InheritGroups(const State& source) {
  JSAutoRealm realm(cx, page);
  auto copy = [&](const JS::PersistentRootedObject& from, JS::PersistentRootedObject& to) {
    if (!from.get()) return true;
    JS::RootedValue value(cx, JS::ObjectValue(*from.get())), clone(cx);
    if (!JS_StructuredClone(cx, value, &clone, nullptr, nullptr)) return false;
    to = &clone.toObject();
    return true;
  };
  for (JS::PersistentRootedObject State::*group :
       {&State::webglGroup, &State::gpuGroup, &State::permissionsGroup, &State::geolocationGroup,
        &State::speechGroup, &State::mediaGroup})
    if (!copy(source.*group, this->*group)) return false;
  for (size_t index = 0; index < kOwnerGroupCount; ++index)
    if (!copy(source.owners[index].group, owners[index].group)) return false;
  return true;
}
}

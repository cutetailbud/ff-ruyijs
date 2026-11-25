#ifndef RUYIJS_WEB_PERMISSIONS_REALM_H
#define RUYIJS_WEB_PERMISSIONS_REALM_H
#include <memory>
#include "js/RootingAPI.h"
#include "js/TypeDecls.h"

namespace ruyijs { class PageHelpers; }
namespace ruyijs::interaction { class Realm; }
namespace ruyijs::web_permissions {
struct State;
// The Permissions owner of one Window realm (docs/owners/PERMISSIONS.md). The
// main context owns the one permission-state table; an iframe realm reads its
// parent's table, so the Context has a single state source.
class Realm final {
 public:
  Realm(JSContext* cx, JS::HandleObject page, PageHelpers& helpers,
        interaction::Realm& events, Realm* parent);
  ~Realm();
  Realm(const Realm&) = delete;
  Realm& operator=(const Realm&) = delete;
  // Installs Permissions and PermissionStatus, and appends the one
  // `permissions` accessor to the Navigator prototype browser_environment owns.
  bool Install(JS::HandleObject navigatorPrototype);
  // Browser Environment "permissions" group port: name -> granted|denied|prompt,
  // already validated by the environment schema.
  bool Configure(JS::HandleObject group);
  bool Describe(JS::MutableHandleValue result);
  // Read-only view of the one table for another owner's port (Geolocation):
  // 0 granted, 1 denied, 2 prompt, -1 for an unknown PermissionName.
  int StateOf(const char* name) const;
  // Scheduling port: queues one Notification event task on the Context's one
  // macrotask queue. Bound by the runtime coordinator with the window message ports.
  using TaskPort = bool (*)(void* context, JS::HandleObject task);
  void SetTaskPort(TaskPort port, void* context);
  // Frame port (bound by the runtime frame owner for an iframe document): the
  // inputs Firefox's PermissionDelegateHandler takes from the document's
  // position, namely whether it is cross-origin to the top-level document and
  // whether a Permissions Policy feature is allowed in it. Unbound, the realm
  // answers as a top-level document.
  using FramePolicyPort = bool (*)(void* context, const char* feature, bool* crossOriginToTop,
                                   bool* featureAllowed);
  void SetFramePolicyPort(FramePolicyPort port, void* context);
 private:
  std::unique_ptr<State> state_;
};
}
#endif

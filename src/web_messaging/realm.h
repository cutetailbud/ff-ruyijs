#ifndef RUYIJS_WEB_MESSAGING_REALM_H
#define RUYIJS_WEB_MESSAGING_REALM_H
#include <memory>
#include "js/RootingAPI.h"
namespace ruyijs { class PageHelpers; }
namespace ruyijs::interaction { class Realm; }
namespace ruyijs::web_messaging {
struct State;
// The unique MessageChannel/MessagePort owner in a Window or Worker realm.
class Realm final {
 public:
  Realm(JSContext*, JS::HandleObject, PageHelpers&, interaction::Realm&);
  ~Realm();
  bool Install();
  using TaskPort = bool (*)(void*, JS::HandleObject);
  void SetTaskPort(TaskPort, void*);
  void Retire();
  // SharedWorker ports (docs/owners/WORKER.md, "Shared workers"): a port whose
  // entangled peer lives in another process. A post on it calls `relay` with
  // the port id, the value and the transfer list (the owner encodes the frame);
  // messages the owner decoded arrive through DeliverRemote, and a peer close
  // through CloseRemote.
  using RemoteRelay = bool (*)(void* context, uint32_t id, JS::HandleValue value,
                               JS::HandleValue transfer);
  using RemoteClose = void (*)(void* context, uint32_t id);
  void SetRemoteRelay(RemoteRelay, void* context);
  void SetRemoteClose(RemoteClose, void* context);
  JSObject* NewRemotePort(uint32_t id);
  // A local entangled MessagePort pair for another owner's object that owns a
  // port by specification (AudioWorkletNode.port and its processor-side peer).
  // Same construction as MessageChannel; the ports are this realm's.
  bool NewChannel(JS::MutableHandleObject first, JS::MutableHandleObject second);
  bool DeliverRemote(uint32_t id, JS::HandleValue value);
  // A MessagePort in the transfer list of a message to another process: the
  // port object is detached, its endpoint stays entangled with its peer in this
  // realm and becomes the relay end of `id` (Firefox keeps the peer entangled
  // across the process boundary). False: not a transferable port.
  static bool IsPort(JSObject*);
  static bool BridgePort(JSObject*, uint32_t id);
  void CloseRemote(uint32_t id);
  // Existing Window clone/message adapters consume this transfer-aware path.
  bool Clone(JS::HandleValue, JS::HandleValue, JS::HandleObject destination,
             JS::MutableHandleValue, JS::MutableHandleObject ports);
 private:
  std::unique_ptr<State> state_;
};
}
#endif

#ifndef RUYIJS_WEB_MESSAGING_STATE_H
#define RUYIJS_WEB_MESSAGING_STATE_H
#include "web_messaging/realm.h"
#include "runtime/web_kit.h"
#include "js/StructuredClone.h"
#include <deque>
#include <vector>

namespace ruyijs::web_messaging {
struct Record;
struct Message;
// Endpoint identity survives transfer; wrappers and enabled state do not.
struct Endpoint {
  std::weak_ptr<Endpoint> peer;
  Record* receiver = nullptr;
  bool enabled = false, scheduled = false;
  std::deque<std::unique_ptr<Message>> pending;
  ~Endpoint();
};
struct Record {
  Record(JSContext* cx, State& owner, JS::HandleObject port,
         std::shared_ptr<Endpoint> endpoint)
      : owner(owner), port(cx, port), endpoint(std::move(endpoint)) {}
  State& owner;
  JS::PersistentRootedObject port;
  std::shared_ptr<Endpoint> endpoint;
  bool closed = false;
  // A SharedWorker port's peer lives in another process; remoteId identifies
  // the connection in both realms.
  bool remote = false;
  uint32_t remoteId = 0;
  // A port transferred to another process (a dedicated Worker or its parent):
  // this hidden record keeps the transferred endpoint, so what its local peer
  // posts is relayed as remoteId and what arrives for remoteId is posted to
  // that peer (Realm::BridgePort).
  bool bridge = false;
};
struct State {
  State(JSContext* cx, JS::HandleObject page, PageHelpers& helpers, interaction::Realm& events)
      : cx(cx), kit(cx, page, helpers, this), events(events), channel(cx), port(cx),
        exception(cx), add(cx), remove(cx), prevent(cx) {}
  JSContext* cx;
  WebKit kit;
  interaction::Realm& events;
  WebFace channel, port;
  JS::PersistentRootedObject exception;
  JS::PersistentRootedValue add, remove, prevent;
  std::vector<std::unique_ptr<Record>> records;
  Realm::TaskPort task = nullptr;
  void* taskContext = nullptr;
  Realm::RemoteRelay relay = nullptr;
  void* relayContext = nullptr;
  Realm::RemoteClose remoteClose = nullptr;
  void* remoteCloseContext = nullptr;
  bool active = true;
  State* previous = nullptr;
};
extern thread_local State* activeState;
enum PortSlot { PortRecord, OnMessage, OnMessageError, MessageHandler, ErrorHandler, PortSlots };
extern const JSClass kPortClass, kChannelClass;
Record* Receiver(JSContext*, JS::HandleValue);
Record* Unwrap(JSObject*);
JSObject* NewPort(State&, std::shared_ptr<Endpoint>);
bool Queue(const std::shared_ptr<Endpoint>&);
void Disconnect(Endpoint&);
bool CloneError(State&);
bool Construct(JSContext*, unsigned, JS::Value*);
bool GetPort(JSContext*, unsigned, JS::Value*);
bool Post(JSContext*, unsigned, JS::Value*);
bool Start(JSContext*, unsigned, JS::Value*);
bool Close(JSContext*, unsigned, JS::Value*);
bool GetHandler(JSContext*, unsigned, JS::Value*);
bool SetHandler(JSContext*, unsigned, JS::Value*);
bool TransferOptions(JSContext*, JS::HandleValue, JS::MutableHandleValue);
extern const JSStructuredCloneCallbacks cloneCallbacks;
struct Message {
  explicit Message(JSContext* cx)
      : ports(cx), buffer(JS::StructuredCloneScope::SameProcess, &cloneCallbacks, this) {}
  bool error = false;
  Endpoint* excluded = nullptr;
  Endpoint* recipient = nullptr;
  bool recipientTransferred = false;
  State* destination = nullptr;
  JS::PersistentRootedObject ports;
  // Destroy the buffer before its callback closure members.
  JSAutoStructuredCloneBuffer buffer;
  bool Write(State&, JS::HandleValue, JS::HandleValue);
  bool Read(State&, JS::MutableHandleValue);
};
}
#endif

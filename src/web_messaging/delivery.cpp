#include "web_messaging/state.h"
#include "interaction/realm.h"
#include "jsapi.h"
#include "jsfriendapi.h"
#include "js/Realm.h"
#include "js/Wrapper.h"

namespace ruyijs::web_messaging {
Endpoint::~Endpoint() = default;
void Disconnect(Endpoint& endpoint) {
  if (auto peer = endpoint.peer.lock()) peer->peer.reset();
  endpoint.peer.reset();
}
namespace {
bool Deliver(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  auto* record = static_cast<Record*>(js::GetFunctionNativeReserved(&args.callee(), 0).toPrivate());
  args.rval().setUndefined();
  // The old wrapper may have transferred while this task was waiting.
  auto endpoint = record->endpoint;
  if (!endpoint || !record->owner.active) return true;
  endpoint->scheduled = false;
  if (!endpoint->enabled || endpoint->pending.empty()) return true;
  auto message = std::move(endpoint->pending.front()); endpoint->pending.pop_front();
  State& s = record->owner;
  JS::RootedValue value(cx);
  if (!message->Read(s, &value)) return false;
  JS::RootedObject target(cx, record->port), source(cx), ports(cx, message->ports);
  if (record->bridge) {
    // The peer's message leaves for the other process with its own ports.
    if (!Queue(endpoint)) return false;
    if (!s.relay) return true;
    JS::RootedValue transfer(cx, JS::ObjectValue(*ports));
    return s.relay(s.relayContext, record->remoteId, value, transfer);
  }
  // Queue before dispatch: reentrant postMessage retains FIFO task order.
  if (!Queue(endpoint)) return false;
  return s.events.FireMessage(target, value, "", source, ports);
}
}
bool Queue(const std::shared_ptr<Endpoint>& endpoint) {
  if (endpoint->scheduled || !endpoint->enabled || endpoint->pending.empty() || !endpoint->receiver)
    return true;
  Record& record = *endpoint->receiver;
  State& s = record.owner;
  if (!s.active || !s.task) return true;
  JSAutoRealm realm(s.cx, s.kit.page);
  JS::RootedFunction fn(s.cx, js::NewFunctionWithReserved(s.cx, Deliver, 0, 0, "MessagePort delivery"));
  if (!fn) return false;
  JS::RootedObject task(s.cx, JS_GetFunctionObject(fn));
  js::SetFunctionNativeReserved(task, 0, JS::PrivateValue(&record));
  if (!s.task(s.taskContext, task)) return false;
  endpoint->scheduled = true;
  return true;
}
bool Start(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  Record* record = Receiver(cx, args.thisv());
  if (!record) return false;
  if (record->endpoint) {
    record->endpoint->enabled = true;
    if (!Queue(record->endpoint)) return false;
  }
  args.rval().setUndefined(); return true;
}
bool Close(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  Record* record = Receiver(cx, args.thisv());
  if (!record) return false;
  // Already queued messages survive close in Firefox; subsequent sends do not.
  record->closed = true;
  if (record->remote) {
    if (record->endpoint) record->endpoint->pending.clear();
    if (record->owner.remoteClose)
      record->owner.remoteClose(record->owner.remoteCloseContext, record->remoteId);
  } else if (record->endpoint) {
    Disconnect(*record->endpoint);
  }
  args.rval().setUndefined(); return true;
}
bool Post(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  Record* record = Receiver(cx, args.thisv());
  if (!record) return false;
  if (!argc) return WebKit::Fail(cx, "MessagePort.postMessage requires a message");
  if (record->remote) {
    if (record->closed) { args.rval().setUndefined(); return true; }
    JS::RootedValue transfer(cx);
    if (!TransferOptions(cx, args.get(1), &transfer)) return false;
    State& s = record->owner;
    if (!s.relay) return WebKit::Fail(cx, "MessagePort relay is not bound");
    return s.relay(s.relayContext, record->remoteId, args[0], transfer);
  }
  JS::RootedValue transfer(cx);
  if (!TransferOptions(cx, args.get(1), &transfer)) return false;
  // Serializing may invoke JS getters that close/transfer either endpoint.
  auto sender = record->endpoint;
  auto message = std::make_unique<Message>(cx);
  message->excluded = sender.get();
  auto recipient = sender ? sender->peer.lock() : nullptr;
  message->recipient = recipient.get();
  if (!message->Write(record->owner, args[0], transfer)) return false;
  if (sender && record->endpoint == sender && !message->recipientTransferred &&
      (recipient = sender->peer.lock()) && record->owner.active) {
    recipient->pending.push_back(std::move(message));
    if (!Queue(recipient)) return false;
  }
  args.rval().setUndefined(); return true;
}
void Realm::SetRemoteRelay(RemoteRelay relay, void* context) {
  state_->relay = relay;
  state_->relayContext = context;
}
void Realm::SetRemoteClose(RemoteClose close, void* context) {
  state_->remoteClose = close;
  state_->remoteCloseContext = context;
}
JSObject* Realm::NewRemotePort(uint32_t id) {
  State& s = *state_;
  auto endpoint = std::make_shared<Endpoint>();
  JS::RootedObject port(s.cx, NewPort(s, std::move(endpoint)));
  if (!port) return nullptr;
  for (auto& record : s.records)
    if (record->port.get() == port.get()) { record->remote = true; record->remoteId = id; break; }
  return port;
}
bool Realm::NewChannel(JS::MutableHandleObject first, JS::MutableHandleObject second) {
  State& s = *state_;
  JSAutoRealm realm(s.cx, s.kit.page);
  auto one = std::make_shared<Endpoint>(), two = std::make_shared<Endpoint>();
  one->peer = two; two->peer = one;
  first.set(NewPort(s, one));
  if (!first) return false;
  second.set(NewPort(s, two));
  return second != nullptr;
}
bool Realm::DeliverRemote(uint32_t id, JS::HandleValue value) {
  State& s = *state_;
  for (auto& record : s.records) {
    if (!record->remote || record->remoteId != id || !record->endpoint) continue;
    if (record->closed) return true;
    JSAutoRealm realm(s.cx, s.kit.page);
    JS::RootedValue wrapped(s.cx, value);
    if (!JS_WrapValue(s.cx, &wrapped)) return false;
    if (record->bridge) {
      // From the other process to the transferred port's local peer.
      auto peer = record->endpoint->peer.lock();
      if (!peer) return true;
      auto message = std::make_unique<Message>(s.cx);
      message->excluded = record->endpoint.get();
      message->recipient = peer.get();
      if (!message->Write(s, wrapped, JS::UndefinedHandleValue)) return false;
      peer->pending.push_back(std::move(message));
      return Queue(peer);
    }
    auto message = std::make_unique<Message>(s.cx);
    message->excluded = record->endpoint.get();
    message->recipient = record->endpoint.get();
    if (!message->Write(s, wrapped, JS::UndefinedHandleValue)) return false;
    record->endpoint->pending.push_back(std::move(message));
    return Queue(record->endpoint);
  }
  return true;  // an unknown or already-closed port drops the message
}
void Realm::CloseRemote(uint32_t id) {
  State& s = *state_;
  for (auto& record : s.records) {
    if (!record->remote || record->remoteId != id || !record->endpoint) continue;
    record->closed = true;
    record->endpoint->pending.clear();
    if (record->bridge) Disconnect(*record->endpoint);
    return;
  }
}
bool Realm::IsPort(JSObject* object) {
  Record* record = object ? Unwrap(object) : nullptr;
  return record && !record->closed && record->endpoint && !record->remote;
}
bool Realm::BridgePort(JSObject* object, uint32_t id) {
  Record* record = object ? Unwrap(object) : nullptr;
  if (!record || record->closed || !record->endpoint || record->remote) return false;
  State& s = record->owner;
  // The transferred object is detached; a hidden record of the same realm
  // receives what the peer posts and relays it.
  std::shared_ptr<Endpoint> endpoint = std::move(record->endpoint);
  endpoint->receiver = nullptr;
  endpoint->scheduled = false;
  JSAutoRealm realm(s.cx, s.kit.page);
  JS::RootedObject hidden(s.cx, NewPort(s, endpoint));
  if (!hidden) return false;
  Record& bridge = *s.records.back();
  bridge.remote = bridge.bridge = true;
  bridge.remoteId = id;
  endpoint->enabled = true;
  return Queue(endpoint);
}
}

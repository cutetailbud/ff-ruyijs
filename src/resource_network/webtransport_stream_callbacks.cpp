#include "resource_network/webtransport.h"
#include "resource_network/native_endpoints.h"
#include "resource_network/writable_stream.h"
#include "js/ArrayBuffer.h"
#include "js/Promise.h"
#include "js/Exception.h"
#include "js/experimental/TypedData.h"
#include <cstring>

namespace ruyijs::resource_network {
namespace {
CallbackResult SourceStart(JSContext*,JS::HandleObject,JS::HandleObject,JS::HandleObject,JS::HandleValue,JS::MutableHandleValue out) {
  out.setUndefined();return CallbackResult::Success;
}
CallbackResult SourcePull(JSContext* cx,JS::HandleObject endpoint,JS::HandleObject,JS::HandleObject,JS::HandleValue,JS::MutableHandleValue out) {
  return CallbackOutcome(TransportGuard(cx,[&] {
    JS::RootedObject transport(cx,SlotObject(endpoint,WESession)),promise(cx,JS::NewPromiseObject(cx,nullptr));if (!promise) return false;
    JS::SetReservedSlot(endpoint,WEReadPending,JS::ObjectValue(*promise));out.setObject(*promise);
    if (EndpointKind(JS::GetReservedSlot(endpoint,WEKind).toInt32())!=EndpointKind::ByteStream) return true;
    WebTransportCommand command;command.action=WebTransportAction::Read;command.stream=uint64_t(JS::GetReservedSlot(endpoint,WEStream).toNumber());
    return TransportSubmit(cx,transport,command,promise,endpoint,JS::UndefinedHandleValue,JS::NullHandleValue,JS::UndefinedHandleValue);
  }));
}
CallbackResult SourceCancel(JSContext* cx,JS::HandleObject endpoint,JS::HandleObject,JS::HandleObject,JS::HandleValue,JS::MutableHandleValue out) {
  return CallbackOutcome(TransportGuard(cx,[&] {
    JS::RootedObject transport(cx,SlotObject(endpoint,WESession)),promise(cx,JS::NewPromiseObject(cx,nullptr));if (!promise) return false;out.setObject(*promise);
    if (EndpointKind(JS::GetReservedSlot(endpoint,WEKind).toInt32())!=EndpointKind::ByteStream)
      return JS::ResolvePromise(cx,promise,JS::UndefinedHandleValue);
    WebTransportCommand command;command.action=WebTransportAction::StopSending;command.stream=uint64_t(JS::GetReservedSlot(endpoint,WEStream).toNumber());
    return TransportSubmit(cx,transport,command,promise,endpoint,JS::UndefinedHandleValue,JS::NullHandleValue,JS::UndefinedHandleValue);
  }));
}
CallbackResult SinkStart(JSContext*,JS::HandleObject,JS::HandleObject,JS::HandleValue,JS::MutableHandleValue out,CallbackContext) {
  out.setUndefined();return CallbackResult::Success;
}
CallbackResult Send(JSContext* cx,JS::HandleObject endpoint,JS::HandleValue chunk,JS::MutableHandleValue out,WebTransportAction action) {
  return CallbackOutcome(TransportGuard(cx,[&] {
    JS::RootedObject transport(cx,SlotObject(endpoint,WESession)),promise(cx,JS::NewPromiseObject(cx,nullptr)),buffer(cx);if (!promise) return false;out.setObject(*promise);
    WebTransportCommand command;command.action=action;command.stream=uint64_t(JS::GetReservedSlot(endpoint,WEStream).toNumber());
    const bool datagram=EndpointKind(JS::GetReservedSlot(endpoint,WEKind).toInt32())==EndpointKind::Datagram;
    if(action==WebTransportAction::Reset && !datagram) {
      State* state=TransportState(cx,transport);if(!state)return false;
      command.code=TransportStreamErrorCode(*state,chunk);
    }
    if (action==WebTransportAction::Write) {
      if (!TransportBytes(cx,chunk,&command.data)) {
        if(datagram){JS_ClearPendingException(cx);Error(cx,"Wrong type for Datagram stream write");}
        return TransportRejectPending(cx,out);
      }
      if(datagram) {
        JS::RootedObject duplex(cx,SlotObject(transport,WTDatagrams));
        if(command.data.size()>JS::GetReservedSlot(duplex,WDMaxSize).toNumber())return JS::ResolvePromise(cx,promise,JS::UndefinedHandleValue);
      }
      if (datagram) command.action=WebTransportAction::Datagram;
      buffer=JS_NewUint8Array(cx,command.data.size());if (!buffer) return false;
      {JS::AutoCheckCannotGC nogc;bool shared=false;if (!command.data.empty())std::memcpy(JS_GetUint8ArrayData(buffer,&shared,nogc),command.data.data(),command.data.size());}
    } else if (datagram) return JS::ResolvePromise(cx,promise,JS::UndefinedHandleValue);
    JS::RootedValue data(cx,JS::ObjectOrNullValue(buffer)),group(cx,JS::GetReservedSlot(endpoint,WEGroup)),order(cx,JS::GetReservedSlot(endpoint,WEOrder));
    return TransportSubmit(cx,transport,command,promise,endpoint,data,group,order);
  }));
}
CallbackResult Write(JSContext* cx,JS::HandleObject ep,JS::HandleObject,JS::HandleValue chunk,JS::MutableHandleValue out,CallbackContext) {return Send(cx,ep,chunk,out,WebTransportAction::Write);}
CallbackResult Close(JSContext* cx,JS::HandleObject ep,JS::HandleObject,JS::HandleValue chunk,JS::MutableHandleValue out,CallbackContext) {return Send(cx,ep,chunk,out,WebTransportAction::Finish);}
CallbackResult Abort(JSContext* cx,JS::HandleObject ep,JS::HandleObject,JS::HandleValue chunk,JS::MutableHandleValue out,CallbackContext) {return Send(cx,ep,chunk,out,WebTransportAction::Reset);}
const NativeSourceSteps kSource={SourceStart,SourcePull,SourceCancel};
const NativeSinkSteps kSink={SinkStart,Write,Close,Abort};
}
bool TransportReadStream(State& s,JS::HandleObject endpoint,bool bytes,JS::MutableHandleObject stream) {
  JS::RootedObject controller(s.cx),prototype(s.cx);
  if (EndpointKind(JS::GetReservedSlot(endpoint,WEKind).toInt32())==EndpointKind::ByteStream) prototype=s.transportReceive.prototype;
  if (!NewNativeReadableStream(s,endpoint,kSource,bytes,0,prototype,stream,&controller)) return false;
  JS::SetReservedSlot(endpoint,WEReadable,JS::ObjectValue(*stream));JS::SetReservedSlot(endpoint,WEReadController,JS::ObjectValue(*controller));return true;
}
bool TransportWriteStream(State& s,JS::HandleObject endpoint,bool datagram,JS::MutableHandleObject stream) {
  JS::RootedObject transport(s.cx,SlotObject(endpoint,WESession)),duplex(s.cx,SlotObject(transport,WTDatagrams));
  const bool defaultDatagram=datagram&&!duplex;
  JS::RootedObject controller(s.cx),prototype(s.cx,defaultDatagram?s.writableStream.prototype.get():datagram?s.transportDatagramsWritable.prototype.get():s.transportSend.prototype.get());
  const double hwm=datagram&&duplex?JS::GetReservedSlot(duplex,WDOutgoingBuffered).toNumber():1;
  if (!NewNativeWritableStream(s,endpoint,kSink,hwm,prototype,stream,&controller)) return false;
  JS::SetReservedSlot(endpoint,WEWritable,JS::ObjectValue(*stream));JS::SetReservedSlot(endpoint,WEWriteController,JS::ObjectValue(*controller));return true;
}
}

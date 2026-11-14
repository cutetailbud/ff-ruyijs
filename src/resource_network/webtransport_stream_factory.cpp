#include "resource_network/webtransport.h"
#include "js/Array.h"
#include "js/PropertyAndElement.h"
#include <limits>

namespace ruyijs::resource_network {
bool TransportNewEndpoint(State& s,JS::HandleObject transport,uint64_t id,EndpointKind kind,JS::MutableHandleObject result) {
  result.set(JS_NewObjectWithGivenProto(s.cx,&kTransportEndpointClass,nullptr));if (!result) return false;
  JS::SetReservedSlot(result,WESession,JS::ObjectValue(*transport));
  JS::SetReservedSlot(result,WEKind,JS::Int32Value(int(kind)));JS::SetReservedSlot(result,WEStream,JS::DoubleValue(double(id)));
  JS::SetReservedSlot(result,WEGroup,JS::NullValue());JS::SetReservedSlot(result,WEOrder,JS::DoubleValue(0));
  JS::RootedObject endpoints(s.cx,SlotObject(transport,WTEndpoints));uint32_t length=0;
  JS::RootedValue value(s.cx,JS::ObjectValue(*result));
  return JS::GetArrayLength(s.cx,endpoints,&length) && JS_DefineElement(s.cx,endpoints,length,value,JSPROP_ENUMERATE);
}
bool TransportNewEndpoints(State& s,JS::HandleObject transport,uint64_t id,bool bidi,JS::HandleValue group,int64_t order,JS::MutableHandleObject result) {
  JS::RootedObject endpoint(s.cx),readable(s.cx),writable(s.cx);
  if (!TransportNewEndpoint(s,transport,id,EndpointKind::ByteStream,&endpoint)) return false;
  JS::SetReservedSlot(endpoint,WEGroup,group);JS::SetReservedSlot(endpoint,WEOrder,JS::DoubleValue(double(order)));
  // QUIC stream direction bit=2; initiator bit=1 means a peer-created stream.
  const bool remoteUni=(id&3)==3;
  if ((bidi || remoteUni) && !TransportReadStream(s,endpoint,true,&readable)) return false;
  if ((bidi || !remoteUni) && !TransportWriteStream(s,endpoint,false,&writable)) return false;
  if (!bidi) {result.set(remoteUni?readable.get():writable.get());return true;}
  result.set(JS_NewObjectWithGivenProto(s.cx,&kTransportBidiClass,s.transportBidi.prototype));if (!result) return false;
  JS::SetReservedSlot(result,0,JS::ObjectValue(*readable));JS::SetReservedSlot(result,1,JS::ObjectValue(*writable));return true;
}
bool TransportNewDatagramWritable(State& s,JS::HandleObject transport,JS::HandleValue group,int64_t order,JS::MutableHandleObject result) {
  JS::RootedObject endpoint(s.cx);if (!TransportNewEndpoint(s,transport,0,EndpointKind::Datagram,&endpoint)) return false;
  JS::SetReservedSlot(endpoint,WEGroup,group);JS::SetReservedSlot(endpoint,WEOrder,JS::DoubleValue(double(order)));
  return TransportWriteStream(s,endpoint,true,result);
}
bool TransportInitialStreams(State& s,JS::HandleObject transport) {
  JS::RootedObject endpoint(s.cx),stream(s.cx),datagrams(s.cx),writable(s.cx);
  for (bool bidi:{true,false}) {
    if (!TransportNewEndpoint(s,transport,0,bidi?EndpointKind::IncomingBidi:EndpointKind::IncomingUni,&endpoint) ||
        !TransportReadStream(s,endpoint,false,&stream)) return false;
    JS::SetReservedSlot(transport,bidi?WTBidi:WTUni,JS::ObjectValue(*stream));
    JS::SetReservedSlot(transport,bidi?WTBidiController:WTUniController,JS::GetReservedSlot(endpoint,WEReadController));
  }
  if (!TransportNewEndpoint(s,transport,0,EndpointKind::Datagram,&endpoint) || !TransportReadStream(s,endpoint,false,&stream) ||
      !TransportNewDatagramWritable(s,transport,JS::NullHandleValue,0,&writable)) return false;
  datagrams=JS_NewObjectWithGivenProto(s.cx,&kTransportDatagramsClass,s.transportDatagrams.prototype);if (!datagrams) return false;
  JS::SetReservedSlot(datagrams,WDSession,JS::ObjectValue(*transport));
  JS::SetReservedSlot(datagrams,WDReadable,JS::ObjectValue(*stream));JS::SetReservedSlot(datagrams,WDWritable,JS::ObjectValue(*writable));
  JS::SetReservedSlot(datagrams,WDMaxSize,JS::Int32Value(1024));
  JS::SetReservedSlot(datagrams,WDIncomingAge,JS::DoubleValue(std::numeric_limits<double>::infinity()));
  JS::SetReservedSlot(datagrams,WDOutgoingAge,JS::DoubleValue(std::numeric_limits<double>::infinity()));
  JS::SetReservedSlot(datagrams,WDIncomingBuffered,JS::DoubleValue(5));JS::SetReservedSlot(datagrams,WDOutgoingBuffered,JS::DoubleValue(5));
  JS::SetReservedSlot(transport,WTDatagrams,JS::ObjectValue(*datagrams));return true;
}
}

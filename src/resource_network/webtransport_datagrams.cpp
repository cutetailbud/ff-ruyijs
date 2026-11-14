#include "resource_network/webtransport.h"
#include "js/Conversions.h"
#include "js/Realm.h"
#include <cmath>
#include <limits>

namespace ruyijs::resource_network {
namespace {
JSObject* Receiver(JS::HandleValue value) {
  JSObject* object=value.isObject()?js::CheckedUnwrapStatic(&value.toObject()):nullptr;
  return object && JS::GetClass(object)==&kTransportDatagramsClass?object:nullptr;
}
const char* kNames[]={"","readable","writable","maxDatagramSize","incomingMaxAge","outgoingMaxAge","incomingMaxBufferedDatagrams","outgoingMaxBufferedDatagrams"};
}
bool TransportDatagramGet(JSContext* cx,unsigned argc,JS::Value* vp) {
  auto args=JS::CallArgsFromVp(argc,vp);const unsigned slot=WebKit::Slot(args);JS::RootedObject object(cx,Receiver(args.thisv()));
  if (!object) return TransportGuard(cx,[&]{return TransportBindingError(cx,(std::string("get ")+kNames[slot]).c_str(),"WebTransportDatagramDuplexStream");});
  args.rval().set(JS::GetReservedSlot(object,slot));return JS_WrapValue(cx,args.rval());
}
bool TransportDatagramSet(JSContext* cx,unsigned argc,JS::Value* vp) {
  return TransportGuard(cx,[&] {
    auto args=JS::CallArgsFromVp(argc,vp);const unsigned slot=WebKit::Slot(args);JS::RootedObject object(cx,Receiver(args.thisv()));
    if (!object) return TransportBindingError(cx,(std::string("set ")+kNames[slot]).c_str(),"WebTransportDatagramDuplexStream");
    // Firefox 160: the buffered-datagram limits are unsigned long, at least 1.
    if (slot==WDIncomingBuffered || slot==WDOutgoingBuffered) {
      uint32_t count;if (!JS::ToUint32(cx,args.get(0),&count)) return false;
      JS::SetReservedSlot(object,slot,JS::DoubleValue(count?count:1));args.rval().setUndefined();return true;
    }
    double value;if (!JS::ToNumber(cx,args.get(0),&value)) return false;
    if (std::isnan(value) || value<0) {
      const char* detail=slot==WDIncomingAge?"IncomingMaxAge":"OutgoingMaxAge";
      return Error(cx,(std::string("WebTransportDatagramDuplexStream.")+kNames[slot]+" setter: Invalid "+detail).c_str(),true);
    }
    if (slot==WDIncomingAge || slot==WDOutgoingAge) {if (value==0)value=std::numeric_limits<double>::infinity();}
    else if (value<1)value=1;
    JS::SetReservedSlot(object,slot,JS::DoubleValue(value));args.rval().setUndefined();return true;
  });
}
bool TransportCreateWritable(JSContext* cx,unsigned argc,JS::Value* vp) {
  const bool okay=TransportGuard(cx,[&] {
    auto args=JS::CallArgsFromVp(argc,vp);JS::RootedObject self(cx,Receiver(args.thisv()));
    if (!self) return TransportBindingError(cx,"createWritable","WebTransportDatagramDuplexStream");
    JS::RootedObject transport(cx,SlotObject(self,WDSession)),result(cx);State* state=TransportState(cx,transport);if (!state)return false;
    JSAutoRealm realm(cx,transport);JS::RootedValue group(cx),input(cx,args.get(0));int64_t order;
    if(TransportStatus(transport)==TransportPhase::Closed || TransportStatus(transport)==TransportPhase::Failed)
      return TransportThrowDom(*state,"InvalidStateError","WebTransportDatagramDuplexStream.createWritable: WebTransport closed or failed");
    if (!JS_WrapValue(cx,&input) || !TransportSendOptions(cx,transport,input,&group,&order) || !TransportNewDatagramWritable(*state,transport,group,order,&result)) return false;
    args.rval().setObject(*result);return true;
  });
  auto args=JS::CallArgsFromVp(argc,vp);return okay && JS_WrapValue(cx,args.rval());
}
}

#include "resource_network/webtransport.h"
#include "runtime/native_constructor_trace.h"
#include "js/Array.h"
#include "js/Promise.h"
#include "js/Realm.h"
#include "js/String.h"

namespace ruyijs::resource_network {
namespace {
void Finalize(JS::GCContext*,JSObject* object) {TransportFinalize(object);}
const JSClassOps kOps={nullptr,nullptr,nullptr,nullptr,nullptr,nullptr,Finalize};
}
const JSClass kTransportClass={"WebTransport",JSCLASS_HAS_RESERVED_SLOTS(WTSlots)|JSCLASS_FOREGROUND_FINALIZE,&kOps};
const JSClass kTransportEndpointClass={"WebTransport endpoint",JSCLASS_HAS_RESERVED_SLOTS(WESlots)};
const JSClass kTransportDatagramsClass={"WebTransportDatagramDuplexStream",JSCLASS_HAS_RESERVED_SLOTS(WDSlots)};
const JSClass kTransportBidiClass={"WebTransportBidirectionalStream",JSCLASS_HAS_RESERVED_SLOTS(2)};
const JSClass kTransportGroupClass={"WebTransportSendGroup",JSCLASS_HAS_RESERVED_SLOTS(2)};
const JSClass kTransportRequestClass={"WebTransport request",JSCLASS_HAS_RESERVED_SLOTS(WRSlots)};
State* TransportState(JSContext* cx,JS::HandleObject object) {
  JS::RootedObject factories(cx,SlotObject(object,WTFactories));return StreamFactoryState(cx,factories);
}
bool TransportConstruct(JSContext* cx,unsigned argc,JS::Value* vp) {
  return TransportGuard(cx,[&] {
    auto args=JS::CallArgsFromVp(argc,vp);NativeConstructorTrace trace(cx,args);auto& state=*FunctionOwner(args);
    if (!args.isConstructing()) return Error(cx,"WebTransport constructor: 'new' is required");
    JS::RootedObject prototype(cx);if (!TransportPrototype(cx,args,0,&prototype)) return false;
    if (!argc) return Error(cx,"WebTransport constructor: At least 1 argument required, but only 0 passed");
    WebTransportOptions options;if (!TransportOptions(cx,args,&options)) return false;
    JS::RootedObject object(cx,JS_NewObjectWithGivenProto(cx,&kTransportClass,prototype)),factories(cx);
    JS::RootedObject ready(cx,JS::NewPromiseObject(cx,nullptr)),closed(cx,JS::NewPromiseObject(cx,nullptr)),draining(cx,JS::NewPromiseObject(cx,nullptr));
    JS::RootedObject endpoints(cx,JS::NewArrayObject(cx,0));
    if (!object || !ready || !closed || !draining || !endpoints || !StreamFactories(cx,state.page,&factories)) return false;
    JS::SetReservedSlot(object,WTFactories,JS::ObjectValue(*factories));
    JS::SetReservedSlot(object,WTReady,JS::ObjectValue(*ready));JS::SetReservedSlot(object,WTClosed,JS::ObjectValue(*closed));
    JS::SetReservedSlot(object,WTDraining,JS::ObjectValue(*draining));JS::SetReservedSlot(object,WTEndpoints,JS::ObjectValue(*endpoints));
    JS::SetReservedSlot(object,WTProtocol,JS::StringValue(JS_GetEmptyString(cx)));SetTransportStatus(object,TransportPhase::Connecting);
    JS::SetReservedSlot(object,WTWasReady,JS::BooleanValue(false));
    JS::SetReservedSlot(object,WTNextGroup,JS::DoubleValue(1));
    JS::SetReservedSlot(object,WTClosing,JS::BooleanValue(false));
    if (!TransportInitialStreams(state,object)) return false;
    std::string failure;
    if (!TransportAllocate(state,object,options,&failure)) return Error(cx,failure.c_str());
    if (!TransportSchedule(cx,object)) return false;
    args.rval().setObject(*object);return trace.Finish(true);
  });
}
bool TransportGet(JSContext* cx,unsigned argc,JS::Value* vp) {
  auto args=JS::CallArgsFromVp(argc,vp);const int kind=WebKit::Slot(args);
  JS::RootedObject object(cx,args.thisv().isObject()?js::CheckedUnwrapStatic(&args.thisv().toObject()):nullptr);
  const char* names[]={"get ready","get reliability","get congestionControl","get responseHeaders","get protocol","get closed","get draining","get datagrams","get incomingBidirectionalStreams","get incomingUnidirectionalStreams"};
  if (!object || JS::GetClass(object)!=&kTransportClass) {
    TransportBindingError(cx,names[kind],"WebTransport");
    return kind==WTGetReady || kind==WTGetClosed || kind==WTGetDraining?TransportRejectPending(cx,args.rval()):false;
  }
  if (kind==WTGetReliability || kind==WTGetCongestion) {
    JS::RootedString text(cx,JS_NewStringCopyZ(cx,kind==WTGetCongestion?"default":JS::GetReservedSlot(object,WTWasReady).toBoolean()?"supports-unreliable":"pending"));
    if (!text) return false;args.rval().setString(text);return true;
  }
  // Firefox 160: the CONNECT response's headers once the session is up;
  // ruyijs's Necko session does not report them, so this stays null.
  if (kind==WTGetResponseHeaders) {args.rval().setNull();return true;}
  const unsigned slots[]={WTReady,0,0,0,WTProtocol,WTClosed,WTDraining,WTDatagrams,WTBidi,WTUni};
  args.rval().set(JS::GetReservedSlot(object,slots[kind]));return JS_WrapValue(cx,args.rval());
}
}

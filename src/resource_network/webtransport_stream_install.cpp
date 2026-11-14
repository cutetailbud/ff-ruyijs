#include "resource_network/webtransport.h"
#include "js/PropertyAndElement.h"
#include "js/Realm.h"
#include "js/Symbol.h"

namespace ruyijs::resource_network {
namespace {
bool Illegal(JSContext* cx,unsigned,JS::Value*) {return Error(cx,"Illegal constructor.");}
bool Begin(State& s,WebFace& face,const char* name,const JSClass* clasp,JS::HandleObject parent,JS::HandleObject constructorParent) {
  if (!s.kit.Define(name,clasp,parent,Illegal,&face)) return false;
  JS::RootedId tag(s.cx,JS::PropertyKey::Symbol(JS::GetWellKnownSymbol(s.cx,JS::SymbolCode::toStringTag)));
  return (!constructorParent || JS_SetPrototype(s.cx,face.constructor,constructorParent)) &&
      JS_DeleteProperty(s.cx,face.prototype,"constructor") && JS_DeletePropertyById(s.cx,face.prototype,tag);
}
}
bool TransportInstallStreams(State& s) {
  auto& k=s.kit;JSContext* cx=s.cx;JS::RootedObject object(cx,JS::GetRealmObjectPrototype(cx)),none(cx);
  auto& d=s.transportDatagrams;
  if (!Begin(s,d,"WebTransportDatagramDuplexStream",&kTransportDatagramsClass,object,none) ||
      !k.Method(d,"WebTransportDatagramDuplexStream","createWritable",TransportCreateWritable,0,0)) return false;
  const char* names[]={"readable","writable","maxDatagramSize","incomingMaxAge","outgoingMaxAge","incomingMaxBufferedDatagrams","outgoingMaxBufferedDatagrams"};
  for (int i=0;i<7;++i) if (!k.Accessor(d,"WebTransportDatagramDuplexStream",names[i],TransportDatagramGet,i>=3?TransportDatagramSet:nullptr,i+1)) return false;
  if (!TransportFinishFace(s,d,"WebTransportDatagramDuplexStream") ||
      !Begin(s,s.transportBidi,"WebTransportBidirectionalStream",&kTransportBidiClass,object,none) ||
      !k.Accessor(s.transportBidi,"WebTransportBidirectionalStream","readable",TransportBidiGet,nullptr,0) ||
      !k.Accessor(s.transportBidi,"WebTransportBidirectionalStream","writable",TransportBidiGet,nullptr,1) ||
      !TransportFinishFace(s,s.transportBidi,"WebTransportBidirectionalStream")) return false;
  JS::RootedObject writableProto(cx,s.writableStream.prototype),writableCtor(cx,s.writableStream.constructor);
  for (bool datagram:{false,true}) {
    auto& face=datagram?s.transportDatagramsWritable:s.transportSend;
    const char* name=datagram?"WebTransportDatagramsWritable":"WebTransportSendStream";
    if (!Begin(s,face,name,&kTransportEndpointClass,writableProto,writableCtor)) return false;
    if (!datagram && !k.Method(face,name,"getStats",TransportEndpointStats,0,1)) return false;
    if (!k.Accessor(face,name,"sendGroup",TransportEndpointGet,TransportEndpointSet,datagram?2:0) ||
        !k.Accessor(face,name,"sendOrder",TransportEndpointGet,TransportEndpointSet,datagram?3:1) ||
        !TransportFinishFace(s,face,name)) return false;
  }
  JS::RootedObject readableProto(cx,s.stream.prototype),readableCtor(cx,s.stream.constructor);
  return Begin(s,s.transportReceive,"WebTransportReceiveStream",&kTransportEndpointClass,readableProto,readableCtor) &&
      k.Method(s.transportReceive,"WebTransportReceiveStream","getStats",TransportEndpointStats,0,0) &&
      TransportFinishFace(s,s.transportReceive,"WebTransportReceiveStream") &&
      Begin(s,s.transportGroup,"WebTransportSendGroup",&kTransportGroupClass,object,none) &&
      k.Method(s.transportGroup,"WebTransportSendGroup","getStats",TransportGroupStats,0,0) &&
      TransportFinishFace(s,s.transportGroup,"WebTransportSendGroup");
}
bool TransportBidiGet(JSContext* cx,unsigned argc,JS::Value* vp) {
  auto args=JS::CallArgsFromVp(argc,vp);const unsigned slot=WebKit::Slot(args);
  JS::RootedObject object(cx,args.thisv().isObject()?js::CheckedUnwrapStatic(&args.thisv().toObject()):nullptr);
  if (!object || JS::GetClass(object)!=&kTransportBidiClass) return TransportBindingError(cx,slot?"get writable":"get readable","WebTransportBidirectionalStream");
  args.rval().set(JS::GetReservedSlot(object,slot));return JS_WrapValue(cx,args.rval());
}
}

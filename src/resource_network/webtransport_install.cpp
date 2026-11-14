#include "resource_network/webtransport.h"
#include "runtime/page_reference.h"
#include "js/PropertyAndElement.h"
#include "js/Realm.h"
#include "js/String.h"
#include "js/Symbol.h"

namespace ruyijs::resource_network {
namespace {
const JSClass kTransportPrototypes={"WebTransport prototypes",JSCLASS_HAS_RESERVED_SLOTS(2)};
bool Reliable(JSContext*,unsigned argc,JS::Value* vp) {JS::CallArgsFromVp(argc,vp).rval().setBoolean(false);return true;}
bool ClearFace(State& s,WebFace& f) {
  JS::RootedId tag(s.cx,JS::PropertyKey::Symbol(JS::GetWellKnownSymbol(s.cx,JS::SymbolCode::toStringTag)));
  return JS_DeleteProperty(s.cx,f.prototype,"constructor") && JS_DeletePropertyById(s.cx,f.prototype,tag);
}
}
bool TransportFinishFace(State& s,WebFace& face,const char* name) {
  JS::RootedString text(s.cx,JS_NewStringCopyZ(s.cx,name));
  JS::RootedId tag(s.cx,JS::PropertyKey::Symbol(JS::GetWellKnownSymbol(s.cx,JS::SymbolCode::toStringTag)));
  return text && JS_DefineProperty(s.cx,face.prototype,"constructor",face.constructor,0) &&
      JS_DefinePropertyById(s.cx,face.prototype,tag,text,JSPROP_READONLY) && s.secure.Add(s.page,name);
}
bool TransportPrototype(JSContext* cx,const JS::CallArgs& args,unsigned kind,JS::MutableHandleObject result) {
  JS::RootedObject target(cx,&args.newTarget().toObject());JS::RootedValue value(cx);
  if (!JS_GetProperty(cx,target,"prototype",&value)) return false;
  if (value.isObject()) {result.set(&value.toObject());return true;}
  JS::Realm* realm=JS::GetFunctionRealm(cx,target);if (!realm) return false;
  JS::RootedObject global(cx,JS::GetRealmGlobalOrNull(realm));
  if (!ReadPageReference(global,PageReference::ResourceTransportPrototypes,&value) || !value.isObject())
    return Error(cx,"WebTransport target realm is not installed");
  result.set(SlotObject(&value.toObject(),kind));return JS_WrapObject(cx,result);
}
bool InstallWebTransport(State& s,PageHelpers&) {
  JSContext* cx=s.cx;auto& kit=s.kit;JS::RootedObject parent(cx,JS::GetRealmObjectPrototype(cx));
  if (!s.transportExceptions.create || !s.transportExceptions.data) return Error(cx,"WebTransport exception factory is not bound");
  if (!kit.Define("WebTransport",&kTransportClass,parent,TransportConstruct,&s.transport,1) || !ClearFace(s,s.transport)) return false;
  const char* methods[]={"getStats","exportKeyingMaterial","close","createBidirectionalStream","createUnidirectionalStream","createSendGroup"};
  for (int i=0;i<6;++i) if (!kit.Method(s.transport,"WebTransport",methods[i],TransportMethodCall,i==1?1:0,i)) return false;
  const char* attrs[]={"ready","reliability","congestionControl","responseHeaders","protocol","closed","draining","datagrams","incomingBidirectionalStreams","incomingUnidirectionalStreams"};
  for (int i=0;i<10;++i) if (!kit.Accessor(s.transport,"WebTransport",attrs[i],TransportGet,nullptr,i)) return false;
  JS::RootedObject reliable(cx,kit.Function(Reliable,"get supportsReliableOnly",0,0));
  JS::RootedObject none(cx),domConstructor(cx);JS::RootedValue value(cx);
  if (!reliable || !JS_DefineProperty(cx,s.transport.constructor,"supportsReliableOnly",reliable,none,JSPROP_ENUMERATE) ||
      !TransportFinishFace(s,s.transport,"WebTransport") || !JS_GetProperty(cx,s.page,"DOMException",&value) || !value.isObject()) return false;
  domConstructor=&value.toObject();if (!JS_GetProperty(cx,domConstructor,"prototype",&value) || !value.isObject()) return false;
  s.transportExceptionPrototype=&value.toObject();parent=s.transportExceptionPrototype;
  if (!kit.Define("WebTransportError",&kTransportErrorDataClass,parent,TransportErrorConstruct,&s.transportError) ||
      !JS_SetPrototype(cx,s.transportError.constructor,domConstructor) || !ClearFace(s,s.transportError) ||
      !kit.Accessor(s.transportError,"WebTransportError","source",TransportErrorGet,nullptr,0) ||
      !kit.Accessor(s.transportError,"WebTransportError","streamErrorCode",TransportErrorGet,nullptr,1) ||
      !TransportFinishFace(s,s.transportError,"WebTransportError") || !TransportInstallStreams(s)) return false;
  JS::RootedObject record(cx,JS_NewObjectWithGivenProto(cx,&kTransportPrototypes,nullptr));if (!record) return false;
  JS::SetReservedSlot(record,0,JS::ObjectValue(*s.transport.prototype));JS::SetReservedSlot(record,1,JS::ObjectValue(*s.transportError.prototype));
  value.setObject(*record);return WritePageReference(s.page,PageReference::ResourceTransportPrototypes,value);
}
}

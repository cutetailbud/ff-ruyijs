#include "web_indexeddb/keys.h"
#include "js/Object.h"
#include "js/PropertyAndElement.h"
#include "js/Wrapper.h"
namespace ruyijs::web_indexeddb {
namespace {
State* StoreReceiver(JSContext* cx,const JS::CallArgs& args,JS::MutableHandleObject out) {
  JS::HandleValue value=args.thisv();
  out.set(value.isObject()?js::CheckedUnwrapStatic(&value.toObject()):nullptr);
  if(!out || JS::GetClass(out)!=&kStoreClass) {
    WebKit::FailBrand(cx,args,"IDBObjectStore");return nullptr;
  }
  return static_cast<State*>(JS::GetReservedSlot(out,StoreOwner).toPrivate());
}
}
JSObject* NewObjectStore(State& s,JS::HandleObject tx,const std::u16string& name,const std::u16string& path,
                         JS::HandleObject db) {
  JS::RootedObject object(s.cx,s.kit.New(&kStoreClass,s.storeFace.prototype));
  JS::RootedValue n(s.cx),p(s.cx,JS::NullValue());
  if(!object || !String(s.cx,name,&n) || (!path.empty() && !String(s.cx,path,&p)))return nullptr;
  JS::SetReservedSlot(object,StoreOwner,JS::PrivateValue(&s));
  JS::SetReservedSlot(object,StoreTransaction,tx?JS::ObjectValue(*tx):JS::NullValue());
  JS::SetReservedSlot(object,StoreName,n);JS::SetReservedSlot(object,StoreKeyPath,p);
  JS::SetReservedSlot(object,StoreDatabase,JS::ObjectValue(*db));
  JS::SetReservedSlot(object,StoreIndexes,JS::UndefinedValue());
  JS::SetReservedSlot(object,StoreTxId,JS::Int32Value(0));
  return object;
}
JSObject* NewRequest(State& s,JS::HandleObject source,JS::HandleObject tx) {
  JS::RootedObject r(s.cx,s.kit.New(&kRequestClass,s.requestFace.prototype));
  if(!r || !s.events.Attach(r))return nullptr;
  JS::SetReservedSlot(r,OwnerSlot,JS::PrivateValue(&s));
  JS::SetReservedSlot(r,ResultSlot,JS::UndefinedValue());JS::SetReservedSlot(r,ErrorSlot,JS::NullValue());
  JS::SetReservedSlot(r,ReadySlot,JS::BooleanValue(false));
  JS::SetReservedSlot(r,SourceSlot,JS::ObjectValue(*source));
  JS::SetReservedSlot(r,TransactionSlot,JS::ObjectValue(*tx));
  for(int i=0;i<4;++i)JS::SetReservedSlot(r,HandlerSlot+i,JS::NullValue());
  return r;
}
// Data jobs run in request order, ahead of their transaction's completion job.
// The catalog transaction a request runs in: its source's object store's
// (a cursor's source is a store or an index, an index's is its store).
JSObject* SourceStore(JSObject* source) {
  for(int depth=0;source && depth<3;++depth) {
    const JSClass* clasp=JS::GetClass(source);
    if(clasp==&kStoreClass)return source;
    if(clasp==&kIndexClass)source=&JS::GetReservedSlot(source,IndexStore).toObject();
    else if(clasp==&kCursorClass || clasp==&kCursorValueClass)source=&JS::GetReservedSlot(source,CursorSource).toObject();
    else return nullptr;
  }
  return nullptr;
}
uint32_t SourceTxId(JSObject* source) {
  source=SourceStore(source);
  return source?uint32_t(JS::GetReservedSlot(source,StoreTxId).toInt32()):0;
}
bool EnqueueJob(State& s,std::unique_ptr<Job> job,JS::HandleObject tx) {
  if(job->request && JS::GetClass(job->request)==&kRequestClass)
    job->txId=SourceTxId(&JS::GetReservedSlot(job->request,SourceSlot).toObject());
  if(!job->txId)job->txId=uint32_t(JS::GetReservedSlot(tx,TxId).toInt32());
  auto position=s.store->jobs.begin();
  for(;position!=s.store->jobs.end();++position)
    if((*position)->kind==3 && (*position)->request==tx)break;
  s.store->jobs.insert(position,std::move(job));
  return Schedule(*s.store);
}
bool StoreGet(JSContext* cx,unsigned n,JS::Value* vp) {
  auto args=JS::CallArgsFromVp(n,vp);JS::RootedObject self(cx);
  if(!StoreReceiver(cx,args,&self))return false;
  const int slot=WebKit::Slot(args);  // read before rval(), which shares the callee slot
  args.rval().set(JS::GetReservedSlot(self,slot));
  // An upgrade-created store's transaction is its connection's versionchange one.
  if(slot==StoreTransaction && args.rval().isNull())
    args.rval().set(JS::GetReservedSlot(&JS::GetReservedSlot(self,StoreDatabase).toObject(),DbUpgrade));
  return JS_WrapValue(cx,args.rval());
}
bool StoreMethod(JSContext* cx,unsigned n,JS::Value* vp) {
  auto args=JS::CallArgsFromVp(n,vp);JS::RootedObject self(cx);
  State* s=StoreReceiver(cx,args,&self);if(!s)return false;
  State& caller=*static_cast<State*>(WebKit::Owner(args));
  int op=WebKit::Slot(args);Query query;JS::RootedValue queryInput(cx);
  if(!PrepareQuery(cx,args,op,&query,&queryInput))return false;
  JS::RootedValue tv(cx,JS::GetReservedSlot(self,StoreTransaction));
  // An upgrade-created store runs in its connection's versionchange transaction.
  if(!tv.isObject())tv=JS::GetReservedSlot(&JS::GetReservedSlot(self,StoreDatabase).toObject(),DbUpgrade);
  if(!tv.isObject())return Throw(caller,"TransactionInactiveError");
  JS::RootedObject tx(cx,&tv.toObject());
  if(JS::GetReservedSlot(tx,TxActive).toInt32())return Throw(caller,"TransactionInactiveError");
  std::u16string mode,key,path;
  JS::RootedValue modeValue(cx,JS::GetReservedSlot(tx,TxMode)),keyValue(cx,args.get(0));
  if(!Text(cx,modeValue,&mode))return false;
  const bool write=op==Put || op==Add;
  bool generate=false,autoIncrement=false;
  if(write) {
    if(mode!=u"readwrite" && mode!=u"versionchange")return Throw(caller,"ReadOnlyError");
    {
      Key database;JS::RootedObject ignored(cx);std::u16string storeName;
      JS::RootedValue nameValue(cx,JS::GetReservedSlot(self,StoreName));
      if(!StoreConnection(caller,self,&database,&ignored) || !Text(cx,nameValue,&storeName) ||
         !s->store->model.AutoIncrement(database,storeName,&autoIncrement))return Throw(caller,"InvalidStateError");
    }
    JS::RootedValue pv(cx,JS::GetReservedSlot(self,StoreKeyPath));
    if(pv.isString()) {
      if(!args.get(1).isUndefined() || !args[0].isObject())return Throw(caller,"DataError");
      if(!Text(cx,pv,&path))return false;
      JS::RootedObject record(cx,&args[0].toObject());
      if(!JS_GetUCProperty(cx,record,path.data(),path.size(),&keyValue))return false;
      // In-line key absent: the generator supplies it and it is injected.
      generate=autoIncrement && keyValue.isUndefined();
    } else {
      keyValue.set(args.get(1));
      generate=autoIncrement && keyValue.isUndefined();
      path.clear();
    }
    if(!generate && !EncodeKey(caller,keyValue,&key))return false;
  } else if(!ReadQuery(caller,queryInput,op,&query))return false;
  if(s->store->jobs.size()>=1024)return Throw(caller,"QuotaExceededError");
  std::vector<uint8_t> bytes;
  IndexKeys indexKeys;
  if(write) {  // index keys come from the record's clone, at request time
    Key database;JS::RootedObject ignored(cx);JS::RootedValue clone(cx);std::u16string storeName;
    JS::RootedValue nameValue(cx,JS::GetReservedSlot(self,StoreName));
    if(!CloneWrite(caller,args[0],&bytes) || !CloneRead(caller,bytes,&clone) ||
       !StoreConnection(caller,self,&database,&ignored) || !Text(cx,nameValue,&storeName) ||
       !RecordIndexKeys(caller,database,storeName,clone,&indexKeys))return false;
  }
  {
  JSAutoRealm realm(cx,self);
  JS::RootedObject request(cx,NewRequest(*s,self,tx));if(!request)return false;
  auto job=std::make_unique<Job>(s,cx,request,Key{},0,false);
  job->kind=write?Put:op;job->noOverwrite=op==Add;job->recordKey=std::move(key);job->bytes=std::move(bytes);
  job->query=std::move(query);job->indexKeys=std::move(indexKeys);
  if(write) {
    job->generate=generate;job->observe=autoIncrement && !generate;
    if(generate && !path.empty()) {
      job->injectPath=path;
      JS::RootedValue nameValue(cx,JS::GetReservedSlot(self,StoreName));JS::RootedObject ignored(cx);
      if(!StoreConnection(caller,self,&job->key,&ignored) || !Text(cx,nameValue,&job->storeName))return false;
    }
  }
  if(!EnqueueJob(*s,std::move(job),tx))return false;
  args.rval().setObject(*request);
  }
  return JS_WrapValue(cx,args.rval());
}
}

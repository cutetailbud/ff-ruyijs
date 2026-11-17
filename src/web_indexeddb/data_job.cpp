#include "web_indexeddb/keys.h"
#include "js/Object.h"
namespace ruyijs::web_indexeddb {
bool FinishClose(State& s,JS::HandleObject db) {
  for(const auto& job:s.store->jobs) {
    if(job->kind!=3 || job->done)continue;
    if(&JS::GetReservedSlot(job->request,TxDatabase).toObject()==db)return true;
  }
  if(s.store->model.Close(JS::GetReservedSlot(db,DbId).toInt32()))return Schedule(*s.store);
  return true;
}
// The key generator's next number becomes the record key; an in-line key path
// receives it in the stored clone and the index keys are read from that clone.
bool GenerateKey(Job& job,uint32_t id,const char** error) {
  auto& s=*job.owner;JSContext* cx=s.cx;
  double number=0;
  if(!s.store->model.NextKey(id,&number)) { *error="ConstraintError";return true; }
  JS::RootedValue key(cx,JS::NumberValue(number));
  if(!EncodeKey(s,key,&job.recordKey))return false;
  if(job.injectPath.empty())return true;
  JS::RootedValue clone(cx);
  if(!CloneRead(s,job.bytes,&clone))return false;
  if(!clone.isObject()) { *error="DataError";return true; }
  JS::RootedObject record(cx,&clone.toObject());
  if(!JS_SetUCProperty(cx,record,job.injectPath.data(),job.injectPath.size(),key))return false;
  job.bytes.clear();job.indexKeys.clear();
  return CloneWrite(s,clone,&job.bytes) && RecordIndexKeys(s,job.key,job.storeName,clone,&job.indexKeys);
}
bool DataJob(Job& job) {
  auto& s=*job.owner;JSContext* cx=s.cx;
  JS::RootedObject tx(cx,job.kind==3?job.request.get():
      &JS::GetReservedSlot(job.request,TransactionSlot).toObject());
  // The completion job ends the whole scope; a request runs in its store's
  // catalog transaction (EnqueueJob).
  uint32_t id=job.kind==3?JS::GetReservedSlot(tx,TxId).toInt32():job.txId;
  int state=JS::GetReservedSlot(tx,TxActive).toInt32();
  if(job.kind==3) {
    bool aborted=state==2;
    if(!aborted && !EndTransaction(s,tx,true)) {
      EndTransaction(s,tx,false);aborted=true;
      JS::RootedValue error(cx);if(!ErrorValue(s,"QuotaExceededError",&error))return false;
      JS::SetReservedSlot(tx,TxError,error);
    }
    JS::SetReservedSlot(tx,TxActive,JS::Int32Value(aborted?2:1));job.done=true;
    JS::RootedObject db(cx,&JS::GetReservedSlot(tx,TxDatabase).toObject());
    if(!s.events.Fire(tx,aborted?"abort":"complete",false,false))return false;
    return !JS::GetReservedSlot(db,9).toBoolean() || FinishClose(s,db);
  }
  JS::RootedValue value(cx,JS::UndefinedValue());const char* error=nullptr;
  // A request of the versionchange transaction (catalog id 0) runs in its own
  // catalog transaction over the upgraded database, committed when it ends:
  // an aborted upgrade reverts the whole database (Model::Revert).
  bool upgrade=false;
  if(!id && state!=2) {
    JS::RootedObject store(cx,SourceStore(&JS::GetReservedSlot(job.request,SourceSlot).toObject()));
    Key database;JS::RootedObject ignored(cx);std::u16string storeName;
    JS::RootedValue nameValue(cx,store?JS::GetReservedSlot(store,StoreName):JS::UndefinedValue());
    if(!store || !StoreConnection(s,store,&database,&ignored) || !Text(cx,nameValue,&storeName) ||
       !(id=s.store->model.Begin(database,storeName,true))) {
      JS_ClearPendingException(cx);error="InvalidStateError";
    } else { upgrade=true;job.txId=id; }
  }
  else if(job.kind==Put) {
    if(job.generate && !GenerateKey(job,id,&error)) return false;
  }
  if(error) {
  } else if(job.kind==Put) {
    if(job.observe) {
      JS::RootedValue explicitKey(cx);
      if(!DecodeKey(s,job.recordKey,&explicitKey))return false;
      if(explicitKey.isNumber()) s.store->model.Observe(id,explicitKey.toNumber());
    }
    const WriteCode code=s.store->model.Write(id,job.recordKey,job.bytes,job.indexKeys,job.noOverwrite);
    if(code==WriteCode::Quota)error="QuotaExceededError";
    else if(code!=WriteCode::Ok)error="ConstraintError";  // unique index or add() on an existing key
    else if(!DecodeKey(s,job.recordKey,&value))return false;
  } else if(job.kind==Delete || job.kind==Clear) {
    if(!s.store->model.Delete(id,job.query.range,job.kind==Clear))error="QuotaExceededError";
  } else if(job.kind==CursorStepKind) {
    if(!CursorJob(job,&error,&value))return false;
  } else {
    if(!QueryResult(job,&value,&error))return false;
  }
  if(upgrade) {
    if(!s.store->model.End(id,!error) && !error)error="QuotaExceededError";
    job.txId=0;
  }
  JS::SetReservedSlot(job.request,ReadySlot,JS::BooleanValue(true));job.done=true;
  if(error) {
    if(!ErrorValue(s,error,&value))return false;
    JS::SetReservedSlot(job.request,ErrorSlot,value);
    JS::SetReservedSlot(job.request,ResultSlot,JS::UndefinedValue());
    // An error event the page does not cancel aborts the transaction.
    bool allowed=true;
    if(!s.events.Fire(job.request,"error",true,true,&allowed))return false;
    if(allowed && JS::GetReservedSlot(tx,TxActive).toInt32()!=2) {
      EndTransaction(s,tx,false);JS::SetReservedSlot(tx,TxActive,JS::Int32Value(2));
      if(JS::GetReservedSlot(tx,TxError).isNull())JS::SetReservedSlot(tx,TxError,value);
    }
    return true;
  }
  JS::SetReservedSlot(job.request,ResultSlot,value);
  return s.events.Fire(job.request,"success",false,false);
}
}

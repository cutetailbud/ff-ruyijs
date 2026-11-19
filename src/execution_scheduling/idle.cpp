#include "execution_scheduling/idle.h"
#include "jsfriendapi.h"
#include "js/CallAndConstruct.h"
#include "js/Conversions.h"
#include "js/Wrapper.h"
#include <new>
namespace ruyijs::execution_scheduling {
namespace {
bool Factory(JSContext* cx,const JS::CallArgs& args,JS::MutableHandleObject factory){
 factory.set(&js::GetFunctionNativeReserved(&args.callee(),0).toObject());
 if(args.thisv().isNullOrUndefined())return true;
 JS::RootedObject receiver(cx,args.thisv().isObject()?js::CheckedUnwrapStatic(&args.thisv().toObject()):nullptr);
 if(receiver){auto& s=IdleState(factory);
  for(JSObject* candidate:s.idleFactories){
   if(&JS::GetReservedSlot(candidate,IPage).toObject()==receiver){factory.set(candidate);return true;}
  }
 }
 return Error(cx,"Window idle member called on incompatible receiver");
}
}
bool IdleRequest(JSContext* cx,unsigned argc,JS::Value* vp){
 auto args=JS::CallArgsFromVp(argc,vp);JS::RootedObject factory(cx);
 if(!Factory(cx,args,&factory))return false;auto& s=IdleState(factory);
 s.Observe("Window","requestIdleCallback",int(ApiAccess::Call));
 if(!argc||!args[0].isObject()||!JS::IsCallable(&args[0].toObject()))return Error(cx,"Idle callback must be callable");
 JS::RootedValue options(cx,args.get(1)),timeout(cx);
 if(!options.isNullOrUndefined()){
  if(!options.isObject())return Error(cx,"Idle options must be a dictionary");
  JS::RootedObject object(cx,&options.toObject());if(!JS_GetProperty(cx,object,"timeout",&timeout))return false;
 }
 uint32_t duration=0;
 if(!timeout.isUndefined()&&!JS::ToUint32(cx,timeout,&duration))return false;
 if(s.argumentSlots>=100000)return Error(cx,"Timer argument root limit exceeded",true);
 std::unique_ptr<Callback> callback(new(std::nothrow) Callback(cx));if(!callback){JS_ReportOutOfMemory(cx);return false;}
 callback->kind=Kind::Idle;callback->handler=args[0];
 callback->global=&JS::GetReservedSlot(factory,IPage).toObject();
 if(s.Retired(callback->global)){args.rval().setInt32(0);return true;}
 if(!callback->arguments.append(JS::ObjectValue(*factory)))return false;
 uint32_t handle=uint32_t(JS::GetReservedSlot(factory,INext).toNumber());callback->idleHandle=handle;
 s.SyncClock();if(!s.Accept(s.model.ScheduleIdle(!timeout.isUndefined(),duration,&callback->id)))return false;
 ++s.argumentSlots;s.Track(std::move(callback));
 JS::SetReservedSlot(factory,INext,JS::DoubleValue(uint32_t(handle+1)));args.rval().setNumber(handle);return true;
}
bool IdleCancel(JSContext* cx,unsigned argc,JS::Value* vp){
 auto args=JS::CallArgsFromVp(argc,vp);JS::RootedObject factory(cx);
 if(!Factory(cx,args,&factory))return false;auto& s=IdleState(factory);
 s.Observe("Window","cancelIdleCallback",int(ApiAccess::Call));
 if(!argc)return Error(cx,"cancelIdleCallback requires a handle");
 uint32_t handle=0;if(!JS::ToUint32(cx,args[0],&handle))return false;
 auto hit=s.activeIdles.find({factory.get(),handle});
 if(hit!=s.activeIdles.end()){
  uint64_t id=hit->second;Callback* cb=s.Find(id);
  if(cb&&cb->kind==Kind::Idle&&!cb->running){s.model.CancelIdle(id);s.Remove(id);}
 }
 args.rval().setUndefined();return true;
}
}

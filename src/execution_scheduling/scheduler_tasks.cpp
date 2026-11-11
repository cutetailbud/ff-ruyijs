#include "execution_scheduling/scheduler.h"
#include "execution_scheduling/performance.h"
#include "resource_network/abort.h"
#include "jsfriendapi.h"
#include "js/CallAndConstruct.h"
#include "js/Conversions.h"
#include "js/Exception.h"
#include <cmath>
#include <new>
namespace ruyijs::execution_scheduling {
bool ScheduleWebTask(JSContext* cx,JS::HandleObject f,const JS::CallArgs& args,bool continuation,JS::HandleObject promise){
 auto& s=TaskState(f);JS::RootedValue handler(cx),signal(cx),options(cx,args.get(1)),priorityValue(cx);
 int priority=1;bool dynamic=false;double delay=0;
 if(!continuation){
  handler=args.get(0);
  if(!handler.isObject()||!JS::IsCallable(&handler.toObject()))return Error(cx,"postTask requires a callable");
  JS::RootedValue value(cx);if(!JS_WrapValue(cx,&options)||!PerfOption(cx,options,"delay",&value))return false;
  if(!value.isUndefined()&&!JS::ToNumber(cx,value,&delay))return false;
  if(!std::isfinite(delay)||std::trunc(delay)<0||std::trunc(delay)>=18446744073709551616.0)
   return Error(cx,"delay is outside unsigned long long range");
  delay=std::trunc(delay);
  if(!PerfOption(cx,options,"priority",&priorityValue)||
    (!priorityValue.isUndefined()&&!TaskPriority(cx,priorityValue,&priority))||
    !PerfOption(cx,options,"signal",&signal))return false;
 }else if(Callback* running=s.Find(s.runningWebTask)){
  signal=running->arguments[2];priority=running->taskPriority;dynamic=running->dynamicPriority;
 }
 if(!JS_WrapValue(cx,&handler)||!JS_WrapValue(cx,&signal))return false;
 bool aborted=false;JS::RootedValue reason(cx);
 if(!TaskAborted(cx,f,signal,&aborted,&reason))return false;
 if(aborted){JS_SetPendingException(cx,reason);return false;}
 if(!continuation&&priorityValue.isUndefined()&&signal.isObject()){
  JS::RootedObject record(cx);if(!TaskFind(cx,signal,false,&record))return false;
  if(record){priority=JS::GetReservedSlot(record,TPriority).toInt32();dynamic=true;}
 }
 std::unique_ptr<Callback> cb(new(std::nothrow) Callback(cx));
 if(s.argumentSlots>99996)return Error(cx,"Task argument root limit exceeded",true);
 if(!cb){JS_ReportOutOfMemory(cx);return false;}
 cb->kind=Kind::WebTask;cb->handler=handler;cb->taskPriority=priority;cb->dynamicPriority=dynamic;
 // A retired document's task never runs; its promise stays pending.
 cb->global=&JS::GetReservedSlot(f,TPage).toObject();if(s.Retired(cb->global))return true;
 if(!cb->arguments.resize(4))return false;
 cb->arguments[0].setObject(*f);cb->arguments[1].setObject(*promise);cb->arguments[2].set(signal);
 s.SyncClock();if(!s.Accept(s.model.ScheduleWebTask(delay,priority,continuation,&cb->id)))return false;
 if(signal.isObject()){
  JS::RootedObject listener(cx,TaskFunction(cx,f,WebTaskAbort,"taskAbort",0,int(cb->id)));
  if(!listener){s.model.CancelWebTask(cb->id);return false;}
  cb->arguments[3].setObject(*listener);
  if(!resource_network::AddAbortAlgorithm(cx,signal,cb->arguments[3])){s.model.CancelWebTask(cb->id);return false;}
 }
 s.argumentSlots+=cb->arguments.length();s.Track(std::move(cb));return true;
}
}

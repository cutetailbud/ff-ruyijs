#include "resource_network/native_fetch_internal.h"
#include "resource_network/state.h"
#include "resource_network/abort.h"
#include "jsfriendapi.h"
#include "js/Realm.h"

namespace ruyijs::resource_network {
bool NativeFetchAbort(JSContext* cx,unsigned argc,JS::Value* vp) {
  auto args=JS::CallArgsFromVp(argc,vp);
  // rval() is the callee's own slot (vp[0]): read the callee first. Writing
  // rval first made every abort of an in-flight fetch read `undefined` as the
  // callee object (0xC0000005 in xhs).
  auto slot=js::GetFunctionNativeReserved(&args.callee(),0);
  args.rval().setUndefined();
  if (!slot.isObject()) return true;
  JS::RootedObject operation(cx,&slot.toObject());
  auto* record=FetchRecord(operation);
  if (!record) return true;
  // Cancel drops the Job's reference: retain first, before any terminal work.
  auto keep=record->weak_from_this().lock();
  JSAutoRealm realm(cx,operation);
  JS::RootedObject factories(cx,SlotObject(operation,FFactories));
  State* state=StreamFactoryState(cx,factories);
  return state && FinishNativeFetch(*state,keep,NativeFetchOutcome::Aborted);
}
bool InstallNativeFetchAbort(State& state,JS::HandleObject operation) {
  JSContext* cx=state.cx;
  JS::RootedValue signal(cx,JS::GetReservedSlot(operation,FSignal));
  if (signal.isNullOrUndefined()) return true;
  JS::RootedFunction function(cx,js::NewFunctionWithReserved(cx,NativeFetchAbort,0,0,nullptr));
  if (!function) return false;
  JS::RootedObject callback(cx,JS_GetFunctionObject(function));
  js::SetFunctionNativeReserved(callback,0,JS::ObjectValue(*operation));
  JS::RootedValue algorithm(cx,JS::ObjectValue(*callback));
  JS::SetReservedSlot(operation,FAlgorithm,algorithm);
  return AddAbortAlgorithm(cx,signal,algorithm);
}
}

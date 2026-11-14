#pragma once
#include "resource_network/state.h"
#include "resource_network/readable_stream.h"
#include "resource_network/model_webtransport.h"
#include "js/GCVector.h"
#include <new>

namespace ruyijs::resource_network {
enum TransportSlot { WTNative,WTFactories,WTReady,WTClosed,WTDraining,WTDatagrams,
  WTBidi,WTUni,WTBidiController,WTUniController,WTRequests,WTEndpoints,WTPhase,WTProtocol,WTWasReady,WTNextGroup,WTClosing,WTTask,WTSlots };
enum class TransportPhase { Connecting,Connected,Closed,Failed };
enum EndpointSlot { WESession,WEKind,WEStream,WEReadable,WEWritable,WEReadController,
  WEWriteController,WEGroup,WEOrder,WEReadPending,WESlots };
enum class EndpointKind { ByteStream,Datagram,IncomingBidi,IncomingUni };
enum DatagramSlot { WDSession,WDReadable,WDWritable,WDMaxSize,WDIncomingAge,WDOutgoingAge,
  WDIncomingBuffered,WDOutgoingBuffered,WDSlots };
enum RequestSlot { WRTicket,WRAction,WRPromise,WREndpoint,WRData,WROffset,WRNext,WRGroup,WROrder,WRCode,WRSize,WRSlots };
enum TransportMethod { WTStats,WTExport,WTClose,WTCreateBidi,WTCreateUni,WTCreateGroup };
enum TransportAttribute { WTGetReady,WTGetReliability,WTGetCongestion,WTGetResponseHeaders,WTGetProtocol,WTGetClosed,
  WTGetDraining,WTGetDatagrams,WTGetBidi,WTGetUni };
extern const JSClass kTransportClass,kTransportEndpointClass,kTransportDatagramsClass,
  kTransportBidiClass,kTransportGroupClass,kTransportRequestClass,kTransportErrorDataClass;
bool InstallWebTransport(State&,PageHelpers&);
bool TransportConstruct(JSContext*,unsigned,JS::Value*);
bool TransportMethodCall(JSContext*,unsigned,JS::Value*);
bool TransportGet(JSContext*,unsigned,JS::Value*);
bool TransportErrorConstruct(JSContext*,unsigned,JS::Value*);
bool TransportErrorGet(JSContext*,unsigned,JS::Value*);
bool TransportDatagramGet(JSContext*,unsigned,JS::Value*);
bool TransportDatagramSet(JSContext*,unsigned,JS::Value*);
bool TransportCreateWritable(JSContext*,unsigned,JS::Value*);
bool TransportBidiGet(JSContext*,unsigned,JS::Value*);
bool TransportEndpointGet(JSContext*,unsigned,JS::Value*);
bool TransportEndpointSet(JSContext*,unsigned,JS::Value*);
bool TransportEndpointStats(JSContext*,unsigned,JS::Value*);
bool TransportGroupStats(JSContext*,unsigned,JS::Value*);
State* TransportState(JSContext*,JS::HandleObject);
WebTransportSession* TransportSession(JSObject*);
bool TransportAllocate(State&,JS::HandleObject,const WebTransportOptions&,std::string*);
void TransportFinalize(JSObject*) noexcept;
void RetireTransports(State&) noexcept;
void TransportRememberStats(JSObject*,const WebTransportEvent&);
const WebTransportEvent* TransportFinalStats(JSObject*);
bool TransportClosedStats(JSContext*,JS::HandleObject,JS::HandleObject);
bool TransportSchedule(JSContext*,JS::HandleObject);
void TransportUpdateTaskRoot(JSObject*);
bool TransportProcess(JSContext*,JS::HandleObject);
bool TransportFail(JSContext*,JS::HandleObject,const char*);
bool TransportCloseSession(JSContext*,JS::HandleObject,uint32_t,const std::string&);
bool TransportClosed(JSContext*,JS::HandleObject,const WebTransportEvent&);
bool TransportResult(JSContext*,JS::HandleObject,const WebTransportEvent&);
bool TransportDeliver(JSContext*,JS::HandleObject,const WebTransportEvent&);
bool TransportStreamError(JSContext*,JS::HandleObject,const WebTransportEvent&);
bool TransportSubmit(JSContext*,JS::HandleObject,const WebTransportCommand&,
    JS::HandleObject promise,JS::HandleObject endpoint,JS::HandleValue data,
    JS::HandleValue group,JS::HandleValue order);
bool TransportRetry(JSContext*,JS::HandleObject,JS::HandleObject);
bool TransportErrorValue(State&,const char*,bool,JS::HandleValue,JS::MutableHandleValue);
uint8_t TransportStreamErrorCode(State&,JS::HandleValue);
bool TransportDomError(State&,const char*,const char*,JS::MutableHandleValue);
bool TransportThrowDom(State&,const char*,const char*);
bool TransportBindingError(JSContext*,const char*,const char*);
bool TransportRejectPending(JSContext*,JS::MutableHandleValue);
bool TransportOptions(JSContext*,const JS::CallArgs&,WebTransportOptions*);
bool TransportSendOptions(JSContext*,JS::HandleObject,JS::HandleValue,JS::MutableHandleValue,int64_t*);
bool TransportBytes(JSContext*,JS::HandleValue,std::string*);
bool TransportNewEndpoints(State&,JS::HandleObject,uint64_t,bool,JS::HandleValue,int64_t,JS::MutableHandleObject);
bool TransportInitialStreams(State&,JS::HandleObject);
bool TransportNewDatagramWritable(State&,JS::HandleObject,JS::HandleValue,int64_t,JS::MutableHandleObject);
bool TransportStatsObject(JSContext*,const WebTransportEvent&,bool,bool,JS::MutableHandleValue);
bool TransportPrototype(JSContext*,const JS::CallArgs&,unsigned,JS::MutableHandleObject);
bool TransportInstallStreams(State&);
bool TransportFinishFace(State&,WebFace&,const char*);
bool TransportNewEndpoint(State&,JS::HandleObject,uint64_t,EndpointKind,JS::MutableHandleObject);
bool TransportReadStream(State&,JS::HandleObject,bool,JS::MutableHandleObject);
bool TransportWriteStream(State&,JS::HandleObject,bool,JS::MutableHandleObject);
bool TransportResolve(JSContext*,JS::HandleObject,JS::HandleValue);
bool TransportReject(JSContext*,JS::HandleObject,JS::HandleValue);
inline TransportPhase TransportStatus(JSObject* value) {return TransportPhase(JS::GetReservedSlot(value,WTPhase).toInt32());}
inline void SetTransportStatus(JSObject* value,TransportPhase phase) {JS::SetReservedSlot(value,WTPhase,JS::Int32Value(int(phase)));}
template<class F> bool TransportGuard(JSContext* cx,F&& fn) {
  return fn();
}
}

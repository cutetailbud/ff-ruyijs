#ifndef RUYIJS_RESOURCE_XHR_H
#define RUYIJS_RESOURCE_XHR_H
#include "resource_network/state.h"
#include "resource_network/async_http.h"
#include "domain/resource_network/xml_http_request.h"
#include <optional>

namespace ruyijs::resource_network {
enum XhrSlot { XRecord, XUpload, XLoadStart, XProgress, XAbort, XError,
               XLoad, XTimeoutHandler, XLoadEnd, XReadyHandler, XResponseCache, XSlotCount };
enum XhrMember { XReady, XTimeout, XCredentials, XUploadMember, XResponseURL,
                 XStatus, XStatusText, XResponseType, XResponse, XText, XXml, XMozAnon, XMozSystem };
enum XhrOperation { XOpen, XSend, XAbortCall, XSetHeader, XGetHeader, XGetAll, XOverride };
struct XhrRecord {
  State* owner = nullptr;
  XhrRecord *previous = nullptr, *next = nullptr;
  void* eventRecord = nullptr;
  ruyijs::domain::resource_network::XmlHttpRequestState state;
  std::string method, url, responseType, mime, outgoing;
  HeaderPairs requestHeaders;
  HttpResult response;
  std::optional<HttpResult> prepared;
  AsyncHttpTicket ticket;  // the asynchronous transfer abort() and open() cancel
  bool urgent = false;  // send() ran while handling user input (Firefox UrgentStart)
};
extern const JSClass kXhrClass, kXhrUploadClass;
XhrRecord* XhrReceiver(JSContext*, const JS::CallArgs&, JS::MutableHandleObject, bool upload = false);
XhrRecord& XhrData(JS::HandleObject);
int XhrSelector(const JS::CallArgs&);
void DetachXhrs(State&);
// Stops the record's in-flight asynchronous transfer, if any (issue #43).
void XhrCancelTransfer(XhrRecord&);
bool XhrException(State&, const char*);
bool XhrFire(JS::HandleObject, const char*);
bool XhrUploadFire(JS::HandleObject, const char*);
bool XhrChange(JS::HandleObject, int);
bool XhrConstructor(JSContext*, unsigned, JS::Value*);
bool XhrIllegalConstructor(JSContext*, unsigned, JS::Value*);
bool XhrGet(JSContext*, unsigned, JS::Value*);
bool XhrSet(JSContext*, unsigned, JS::Value*);
bool XhrHandlerGet(JSContext*, unsigned, JS::Value*);
bool XhrHandlerSet(JSContext*, unsigned, JS::Value*);
bool XhrRequest(JSContext*, unsigned, JS::Value*);
bool XhrHeader(JSContext*, unsigned, JS::Value*);
bool XhrDeliver(JS::HandleObject);
bool XhrResponse(JSContext*, JS::HandleObject, XhrMember, JS::MutableHandleValue);
const char* XhrMemberName(int);
const char* XhrHandlerName(int);
}
#endif

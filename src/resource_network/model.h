#ifndef RUYIJS_RESOURCE_MODEL_H
#define RUYIJS_RESOURCE_MODEL_H
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
#include "resource_network/http_context.h"

namespace ruyijs::resource_network {
struct CacheScope;
struct HttpObservation;
struct HttpResult;
enum class ErrorCode { None, OutOfMemory, Type, Range, InvalidState };
struct Status { ErrorCode code = ErrorCode::None; const char* message = ""; };
enum class BodyKind { Text, Bytes };
using HeaderPairs = std::vector<std::pair<std::string, std::string>>;
struct ResourceInput {
  std::string url, body;
  BodyKind kind = BodyKind::Text;
  int status = 200;
  HeaderPairs headers;
};
using ResourceSnapshot = ResourceInput;
struct ResourceStats { uint64_t resources = 0, bytes = 0; };
struct NetLogRecord {
  uint64_t id = 0;
  std::string method, url, requestBody;
  int status = 0;
  bool matched = false;
  std::vector<std::string> cookieNames;
  std::string error;
};
using NetLogRecords = std::vector<NetLogRecord>;
struct Limits {
  static constexpr size_t resources = 10000, key_bytes = 16384;
  static constexpr size_t body_bytes = 8 * 1024 * 1024, resource_bytes = 64 * 1024 * 1024;
  static constexpr size_t header_fields = 256, header_bytes = 64 * 1024;
  static constexpr size_t header_records = 10000, header_iterators = 10000;
  static constexpr size_t arena_fields = 100000, arena_bytes = 16 * 1024 * 1024;
};
class Model final {
 public:
  Model() noexcept = default;
  ~Model() noexcept;
  Model(const Model&) = delete;
  Model& operator=(const Model&) = delete;
  Status Initialize() noexcept;
  // Realm-local view over another model's one aggregate. A child browsing
  // context gets its own Model object but reads and writes the same resource
  // catalog and the same NetLog, so there is still exactly one of each. The
  // parent model must outlive the view; the view never destroys the aggregate.
  Status ShareFrom(Model& parent) noexcept;
  // Origin is a validated tuple origin captured by the existing origin owner.
  Status OpenCacheScope(std::string_view origin, CacheScope**) noexcept;
  Status Put(const ResourceInput& input, ResourceStats* result) noexcept;
  Status Get(std::string_view url, ResourceSnapshot* result, bool* found) const noexcept;
  // Record goes through the aggregate's own record(): one lookup that also
  // appends to the single NetLog owner. Get stays a pure read.
  Status Record(std::string_view method, std::string_view url, std::string_view body,
                ResourceSnapshot* result, bool* found, uint64_t* recordedId = nullptr,
                bool matchResource = true) noexcept;
  using ObjectUrlPort = bool (*)(void*, const std::string&, std::string*, std::string*);
  void SetObjectUrlPort(ObjectUrlPort port, void* context) {
    objectUrlPort_ = port; objectUrlContext_ = context;
  }
  bool ReadObjectUrl(const std::string& url, std::string* bytes, std::string* type) {
    return objectUrlPort_ && objectUrlPort_(objectUrlContext_, url, bytes, type);
  }
  void RecordResponse(uint64_t id, int status) noexcept;
  void RecordError(uint64_t id, const std::string& error) noexcept;
  void RecordCookies(uint64_t id,const HeaderPairs&) noexcept;
  void SetHttpContext(StorageArea* store,HttpContextPort port,void* context) {
    cookieStore_=store;httpPort_=port;httpContext_=context;
  }
  StorageArea* Cookies() const {return cookieStore_;}
  bool ReadHttpContext(HttpContext* value) {return !httpPort_||httpPort_(httpContext_,value);}
  using HttpObservationPort=bool (*)(void*,const HttpObservation&);
  using HttpClockPort=double (*)(void*);
  void SetHttpObservation(HttpObservationPort port,void* context,HttpClockPort clock,void* clockContext){
    observationPort_=port;observationContext_=context;httpClock_=clock;httpClockContext_=clockContext;
  }
  bool ObserveHttp(const HttpObservation& value){return !observationPort_||observationPort_(observationContext_,value);}
  double HttpNow(){return httpClock_?httpClock_(httpClockContext_):0;}
  bool HasHttpObservation()const{return observationPort_!=nullptr;}
  // A Worker realm's model owns no catalog, NetLog or Cookie jar: this port
  // hands each whole request to the parent Context's one Resource Network
  // owner, which runs PerformHttp there. Null everywhere else.
  struct RemoteRequest;
  using RemoteHttpPort=bool (*)(void*,const RemoteRequest&,HttpResult*,bool* ok);
  void SetRemoteHttp(RemoteHttpPort port,void* context){remoteHttp_=port;remoteHttpContext_=context;}
  bool HasRemoteHttp()const{return remoteHttp_!=nullptr;}
  bool RemoteHttp(const RemoteRequest& request,HttpResult* result,bool* ok){
    return remoteHttp_(remoteHttpContext_,request,result,ok);
  }
  Status Log(NetLogRecords* result) const noexcept;
  // Drops the recorded requests; ids keep counting from the last one (#46).
  Status ClearLog() noexcept;
  ResourceStats Inspect() const noexcept;
  void Clear() noexcept;
 private:
  struct Impl;
  Impl* impl_ = nullptr;
  ObjectUrlPort objectUrlPort_ = nullptr;
  void* objectUrlContext_ = nullptr;
  StorageArea* cookieStore_=nullptr;  // Borrowed from the one StorageArea.
  HttpContextPort httpPort_=nullptr;
  void* httpContext_=nullptr;
  HttpObservationPort observationPort_=nullptr;
  void* observationContext_=nullptr;
  HttpClockPort httpClock_=nullptr;
  void* httpClockContext_=nullptr;
  RemoteHttpPort remoteHttp_=nullptr;
  void* remoteHttpContext_=nullptr;
};
}
#endif

#include "web_workers/clone.h"
#include "document_model/exception.h"
#include "runtime/pipe.h"
#include "runtime/web_kit.h"
#include "jsapi.h"
#include "js/Array.h"
#include "js/ForOfIterator.h"
#include "js/PropertyAndElement.h"
#include "js/StructuredClone.h"
#include <cstdlib>
#include <cstring>

namespace ruyijs::web_workers {
bool WorkerTokenEnabled() {
  static const bool enabled = [] { const char* value = std::getenv("RUYIJS_WORKER_TOKEN");
    return value && std::strcmp(value, "1") == 0; }();
  return enabled;
}
namespace {
constexpr char kMessage = '\x01';
constexpr uint32_t kPortTag = JS_SCTAG_USER_MIN + 1;
PortCheck portCheck = nullptr;
PortBridge portBridge = nullptr;
uint32_t portBase = 0, portCount = 0;
// One clone's closure: the clone error flag and its port transfer state.
struct Closure {
  bool error = false;
  std::vector<uint32_t>* bridged = nullptr;
  const PortSink* sink = nullptr;
};
void CloneError(JSContext*, uint32_t, void* raw, const char*) {
  static_cast<Closure*>(raw)->error = true;
}
bool CanTransfer(JSContext*, JS::HandleObject object, bool* sameProcess, void* raw) {
  auto& closure = *static_cast<Closure*>(raw);
  *sameProcess = false;
  return closure.bridged && portCheck && portCheck(object);
}
bool WriteTransfer(JSContext*, JS::HandleObject object, void* raw, uint32_t* tag,
                   JS::TransferableOwnership* ownership, void** content, uint64_t* extra) {
  auto& closure = *static_cast<Closure*>(raw);
  if (!closure.bridged || !portBridge) return false;
  const uint32_t id = portBase | (++portCount & 0x3fffffffu);
  if (!portBridge(object, id)) return false;
  closure.bridged->push_back(id);
  *tag = kPortTag; *ownership = JS::SCTAG_TMO_CUSTOM;
  *content = nullptr; *extra = id;
  return true;
}
bool ReadTransfer(JSContext* cx, JSStructuredCloneReader*, const JS::CloneDataPolicy&, uint32_t tag,
                  void*, uint64_t extra, void* raw, JS::MutableHandleObject output) {
  auto& closure = *static_cast<Closure*>(raw);
  const PortSink* sink = closure.sink;
  if (tag != kPortTag || !sink || !sink->factory) return false;
  const uint32_t id = uint32_t(extra);
  output.set(sink->factory(sink->context, id));
  if (!output) return false;
  if (sink->ids) sink->ids->push_back(id);
  if (sink->ports) {
    JS::RootedObject ports(cx, sink->ports), port(cx, output);
    if (!JS_WrapObject(cx, &port)) return false;
    uint32_t length = 0;
    if (!JS::GetArrayLength(cx, ports, &length) ||
        !JS_DefineElement(cx, ports, length, port, JSPROP_ENUMERATE)) return false;
  }
  if (!JS_WrapObject(cx, output)) return false;
  return true;
}
void FreeTransfer(uint32_t, JS::TransferableOwnership, void*, uint64_t, void*) {}
const JSStructuredCloneCallbacks kCallbacks = {nullptr, nullptr, CloneError, ReadTransfer,
                                               WriteTransfer, FreeTransfer, CanTransfer};
bool TransferList(JSContext* cx, JS::HandleValue options, JS::MutableHandleValue output) {
  output.setUndefined();
  if (options.isNullOrUndefined()) return true;
  if (!options.isObject()) return WebKit::Fail(cx, "Worker transfer options require an object");
  JS::ForOfIterator iterator(cx);
  if (!iterator.init(options, JS::ForOfIterator::AllowNonIterable)) return false;
  if (!iterator.valueIsIterable()) {
    JS::RootedObject dictionary(cx, &options.toObject());
    JS::RootedValue transfer(cx);
    if (!JS_GetProperty(cx, dictionary, "transfer", &transfer)) return false;
    if (transfer.isUndefined()) return true;
    if (!transfer.isObject()) return WebKit::Fail(cx, "Worker transfer must be a sequence of objects");
    if (!iterator.init(transfer)) return false;
  }
  JS::RootedObject list(cx, JS::NewArrayObject(cx, 0));
  if (!list) return false;
  for (uint32_t index = 0;; ++index) {
    JS::RootedValue item(cx); bool done = false;
    if (!iterator.next(&item, &done)) return false;
    if (done) break;
    if (!item.isObject()) return WebKit::Fail(cx, "Worker transfer entries must be objects");
    if (index >= 65536) return WebKit::Fail(cx, "Worker transfer list limit exceeded");
    if (!JS_DefineElement(cx, list, index, item, JSPROP_ENUMERATE)) return false;
  }
  output.setObject(*list);
  return true;
}
}
void SetPortTransfer(PortCheck check, PortBridge bridge, uint32_t idBase) {
  portCheck = check; portBridge = bridge; portBase = idBase;
}
bool EncodeMessage(JSContext* cx, JS::HandleObject exception, JS::HandleValue value,
                   JS::HandleValue options, std::string* frame, std::vector<uint32_t>* bridged) {
  JS::RootedValue transfer(cx);
  if (!TransferList(cx, options, &transfer)) return false;
  Closure closure;
  closure.bridged = bridged;
  JSAutoStructuredCloneBuffer buffer(JS::StructuredCloneScope::DifferentProcess, &kCallbacks, &closure);
  if (!buffer.write(cx, value, transfer, JS::CloneDataPolicy(), &kCallbacks, &closure)) {
    if (!closure.error && JS_IsExceptionPending(cx)) return false;
    JS_ClearPendingException(cx);
    return document_model::ThrowException(cx, exception, "DataCloneError", "The object could not be cloned");
  }
  if (buffer.data().Size() > Pipe::kMaxFrame - 5)
    return WebKit::Fail(cx, "Worker message exceeds the 16 MiB protocol limit");
  frame->resize(buffer.data().Size() + 5);
  (*frame)[0] = kMessage;
  for (unsigned index = 0; index < 4; ++index)
    (*frame)[index + 1] = char(buffer.version() >> (8 * index));
  auto cursor = buffer.data().Start();
  return buffer.data().ReadBytes(cursor, frame->data() + 5, buffer.data().Size());
}
bool DecodeMessage(JSContext* cx, std::string_view frame, JS::MutableHandleValue value, char kind,
                   const PortSink* sink) {
  if (frame.size() < 5 || frame.size() > Pipe::kMaxFrame || frame[0] != kind)
    return WebKit::Fail(cx, "Invalid Worker message frame");
  uint32_t version = 0;
  for (unsigned index = 0; index < 4; ++index)
    version |= uint32_t(static_cast<unsigned char>(frame[index + 1])) << (8 * index);
  if (version != JS_STRUCTURED_CLONE_VERSION)
    return WebKit::Fail(cx, "Worker structured clone version differs from its parent");
  JSStructuredCloneData data(JS::StructuredCloneScope::DifferentProcess);
  if (!data.AppendBytes(frame.data() + 5, frame.size() - 5)) {
    JS_ReportOutOfMemory(cx); return false;
  }
  Closure closure;
  closure.sink = sink;
  return JS_ReadStructuredClone(cx, data, version, JS::StructuredCloneScope::DifferentProcess,
                                value, JS::CloneDataPolicy(), &kCallbacks, &closure);
}
bool EncodeMessageWithToken(JSContext* cx, JS::HandleObject exception, JS::HandleValue value,
                            JS::HandleValue options, const WorkerMessageToken& token, std::string* frame) {
  if (!token) return WebKit::Fail(cx, "Worker token identity is invalid");
  std::string clone; if (!EncodeMessage(cx, exception, value, options, &clone)) return false;
  frame->assign(1, '\x02');
  for (unsigned i=0;i<8;++i) frame->push_back(char(token.workerId>>(8*i)));
  for (unsigned i=0;i<8;++i) frame->push_back(char(token.sequence>>(8*i)));
  frame->push_back(char(token.direction)); frame->append(clone.data()+1, clone.size()-1); return true;
}
bool DecodeMessageWithToken(JSContext* cx, std::string_view frame, WorkerMessageToken* token, JS::MutableHandleValue value) {
  if (frame.size()<18 || frame[0]!='\x02') return false;
  token->workerId=token->sequence=0;
  for(unsigned i=0;i<8;++i){token->workerId|=uint64_t(static_cast<unsigned char>(frame[1+i]))<<(8*i);token->sequence|=uint64_t(static_cast<unsigned char>(frame[9+i]))<<(8*i);}
  token->direction=uint8_t(frame[17]); std::string clone(1,'\x01');clone.append(frame.data()+18,frame.size()-18);
  if(!*token || !DecodeMessage(cx,clone,value))return WebKit::Fail(cx,"Invalid Worker token frame");return true;
}
bool EncodeSharedMessage(JSContext* cx, JS::HandleObject exception, uint32_t id,
                         JS::HandleValue value, JS::HandleValue transfer, std::string* frame,
                         std::vector<uint32_t>* bridged) {
  std::string clone;
  if (!EncodeMessage(cx, exception, value, transfer, &clone, bridged)) return false;
  frame->assign(1, kSharedMessage);
  for (unsigned index = 0; index < 4; ++index) frame->push_back(char(id >> (8 * index)));
  frame->append(clone.data() + 1, clone.size() - 1);  // drop '\x01', keep version + data
  return true;
}
bool DecodeSharedMessage(JSContext* cx, std::string_view frame, uint32_t* id,
                         JS::MutableHandleValue value, const PortSink* sink) {
  if (!DecodeSharedPortFrame(frame, kSharedMessage, id)) return false;
  std::string clone(1, '\x01');
  clone.append(frame.data() + 5, frame.size() - 5);
  return DecodeMessage(cx, clone, value, '\x01', sink);
}
}

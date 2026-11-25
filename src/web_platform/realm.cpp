#include "web_platform/realm.h"
#include "web_platform/object_url_remote.h"
#include "web_platform/body_factories.h"
#include "web_platform/text_decoder_stream.h"
#include "web_platform/compression_stream.h"

#include "web_platform/state.h"
#include "web_platform/object_url_store.h"
#include "runtime/page_helpers.h"
#include "jsapi.h"
#include "js/Realm.h"

namespace ruyijs::web_platform {
// The interface objects keep the realm's prototype identity: the class is only
// a brand, and the reserved slots hold the href and the parameter list.
const JSClass kUrlClass = {"URL", JSCLASS_HAS_RESERVED_SLOTS(kUrlSlotCount)};
const JSClass kSearchParamsClass = {
    "URLSearchParams", JSCLASS_HAS_RESERVED_SLOTS(kParamsSlotCount)};
// Blob and File are separate brands over one byte store; File's prototype
// chains to Blob.prototype in the installer.
const JSClass kBlobClass = {"Blob", JSCLASS_HAS_RESERVED_SLOTS(kBlobSlotCount)};
const JSClass kFileClass = {"File", JSCLASS_HAS_RESERVED_SLOTS(kBlobSlotCount)};

State::State(JSContext* context, JS::HandleObject page, PageHelpers& helpers)
    : cx(context),
      kit(context, page, helpers, this),
      url(context),
      params(context),
      textEncoder(context),
      textDecoder(context),
      textDecoderStream(context),
      compressionStream(context),
      decompressionStream(context),
      blob(context),
      file(context),
      reader(context),
      readerSync(context),
      formData(context),
      urlPattern(context),
      textEncoderStream(context), fileList(context),
      paramsIteratorPrototype(context),
      formDataIteratorPrototype(context),
      objectUrlReferences(context) {}

Realm::Realm(JSContext* cx, JS::HandleObject page, PageHelpers& helpers,
             interaction::Realm& events, Realm* source)
    : state_(std::make_unique<State>(cx, page, helpers)) {
  state_->events = &events;
  if (source) {
    state_->formFieldsPort = source->state_->formFieldsPort;
    state_->formFieldsContext = source->state_->formFieldsContext;
  }
  if (source) state_->objectUrls = source->state_->objectUrls;
  if (state_->objectUrls) state_->objectUrlOwner = state_->objectUrls->NewOwner();
}
void Realm::SetLineBreakPort(bool (*crlf)(void*), void* context) {
  state_->lineBreak = crlf;
  state_->lineBreakContext = context;
}
void Realm::SetFormFieldsPort(FormFieldsPort port, void* context) {
  state_->formFieldsPort = port;
  state_->formFieldsContext = context;
}
void Realm::SetByteStreamPort(bool (*port)(void*, std::string_view, JS::MutableHandleObject), void* context) {
  state_->byteStream = port;
  state_->byteStreamContext = context;
}
Realm::~Realm() { ClearDocumentUrls(); }

void Realm::SetObjectUrlPorts(bool (*origin)(void*, std::string*), void* context,
                               bool (*mediaBrand)(JSObject*)) {
  state_->objectUrlOrigin = origin;
  state_->objectUrlOriginContext = context;
  state_->objectUrlMediaBrand = mediaBrand;
}
bool Realm::ReadObjectUrl(const std::string& url, std::string* bytes, std::string* type) {
  // A Worker realm reads through the origin's store in the creating process.
  if (!state_->objectUrls && state_->objectUrlExchange) {
    JSAutoRealm realm(state_->cx, state_->kit.page);
    return ReadRemoteObjectUrl(*state_, url, bytes, type);
  }
  if (!state_->objectUrls) return false;
  JSAutoRealm realm(state_->cx, state_->kit.page);
  std::string origin;
  return ObjectUrlOrigin(*state_, &origin) &&
      state_->objectUrls->Read(url, origin, state_->objectUrlOwner, bytes, type);
}
void Realm::ClearDocumentUrls() {
  if (state_->objectUrls) state_->objectUrls->ClearOwner(state_->objectUrlOwner);
}

bool Realm::InstallEncoding() {
  State& state = *state_;
  const JSAutoRealm realm(state.cx, state.kit.page);
  return InstallTextEncoder(state, *state.kit.helpers) &&
         InstallTextDecoder(state, *state.kit.helpers) &&
         InstallTextDecoderStream(state,*state.kit.helpers) &&
         InstallTextEncoderStream(state) &&
         InstallCompressionStreams(state,*state.kit.helpers);
}

bool Realm::InstallCore() {
  State& state = *state_;
  const JSAutoRealm realm(state.cx, state.kit.page);
  return InstallUrl(state, *state.kit.helpers) &&
         InstallSearchParams(state, *state.kit.helpers) &&
         InstallUrlPattern(state) &&
         InstallEncoding() &&
         InstallBlob(state, *state.kit.helpers) &&
         InstallFile(state, *state.kit.helpers);
}
bool Realm::InstallWorker(bool readerSync) {
  if (!state_->objectUrlExchange) return WebKit::Fail(state_->cx, "Worker object URL exchange is not bound");
  const JSAutoRealm realm(state_->cx, state_->kit.page);
  return InstallCore() && InstallFileReader(*state_, *state_->kit.helpers) && InstallFileList(*state_) && InstallDirectory(*state_) &&
         (!readerSync || InstallFileReaderSync(*state_, *state_->kit.helpers)) &&
         InstallFormData(*state_, *state_->kit.helpers) && InstallBodyFactories(*state_);
}
bool Realm::Install() {
  State& state = *state_;
  const JSAutoRealm realm(state.cx, state.kit.page);
  if (!state.objectUrls) {
    state.objectUrls = std::make_shared<ObjectUrlStore>(state.cx);
    state.objectUrlOwner = state.objectUrls->NewOwner();
  }
  return InstallCore() && InstallUrlWindowAlias(state) &&
         InstallFileReader(state, *state.kit.helpers) && InstallFileList(state) && InstallDirectory(state) &&
         InstallFormData(state, *state.kit.helpers) && InstallBodyFactories(state);
}
}  // namespace ruyijs::web_platform

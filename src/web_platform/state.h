#ifndef RUYIJS_WEB_PLATFORM_STATE_H
#define RUYIJS_WEB_PLATFORM_STATE_H
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
#include "interaction/realm.h"
#include "web_platform/form_submission.h"
#include "web_platform/form_query.h"
#include "web_platform/object_url_session.h"
#include "js/RootingAPI.h"
#include "js/TypeDecls.h"
#include "runtime/web_kit.h"

struct JSContext;
namespace ruyijs { class PageHelpers; }
namespace ruyijs::web_platform {
class ObjectUrlStore;

// The URL interface keeps only the serialized href and the cached live
// URLSearchParams object. Every component is read back through the one domain
// parser (UrlValue), so href, the parts and the parameter list can never drift.
// URLSearchParams keeps the encoded query plus the URL it was created from, so a
// mutation writes back to exactly one owner.
inline constexpr int32_t kUrlHrefSlot = 0;
inline constexpr int32_t kUrlParamsSlot = 1;
inline constexpr int32_t kUrlSlotCount = 2;
inline constexpr int32_t kParamsQuerySlot = 0;
inline constexpr int32_t kParamsOwnerSlot = 1;
inline constexpr int32_t kParamsSlotCount = 2;
// One codec handle owns BOM/streaming state; its class finalizer releases it.
inline constexpr int32_t kDecoderEncodingSlot = 0;
inline constexpr int32_t kDecoderFatalSlot = 1;
inline constexpr int32_t kDecoderIgnoreBomSlot = 2;
inline constexpr int32_t kDecoderHandleSlot = 3;
inline constexpr int32_t kDecoderSlotCount = 4;

extern const JSClass kUrlClass, kSearchParamsClass, kParamsIteratorClass;
extern const JSClass kTextEncoderClass, kTextDecoderClass;
// Blob and File share the byte-slot layout and the Blob receiver brand; File
// adds its name and lastModified and keeps its own class/prototype.
inline constexpr int32_t kBlobBytesSlot = 0;
inline constexpr int32_t kBlobTypeSlot = 1;
inline constexpr int32_t kFileSlotName = 2;
inline constexpr int32_t kFileSlotModified = 3;
inline constexpr int32_t kBlobSlotCount = 4;
inline constexpr size_t kBlobByteLimit = 8 * 1024 * 1024;
extern const JSClass kBlobClass, kFileClass;
// FileReader is an EventTarget subclass; its prototype chains to
// EventTarget.prototype and its state lives in the reader slots (readyState,
// result, error, the registered handler callbacks and a generation counter that
// invalidates a superseded read).
inline constexpr int32_t kReaderStateSlot = 0;
inline constexpr int32_t kReaderResultSlot = 1;
inline constexpr int32_t kReaderErrorSlot = 2;
inline constexpr int32_t kReaderHandlersSlot = 3;
inline constexpr int32_t kReaderGenerationSlot = 4;
inline constexpr int32_t kReaderSlotCount = 5;
inline constexpr int32_t kReaderReadyEmpty = 0, kReaderReadyLoading = 1,
                         kReaderReadyDone = 2;
extern const JSClass kReaderClass;
// FormData is one ordered entry list; every method reads and writes that array,
// so the pairs the iterator yields and the pairs a send serializes are the same.
inline constexpr int32_t kFormDataEntriesSlot = 0;
inline constexpr int32_t kFormDataSlotCount = 1;
extern const JSClass kFormDataClass, kFormDataIteratorClass;

// Field ids for the URL accessors and operation ids for the URLSearchParams
// members. The installer and the behaviour files share them so a member is
// defined once.
enum UrlField {
  kHref = 0, kProtocol, kHost, kHostname, kPort, kPathname, kSearch, kHash,
  kOrigin, kSearchParams, kUsername, kPassword
};
enum ParamsMethodKind {
  kAppend = 0, kDelete, kGet, kGetAll, kHas, kSet, kSort, kToString, kForEach
};

struct State {
  JSContext* cx;
  WebKit kit;
  WebFace url, params, textEncoder, textDecoder, textDecoderStream,
      compressionStream, decompressionStream, blob, file, reader, readerSync, formData, urlPattern, textEncoderStream, fileList;
  JS::PersistentRootedObject paramsIteratorPrototype, formDataIteratorPrototype;
  bool (*lineBreak)(void*) = nullptr;  // true: CR LF (Realm::SetLineBreakPort)
  void* lineBreakContext = nullptr;
  JS::PersistentRootedObject objectUrlReferences;
  uint32_t nextObjectUrlReference = 0;
  ObjectUrlExchange objectUrlExchange = nullptr;
  void* objectUrlExchangeContext = nullptr;
  // The owning realm's event tree: FileReader attaches to it and fires there.
  interaction::Realm* events = nullptr;
  FormFieldsPort formFieldsPort = nullptr;
  // Blob.stream(): the Resource Network stream factory (runtime/blob_stream_port.cpp).
  bool (*byteStream)(void*, std::string_view, JS::MutableHandleObject) = nullptr;
  void* byteStreamContext = nullptr;
  void* formFieldsContext = nullptr;
  std::shared_ptr<ObjectUrlStore> objectUrls;
  uint64_t objectUrlOwner = 0;
  bool (*objectUrlOrigin)(void*, std::string*) = nullptr;
  void* objectUrlOriginContext = nullptr;
  bool (*objectUrlMediaBrand)(JSObject*) = nullptr;
  State(JSContext* cx, JS::HandleObject page, PageHelpers& helpers);
};

bool UrlConstructor(JSContext*, unsigned, JS::Value*);
bool UrlGetter(JSContext*, unsigned, JS::Value*);
bool UrlSetter(JSContext*, unsigned, JS::Value*);
bool UrlParse(JSContext*, unsigned, JS::Value*);
bool UrlCanParse(JSContext*, unsigned, JS::Value*);
bool ParamsConstructor(JSContext*, unsigned, JS::Value*);
bool ParamsMethod(JSContext*, unsigned, JS::Value*);
bool ParamsGetter(JSContext*, unsigned, JS::Value*);
bool ParamsIterator(JSContext*, unsigned, JS::Value*);
bool ParamsNext(JSContext*, unsigned, JS::Value*);
bool ParamsIdentity(JSContext*, unsigned, JS::Value*);

// A live URLSearchParams over a query string; a null owner leaves it standalone.
JSObject* NewSearchParams(State& state, const std::string& query, JS::HandleObject owner);
// The URL object the parameters write back to, or null when standalone.
bool ParamsOwner(JSContext* cx, JS::HandleObject params, JS::MutableHandleObject owner);
bool IsSearchParams(JS::HandleValue value);
// Reads a constructor init value (an iterable of pairs or a record) into
// the ordered name/value list the query is serialized from.
bool ConvertInit(JSContext* cx, JS::HandleValue init,
                 std::vector<std::pair<std::string, std::string>>* entries);
bool InstallUrl(State& state, PageHelpers& helpers);
// Window-only LegacyWindowAlias of the realm's URL interface object.
bool InstallUrlWindowAlias(State& state);
bool CreateObjectUrl(JSContext*, unsigned, JS::Value*);
bool RevokeObjectUrl(JSContext*, unsigned, JS::Value*);
bool ObjectUrlOrigin(State&, std::string*);
bool InstallSearchParams(State& state, PageHelpers& helpers);
// URLPattern over the Firefox urlpattern crate (url_pattern*.cpp, docs/owners/WEB_PLATFORM.md).
bool InstallUrlPattern(State& state);
// TextEncoderStream over the native Transform (text_encoder_stream.cpp).
bool InstallTextEncoderStream(State& state);

// The Encoding Standard interfaces. TextEncoder is stateless; TextDecoder
// carries the label, fatal/ignoreBOM and the streaming tail in its slots.
enum DecoderField { kDecoderEncodingField = 0, kDecoderFatalField, kDecoderIgnoreBomField };
bool TextEncoderConstructor(JSContext*, unsigned, JS::Value*);
bool Encode(JSContext*, unsigned, JS::Value*);
bool EncodeInto(JSContext*, unsigned, JS::Value*);
bool TextEncoderGetter(JSContext*, unsigned, JS::Value*);
bool TextDecoderConstructor(JSContext*, unsigned, JS::Value*);
bool Decode(JSContext*, unsigned, JS::Value*);
bool TextDecoderGetter(JSContext*, unsigned, JS::Value*);
bool InstallTextEncoder(State& state, PageHelpers& helpers);
bool InstallTextDecoder(State& state, PageHelpers& helpers);

// Blob and File. One byte store, one part collector and one type rule are
// shared; the classes stay separate so the brand and toStringTag differ.
enum BlobField { kBlobSizeField = 0, kBlobTypeField };
enum FileField { kFileNameField = 0, kFileLastModifiedField, kFileRelativePathField };
enum BlobMethodKind { kBlobText = 0, kBlobArrayBuffer, kBlobSlice, kBlobBytes, kBlobStream };
bool IsBlob(JS::HandleValue value);
bool NormalizeType(JSContext* cx, JS::HandleValue value, std::string* type);
// Reads the `type` member of a BlobPropertyBag; a missing bag or member
// leaves the type empty.
bool NormalizeTypeOption(JSContext* cx, JS::HandleValue options, std::string* type);
// Collects the parts' bytes; `strings` (when given) receives the byte range
// of each string part, the only parts `endings: "native"` converts.
bool CollectBlobParts(JSContext* cx, JS::HandleValue parts, std::string* bytes,
                      std::vector<std::pair<size_t, size_t>>* strings = nullptr);
// BlobPropertyBag.endings, read before `type` (WebIDL members are sorted):
// "native" converts the string parts' line endings to the platform's, CRLF
// for the Windows Firefox this engine is (BlobSet::AppendString). An unknown
// value throws Firefox's TypeError; `bytes` is rewritten in place.
bool ApplyBlobEndings(JSContext* cx, JS::HandleValue options, std::string* bytes,
                      const std::vector<std::pair<size_t, size_t>>& strings, bool crlf);
// Whether the platform this realm presents writes CR LF line breaks (Windows,
// the default profile) or LF; read through the Browser Environment port.
bool NativeCrlf(const State& state);
bool BlobBytes(JSContext* cx, JS::HandleObject self, const uint8_t** data,
               size_t* length);
JSObject* MakeBlob(State& state, const std::string& bytes, const std::string& type,
                   const JSClass* clasp, JS::HandleObject prototype);
bool BlobConstructor(JSContext*, unsigned, JS::Value*);
bool BlobGetter(JSContext*, unsigned, JS::Value*);
bool BlobMethod(JSContext*, unsigned, JS::Value*);
bool FileConstructor(JSContext*, unsigned, JS::Value*);
bool FileGetter(JSContext*, unsigned, JS::Value*);
bool InstallBlob(State& state, PageHelpers& helpers);
bool InstallFile(State& state, PageHelpers& helpers);

// FileReader. The reader shares the Blob byte store and the interaction event
// tree; the read methods own the state transitions and one job of the engine job
// queue delivers the result, so no handler ever runs synchronously.
enum ReaderMethodKind {
  kReaderText = 0, kReaderArrayBuffer, kReaderDataURL, kReaderBinaryString, kReaderAbort
};
enum ReaderField { kReaderReadyStateField = 0, kReaderResultField, kReaderErrorField };
enum ReaderHandlerKind {
  kReaderOnLoadStart = 0, kReaderOnProgress, kReaderOnLoad, kReaderOnAbort, kReaderOnError,
  kReaderOnLoadEnd
};
bool ReaderBrand(JSContext* cx, JS::HandleValue self, JS::MutableHandleObject out);
// Builds a DOMException from the page interface; the one error factory the
// reader uses for InvalidStateError/AbortError/EncodingError.
bool MakeException(State& state, const char* name, const char* message,
                   JS::MutableHandleValue result);
bool QueueReaderJob(State& state, JS::HandleObject reader, int32_t generation,
                    bool failed, JS::HandleValue payload);
bool FileReaderConstructor(JSContext*, unsigned, JS::Value*);
bool FileReaderMethod(JSContext*, unsigned, JS::Value*);
bool FileReaderGetter(JSContext*, unsigned, JS::Value*);
bool ReaderHandlerGet(JSContext*, unsigned, JS::Value*);
bool ReaderHandlerSet(JSContext*, unsigned, JS::Value*);
bool InstallFileReader(State& state, PageHelpers& helpers);
bool InstallFileList(State& state);  // file_list.cpp
bool InstallDirectory(State& state);  // directory.cpp
// One read's result value, computed from the Blob's bounded bytes; shared by the
// FileReader job and FileReaderSync. *failed marks an EncodingError value.
bool ReadBlobPayload(State& state, ReaderMethodKind kind, JS::HandleValue blob, JS::HandleValue label,
                     JS::MutableHandleValue out, bool* failed);
// FileReaderSync ([Exposed=(DedicatedWorker,SharedWorker)]): the same reads,
// returned synchronously (reader_sync.cpp).
bool InstallFileReaderSync(State& state, PageHelpers& helpers);

// FormData. One ordered array of [name, value] pairs where a value is a string or
// a File; a Blob passed to append/set is materialized as a File so the entry stays
// self-describing.
enum FormDataMethodKind {
  kFormDataAppend = 0, kFormDataDelete, kFormDataGet, kFormDataGetAll, kFormDataHas,
  kFormDataSet, kFormDataForEach
};
enum FormDataIteratorKind { kFormDataEntries = 0, kFormDataKeys, kFormDataValues };
bool FormDataBrand(JSContext* cx, JS::HandleValue self, JS::MutableHandleObject out);
bool FormDataList(JSContext* cx, JS::HandleObject self, JS::MutableHandleObject list);
// The one entry-value conversion: a Blob becomes a File (named from the
// optional filename or the Blob's own name), everything else is stringified.
bool FormDataValue(State& state, JS::HandleValue value, JS::HandleValue filename,
                   JS::MutableHandleValue out);
bool FormDataConstructor(JSContext*, unsigned, JS::Value*);
bool FormDataMethod(JSContext*, unsigned, JS::Value*);
bool FormDataIterator(JSContext*, unsigned, JS::Value*);
bool FormDataNext(JSContext*, unsigned, JS::Value*);
bool FormDataIdentity(JSContext*, unsigned, JS::Value*);
bool InstallFormData(State& state, PageHelpers& helpers);
bool PopulateFormData(State&, JS::HandleObject, const std::vector<FormField>&);
// The one Blob-to-File materialization: FormData and File share it, so a copied
// Blob becomes a File with the name the caller gave and no second byte store.
JSObject* MakeFile(State& state, const std::string& bytes, const std::string& type,
                   const std::string& name, JS::HandleObject prototype);

// The application/x-www-form-urlencoded codec shared with FormData.
std::string FormEncode(std::string_view text);
std::string SerializeFormQuery(
    const std::vector<std::pair<std::string, std::string>>& entries);
bool Convert(JSContext* cx, JS::HandleValue value, std::string* text);
bool Value(JSContext* cx, std::string_view text, JS::MutableHandleValue result);
bool ParamsEntries(JSContext* cx, JS::HandleObject params,
                   std::vector<std::pair<std::string, std::string>>* entries);
bool StoreEntries(State& state, JS::HandleObject params,
                  const std::vector<std::pair<std::string, std::string>>& entries);
}  // namespace ruyijs::web_platform
#endif

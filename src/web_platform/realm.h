#ifndef RUYIJS_WEB_PLATFORM_REALM_H
#define RUYIJS_WEB_PLATFORM_REALM_H
#include <memory>
#include <string>
#include "web_platform/form_submission.h"
#include "web_platform/object_url_session.h"
#include "js/RootingAPI.h"
struct JSContext;
namespace ruyijs { class PageHelpers; }
namespace ruyijs::interaction { class Realm; }
namespace ruyijs::web_platform {
struct State;
// Brand check other owners use for a Blob or File value of any realm.
bool IsBlobValue(JS::HandleValue value);
// URL and URLSearchParams are realm-local interfaces: the installer creates them
// on the target realm's global and keeps every prototype, parameter list and
// cached href in that realm, so an iframe never aliases the parent's objects.
class Realm final {
 public:
  Realm(JSContext* cx, JS::HandleObject page, PageHelpers& helpers,
        interaction::Realm& events, Realm* source = nullptr);
  ~Realm();
  Realm(const Realm&) = delete;
  Realm& operator=(const Realm&) = delete;
  bool Install();
  bool InstallEncoding();
  bool InstallWorker(bool readerSync = true);  // FileReaderSync: dedicated Workers only
  void SetObjectUrlExchange(ObjectUrlExchange, void* context);
  std::shared_ptr<ObjectUrlSession> OpenObjectUrlSession();
  void SetFormFieldsPort(FormFieldsPort, void* context);
  // Blob/File endings: "native": true when the presented platform writes CR LF.
  void SetLineBreakPort(bool (*crlf)(void*), void* context);
  void SetByteStreamPort(bool (*)(void*, std::string_view, JS::MutableHandleObject), void* context);
  void SetObjectUrlPorts(bool (*origin)(void*, std::string*), void* context,
                         bool (*mediaBrand)(JSObject*));
  bool ReadObjectUrl(const std::string& url, std::string* bytes, std::string* type);
  void ClearDocumentUrls();
  bool CreateFormData(const std::vector<FormField>&, JS::MutableHandleObject);
  // A FileList of this realm over the File objects of `files` (an array; null: empty).
  bool NewFileList(JS::HandleObject files, JS::MutableHandleObject result);
  bool EncodeFormData(JS::HandleObject, const std::string& encoding,
                      std::string* bytes, std::string* contentType);
 private:
  bool InstallCore();
  std::unique_ptr<State> state_;
};
}  // namespace ruyijs::web_platform
#endif

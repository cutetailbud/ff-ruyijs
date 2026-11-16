#ifndef RUYIJS_WEB_STYLING_REALM_H
#define RUYIJS_WEB_STYLING_REALM_H
#include <memory>
#include "document_model/style_port.h"
#include "document_model/style_source.h"
#include "web_styling/load_ports.h"
#include "web_styling/used_box.h"
#include "js/RootingAPI.h"
struct JSContext;
namespace ruyijs { class PageHelpers; }
namespace ruyijs::browser_environment { class Environment; }
namespace ruyijs::web_styling {
struct State;
struct ComputedStyle;
class ComputedStyleReader;
// Styling / CSSOM owner: getComputedStyle, document.styleSheets, CSSStyleSheet,
// CSSRule, the CSS namespace and matchMedia/MediaQueryList. The Document Model
// keeps owning the DOM and the CSSStyleDeclaration/CSSStyleProperties
// interfaces; this owner stores the computed and rule declaration views, which
// take that one prototype and answer its members through the style port, and
// reads the cascade input through ports, so each constructor and prototype has
// exactly one owner.
// The style declaration port (document_model/style_port.h) over this owner's
// computed and rule declaration views.
bool OwnsDeclaration(JSObject* object);
bool CallDeclaration(void* context, JSContext* cx, document_model::StyleMember member, JS::HandleObject self,
                     const JS::CallArgs& args, const char* property, const char* accessor);
// Reads the configure-only "mediaFeatures" group into the styling media answers.
bool ConfigureProfileGroup(void* context, JS::HandleObject group);
class Realm final {
 public:
  Realm(JSContext* cx, JS::HandleObject page, PageHelpers& helpers,
        browser_environment::Environment& environment, JS::HandleObject document,
        JS::HandleObject shadowPrototype);
  ~Realm();
  Realm(const Realm&) = delete;
  Realm& operator=(const Realm&) = delete;
  bool Install();
  document_model::StyleDeclarationPort DeclarationPort();
  bool ReadComputed(JS::HandleObject, ComputedStyle*);
  std::unique_ptr<ComputedStyleReader> BeginComputedRead();
  // A fingerprint of `element`'s cascade input (sheet order, flags, media and
  // rule text); equal fingerprints give equal computed styles for an
  // unchanged tree and environment. Web Geometry's layout cache keys on it.
  bool CascadeStamp(JS::HandleObject element, uint64_t* stamp);
  // Ports bound by the runtime coordinator after the Document Model is
  // installed: the cascade input, the <style> sources and the DOMException
  // factory. Cleared before either owner is destroyed.
  void SetDocumentPorts(document_model::StyleChainPort chain,
                        document_model::StyleSheetsPort sheets, void* context,
                        document_model::StyleRelationPort relations=nullptr);
  void SetDocumentNodePort(document_model::StyleNodePort node, void* context);
  void SetDocumentSourcePorts(document_model::StyleSourcePort,
                              document_model::StyleAssociationPort, void*);
  void SetDocumentListPorts(document_model::StyleDocumentResolvePort,
                            document_model::StyleDocumentSourcePort, void*);
  bool NewDocumentList(JS::HandleObject, JS::MutableHandleValue);
  void SetAdoptedPorts(document_model::StyleDocumentResolvePort,
                       document_model::StyleScopeDocumentPort,
                       document_model::StyleCascadeScopePort, void*);
  bool NewAdoptedList(JS::HandleObject, JS::MutableHandleValue);
  bool NotifyStyle(JS::HandleObject, document_model::StyleNotifyAction);
  void SetRangeBrandPort(bool (*brand)(JS::HandleValue));
  void SetLoadPort(StyleLoadPort, void*);
  void SetLoadStartPort(StyleLoadStartPort, void*);
  // External sheets whose response has not arrived yet / that are still loading
  // (the document's load event waits for them, as in Firefox).
  bool LoadsInFlight() const;
  bool LoadsPending() const;
  // Applies every sheet whose response arrived (its load task runs now).
  bool CompleteArrived();
  void SetLoadEventPort(StyleLoadEventPort, void*);
  bool StyleElement(uint64_t document, uint64_t key, document_model::StyleElementAction,
                     bool disabled, JS::MutableHandleValue);
  void SetExceptionPort(bool (*port)(void*, const char*, const char*), void* context);
  // The geometry owner's used boxes for getComputedStyle resolved values.
  void SetUsedBoxPort(bool (*port)(void*, JS::HandleObject, UsedBox*, bool*), void* context);
  void SetTaskPort(bool (*port)(void*, JS::HandleObject), void* context);
 private:
  std::unique_ptr<State> state_;
};
}  // namespace ruyijs::web_styling
#endif

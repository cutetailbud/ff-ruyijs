// Style-source contract between the Document Model (the DOM owner) and the
// Styling / CSSOM owner (T17). The DOM stays the single source of truth: the
// styling owner asks for one element's cascade input and the document's ordered
// <style> sources through these ports and never walks a second tree index.
#ifndef RUYIJS_DOCUMENT_MODEL_STYLE_SOURCE_H
#define RUYIJS_DOCUMENT_MODEL_STYLE_SOURCE_H
#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>
#include "js/RootingAPI.h"

namespace ruyijs::document_model {

// One element frame of a cascade input. The element itself is first, its
// ancestors follow from the parent outward. Strings are UTF-8; `tag` is the
// element's local name.
struct StyleFrame {
  std::string tag;
  std::string id;
  std::vector<std::string> classes;
  std::vector<std::pair<std::string, std::string>> attributes;
  std::string inline_style;
};
struct StyleIdentity {uint64_t document=0,key=0;};
struct StyleRelationFrame {
  StyleFrame value;
  StyleIdentity identity,parent,previous,next,first;
  bool root=false,empty=true,html=true,focused=false;
};
using StyleRelationPort=bool (*)(void*,JS::HandleObject,uint64_t,uint64_t,StyleRelationFrame*);

// One author stylesheet source in document order. `key` is the element's stable
// node identity, so the styling owner can keep one CSSStyleSheet per element
// across tree edits instead of keying by position.
struct StyleSheetSource {
  uint64_t document = 0;
  uint64_t key = 0;
  uint64_t scopeDocument = 0, scopeNode = 0;
  std::string href, documentURL, origin, resourceOrigin;
  std::string text;
  std::shared_ptr<const std::string> sharedText;
  const std::string& Text() const noexcept { return sharedText ? *sharedText : text; }
  std::string title, media, titleAttribute, typeAttribute;
  std::string rel, crossOrigin;
  bool external = false, presented = false, hasCrossOrigin = false;
  bool disabled = false;
  bool mainDocument = true;
};

// Reads the cascade input for one JS value. Returns false only on an internal
// error; `is_element` is false when the value is not an Element, which the
// styling owner turns into the DOM's TypeError.
// A null `chain` checks the receiver only, without materializing ancestors.
using StyleChainPort = bool (*)(void* context, JS::HandleObject element,
                                std::vector<StyleFrame>* chain, bool* is_element);
// The document's ordered <style> element sources, rebuilt on every call so the
// live document.styleSheets always reflects the current tree.
using StyleSheetsPort = bool (*)(void* context, std::vector<StyleSheetSource>* out);
// Returns the unique original wrapper for a source node; never reads public JS.
using StyleNodePort = bool (*)(void* context, uint64_t document, uint64_t key,
                               JS::MutableHandleValue out);
using StyleSourcePort = bool (*)(void*, uint64_t, uint64_t, std::vector<StyleSheetSource>*);
using StyleAssociationPort = bool (*)(void*, JS::HandleObject, JS::HandleValue, JS::HandleValue);
struct StyleSheetScope { uint64_t document = 0, key = 0; };
using StyleDocumentResolvePort = bool (*)(void*, JS::HandleValue, JS::MutableHandleValue, bool shadow);
using StyleDocumentSourcePort = bool (*)(void*, JS::HandleObject, StyleSheetScope*,
                                         std::vector<StyleSheetSource>*);
using StyleDocumentListPort = bool (*)(void*, JS::HandleObject, JS::MutableHandleValue);
using StyleScopeDocumentPort = bool (*)(void*, JS::HandleObject, JS::MutableHandleObject);
using StyleCascadeScopePort = bool (*)(void*, JS::HandleObject, JS::MutableHandleObject, bool*);
// Forget: the element is not (or no longer) a stylesheet link; the Styling
// owner only retires a sheet it may still hold for it, without reading the
// document's other style sources.
enum class StyleElementAction { Sheet, ReadDisabled, WriteDisabled, Forget };
using StyleElementPort = bool (*)(void*, uint64_t, uint64_t, StyleElementAction, bool,
                                  JS::MutableHandleValue);
enum class StyleNotifyAction { RetireSheet, MigrateAdopted };
using StyleNotifyPort = bool (*)(void*, JS::HandleObject, StyleNotifyAction);
}  // namespace ruyijs::document_model
#endif

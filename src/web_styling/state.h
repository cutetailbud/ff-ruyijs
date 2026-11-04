// Realm-local state for the Styling / CSSOM owner (T17). The owner supplies
// getComputedStyle, document.styleSheets, CSSStyleSheet/CSSRule, the CSS
// namespace and matchMedia; every value it exposes comes from the imported
// styling owner and the existing Document Model / Browser Environment ports.
#ifndef RUYIJS_WEB_STYLING_STATE_H
#define RUYIJS_WEB_STYLING_STATE_H
#include <cstdint>
#include <memory>
#include <map>
#include <string>
#include <unordered_map>
#include <vector>
#include "document_model/style_source.h"
#include "domain/styling/media_list_value.h"
#include "domain/styling/sheet_set_value.h"
#include "domain/styling/external_style_cache.h"
#include "domain/styling/css_rule_value.h"
#include "js/RootingAPI.h"
#include "runtime/web_kit.h"
#include "web_styling/load_ports.h"
namespace ruyijs::domain::styling { class StyleSheetAggregate; struct RuleIndexStore; }

struct JSContext;
namespace ruyijs {
class PageHelpers;
namespace browser_environment { class Environment; }
namespace web_styling {

// One rule of a CSSStyleSheet: the selector text and the serialized declaration
// block the rule style view exposes. Stable shared records survive list index
// changes and deletion; wrappers live in their sheet's GC-traced slots.
struct SheetCache;
using SheetRule = ruyijs::domain::styling::CssomRuleValue;

// One <style> element's sheet. The element stays the DOM's; this cache holds the
// rules the CSSOM exposes and the single CSSStyleSheet object per element key.
struct SheetCache {
  uint64_t document = 0;
  uint64_t key = 0;
  uint64_t scopeDocument = 0, scopeNode = 0;
  std::string text;  // the source text these rules were materialized from
  std::string href, baseURL;
  std::string title, sourceTitle, sourceMedia, sourceType;
  std::string sourceRel, sourceCors, documentURL, origin, resourceOrigin;
  bool external = false, ready = true, loading = false, originClean = true, hasCors = false;
  bool loadDelivered = true;
  // An asynchronous load in flight (`requested`) and its response once arrived.
  bool requested = false;
  std::shared_ptr<StyleLoadResult> arrived;
  std::shared_ptr<const ruyijs::domain::styling::ExternalStyleResource> resource;
  bool constructed = false, disabled = false, locked = false, retired = false;
  bool mainDocument = true;
  ruyijs::domain::styling::MediaListValue media;
  std::vector<std::shared_ptr<SheetRule>> rules;
  JSObject* object = nullptr;  // Non-owning identity; trace/finalize maintain it.
  explicit SheetCache(JSContext*) {}
};

struct State {
  State(JSContext* cx, JS::HandleObject page, PageHelpers& helpers,
        browser_environment::Environment& environment, JS::HandleObject document,
        JS::HandleObject shadowPrototype);
  ~State();
  JSContext* cx;
  browser_environment::Environment* environment;
  WebKit kit;
  WebFace baseSheet, sheet, rule, groupingRule, conditionRule, mediaRule, supportsRule,
      styleRule, nestedDeclarations, mozDocumentRule, keyframesRule, keyframeRule, counterStyleRule, fontPaletteValuesRule, pageRule, pageDescriptors, ruleException,
      sheetList, ruleList, mediaList, mediaQuery,
      containerRule, fontFaceRule, fontFeatureValuesRule, importRule, layerBlockRule,
      layerStatementRule, namespaceRule, positionTryRule, propertyRule, scopeRule,
      startingStyleRule, fontFaceDescriptors, positionTryDescriptors, highlight, highlightRegistry;
  // declarationPrototype: Document Model's CSSStyleProperties.prototype, the
  // one prototype of every declaration view (members answer via the port).
  JS::PersistentRootedObject page, documentObject, documentPrototype, shadowPrototype,
      declarationPrototype;
  // AbstractRange brand of the Document Model (Highlight members), bound by the
  // runtime assembly; while unbound every value fails the check.
  bool (*rangeBrand)(JS::HandleValue) = nullptr;
  document_model::StyleChainPort chainPort = nullptr;
  document_model::StyleRelationPort relationPort = nullptr;
  void* chainContext = nullptr;
  document_model::StyleSheetsPort sheetsPort = nullptr;
  void* sheetsContext = nullptr;
  document_model::StyleNodePort nodePort = nullptr;
  void* nodeContext = nullptr;
  document_model::StyleSourcePort sourcePort = nullptr;
  document_model::StyleAssociationPort associationPort = nullptr;
  void* sourceContext = nullptr;
  document_model::StyleDocumentResolvePort documentResolve = nullptr;
  document_model::StyleDocumentSourcePort documentSources = nullptr;
  void* documentContext = nullptr;
  document_model::StyleDocumentResolvePort adoptedResolve = nullptr;
  document_model::StyleScopeDocumentPort scopeDocument = nullptr;
  document_model::StyleCascadeScopePort cascadeScope = nullptr;
  void* adoptedContext = nullptr;
  bool (*exceptionPort)(void*, const char*, const char*) = nullptr;
  void* exceptionContext = nullptr;
  bool (*postTask)(void*, JS::HandleObject) = nullptr;
  void* taskContext = nullptr;
  StyleLoadPort loadPort = nullptr;
  void* loadContext = nullptr;
  StyleLoadStartPort startPort = nullptr;
  void* startContext = nullptr;
  StyleLoadEventPort loadEvent = nullptr;
  void* loadEventContext = nullptr;
  std::vector<std::shared_ptr<SheetCache>> sheets;
  ruyijs::domain::styling::ExternalStyleCache externalResources{64,4*1024*1024};
  std::map<std::pair<uint64_t,uint64_t>, ruyijs::domain::styling::SheetSetValue> sheetSets;
  // Parsed cascade inputs by content fingerprint (cascade_sources.cpp): a style
  // read after an inline-style or DOM change reuses the sheets' parsed rules.
  struct CascadeInput {
    uint64_t hash = 0;
    std::shared_ptr<const std::vector<ruyijs::domain::styling::StyleSheetAggregate>> aggregates;
    std::shared_ptr<const struct SelectorDependencies> dependencies;
    // The rule indexes and parsed selectors over these aggregates, shared by
    // every reader of this input (they depend only on the immutable sheets).
    std::shared_ptr<ruyijs::domain::styling::RuleIndexStore> indexes;
  };
  std::vector<CascadeInput> cascadeInputs;
  // getComputedStyle results for one document revision and one cascade stamp.
  // A style, class, attribute or tree edit bumps the document revision, and a
  // stylesheet edit changes the stamp, so the next read computes again.
  struct ComputedBlockCache {
    uint64_t revision = 0;
    uint64_t stamp = 0;
    bool ready = false;
    std::unordered_map<uint64_t, std::string> blocks;
  };
  ComputedBlockCache computedBlocks;
  std::shared_ptr<SheetCache> Materialize(const document_model::StyleSheetSource& source);
  bool RetireSheets(const std::vector<document_model::StyleSheetSource>& sources,
                     const document_model::StyleSheetScope* scope = nullptr);
  JSObject* NewStyleView(const std::string& text, JS::MutableHandleObject out,
                         JS::HandleObject rule = nullptr);
  bool Throw(const char* name, const char* message) const;
};

extern const JSClass kStyleViewClass, kSheetClass, kRuleClass, kStyleRuleClass,
    kGroupingRuleClass, kConditionRuleClass, kMediaRuleClass, kSupportsRuleClass, kNestedDeclarationsClass,
    kKeyframesRuleClass, kKeyframeRuleClass, kPageRuleClass, kSheetListClass, kMediaQueryClass,
    kLayerBlockRuleClass, kLayerStatementRuleClass, kPropertyRuleClass;
}  // namespace web_styling
}  // namespace ruyijs
#endif

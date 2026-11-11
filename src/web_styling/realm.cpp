#include "web_styling/realm.h"

#include "browser_environment/environment.h"
#include "js/Realm.h"
#include "web_styling/computed_source.h"
#include "web_styling/internal.h"
#include "web_styling/page_descriptors.h"

namespace ruyijs::web_styling {
State::State(JSContext* context, JS::HandleObject global, PageHelpers& helpers,
             browser_environment::Environment& owner, JS::HandleObject document,
             JS::HandleObject shadow)
    : cx(context), environment(&owner), kit(context, global, helpers, this),
      baseSheet(context), sheet(context), rule(context), groupingRule(context), conditionRule(context),
      mediaRule(context), supportsRule(context), styleRule(context), nestedDeclarations(context),
      mozDocumentRule(context), keyframesRule(context), keyframeRule(context), counterStyleRule(context),
      fontPaletteValuesRule(context), pageRule(context),
      pageDescriptors(context), ruleException(context), sheetList(context),
      ruleList(context), mediaList(context), mediaQuery(context),
      containerRule(context), fontFaceRule(context), fontFeatureValuesRule(context), importRule(context),
      layerBlockRule(context), layerStatementRule(context), namespaceRule(context), positionTryRule(context),
      propertyRule(context), scopeRule(context), startingStyleRule(context), fontFaceDescriptors(context),
      positionTryDescriptors(context), highlight(context), highlightRegistry(context), page(context, global), documentObject(context, document),
      documentPrototype(context), shadowPrototype(context, shadow), declarationPrototype(context) {
  const JSAutoRealm realm(cx, global);
  // Install the Document member at its Firefox IDL path. Installing on the
  // immediate HTMLDocument prototype would shadow user Document supplements.
  JS::RootedValue constructor(cx);
  if (JS_GetProperty(cx, global, "Document", &constructor) && constructor.isObject()) {
    JS::RootedObject holder(cx, &constructor.toObject());
    JS::RootedValue prototype(cx);
    if (JS_GetProperty(cx, holder, "prototype", &prototype) && prototype.isObject())
      documentPrototype = &prototype.toObject();
  }
  // The Document Model owns CSSStyleDeclaration and CSSStyleProperties. A
  // computed or rule view takes that one CSSStyleProperties.prototype, as in
  // Firefox; its members reach this owner through the style declaration port.
  if (JS_GetProperty(cx, global, "CSSStyleProperties", &constructor) &&
      constructor.isObject()) {
    JS::RootedObject holder(cx, &constructor.toObject());
    JS::RootedValue prototype(cx);
    if (JS_GetProperty(cx, holder, "prototype", &prototype) && prototype.isObject())
      declarationPrototype = &prototype.toObject();
  }
}
State::~State() {
  computedBlocks.reader.reset();
  // Native shared holders may outlive this owner until the closing GC.
  for (const auto& cache : sheets) cache->object = nullptr;
}

JSObject* State::NewStyleView(const std::string& text, JS::MutableHandleObject out, JS::HandleObject rule) {
  JS::RootedString block(cx, JS_NewStringCopyUTF8N(cx, JS::UTF8Chars(text.data(), text.size())));
  if (!block) return nullptr;
  const bool pageView=rule&&RuleDataOf(rule).kind==6;
  JS::RootedObject proto(cx,pageView?nullptr:declarationPrototype.get());
  JS::RootedObject object(cx, JS_NewObjectWithGivenProto(cx, &kStyleViewClass, proto));
  if (!object) return nullptr;
  JS::SetReservedSlot(object, 0, JS::StringValue(block));
  JS::SetReservedSlot(object, 1, JS::PrivateValue(this));
  JS::SetReservedSlot(object, 2, rule ? JS::ObjectValue(*rule) : JS::NullValue());
  if(pageView) {
    JS::RootedValue storage(cx,JS::ObjectValue(*object));js::ProxyOptions options;
    options.setClass(&kPageDescriptorsClass);
    object=js::NewProxyObject(cx,PageDescriptorsHandler(),storage,pageDescriptors.prototype,options);
    if(!object) return nullptr;
  }
  out.set(object);
  return object;
}

bool State::Throw(const char* name, const char* message) const {
  if (exceptionPort) return exceptionPort(exceptionContext, name, message);
  return WebKit::Fail(cx, message);
}

Realm::Realm(JSContext* cx, JS::HandleObject page, PageHelpers& helpers,
             browser_environment::Environment& environment, JS::HandleObject document,
             JS::HandleObject shadow)
    : state_(std::make_unique<State>(cx, page, helpers, environment, document, shadow)) {}
Realm::~Realm() = default;

document_model::StyleDeclarationPort Realm::DeclarationPort() {
  return {OwnsDeclaration,CallDeclaration,state_.get()};
}

void Realm::SetDocumentPorts(document_model::StyleChainPort chain,
                             document_model::StyleSheetsPort sheets, void* context,
                             document_model::StyleRelationPort relations) {
  state_->chainPort = chain;
  state_->relationPort = relations;
  state_->sheetsPort = sheets;
  state_->chainContext = context;
  state_->sheetsContext = context;
}
void Realm::SetExceptionPort(bool (*port)(void*, const char*, const char*), void* context) {
  state_->exceptionPort = port;
  state_->exceptionContext = context;
}
void Realm::SetDocumentNodePort(document_model::StyleNodePort node, void* context) {
  state_->nodePort = node;
  state_->nodeContext = context;
}
void Realm::SetDocumentSourcePorts(document_model::StyleSourcePort source,
                                   document_model::StyleAssociationPort association, void* context) {
  state_->sourcePort = source;state_->associationPort = association;state_->sourceContext = context;
}
void Realm::SetTaskPort(bool (*port)(void*, JS::HandleObject), void* context) {
  state_->postTask = port;
  state_->taskContext = context;
}
void Realm::SetDocumentListPorts(document_model::StyleDocumentResolvePort resolve,
                                 document_model::StyleDocumentSourcePort sources, void* context) {
  state_->documentResolve = resolve;state_->documentSources = sources;state_->documentContext = context;
}

void Realm::SetRangeBrandPort(bool (*brand)(JS::HandleValue)) { state_->rangeBrand = brand; }

bool Realm::Install() {
  State& s = *state_;
  if (!s.documentPrototype || !s.shadowPrototype || !s.declarationPrototype) return false;
  const JSAutoRealm realm(s.cx, s.kit.page);
  return InstallComputed(s) && InstallSheets(s) && InstallCSS(s) && InstallMedia(s);
}
}  // namespace ruyijs::web_styling

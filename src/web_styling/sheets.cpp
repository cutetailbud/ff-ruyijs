// Mutations use the original CSS grammar and stable owner records.
#include "domain/styling/constructed_text.h"
#include "domain/styling/css_components.h"
#include "web_styling/cssom.h"
#include "js/Array.h"
#include "js/Realm.h"

namespace ruyijs::web_styling {
bool SheetRules(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  JS::RootedObject self(cx);
  if (!CssReceiver(cx, args.thisv(), &kSheetClass, &self)) return false;
  if (!SheetDataOf(self).originClean)
    return SheetOwnerOf(self).Throw("SecurityError", "Stylesheet rules are not origin-clean");
  args.rval().set(JS::GetReservedSlot(self, SheetList));
  return JS_WrapValue(cx, args.rval());
}
bool InsertCssRule(JSContext* cx, JS::HandleObject self, const std::string& text, uint32_t index) {
  State& state = ContainerOwnerOf(self);
  JSAutoRealm realm(cx, state.page);
  const bool group=IsRuleObject(self);
  if(group) {
    auto parent=JS::GetReservedSlot(self,RuleParent);
    if(parent.isObject()) {
      JS::RootedObject sheet(cx,&parent.toObject());
      if(SheetDataOf(sheet).locked) return state.Throw("NotAllowedError","Sheet replacement is pending");
    }
  }
  if (!group) {
    const auto& cache=SheetDataOf(self);
    if (!cache.originClean) return state.Throw("SecurityError", "Stylesheet rules are not origin-clean");
    if (cache.locked) return state.Throw("InvalidAccessError", "Sheet replacement is pending");
  }
  auto& rows=ContainerRows(self);
  if (index > rows.size()) return state.Throw("IndexSizeError", "rule index out of range");
  const auto clean=ruyijs::domain::styling::CssLower(ruyijs::domain::styling::CssClean(text));
  if (group && (clean.substr(0,7)=="@import" || clean.substr(0,10)=="@namespace"))
    return state.Throw("HierarchyRequestError", "Statement rule is not allowed in a group");
  auto parsed=ruyijs::domain::styling::ParseCssRuleValues(text,true);
  if (!parsed.valid || parsed.rules.size()!=1)
    return state.Throw("SyntaxError", "insertRule requires exactly one rule");
  if(group&&RuleDataOf(self).kind==6&&parsed.rules[0]->kind==6)
    return state.Throw("HierarchyRequestError","A page rule cannot contain another page rule");
  if (!ShiftRuleWrappers(cx, self, index, true)) return false;
  rows.insert(rows.begin() + index, std::move(parsed.rules[0]));
  ++state.cssomGeneration;
  return true;
}
bool SheetInsert(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  JS::RootedObject self(cx);
  if (!CssReceiver(cx, args.thisv(), &kSheetClass, &self)) return false;
  if (!argc) return WebKit::Fail(cx, "insertRule requires a rule");
  std::string text;uint32_t index = 0;
  if (!CssString(cx, args[0], &text) ||
      (argc > 1 && !JS::ToUint32(cx, args[1], &index)) ||
      !InsertCssRule(cx, self, text, index)) return false;
  args.rval().setNumber(index);return true;
}
bool DeleteCssRule(JSContext* cx, JS::HandleObject self, uint32_t index) {
  State& state = ContainerOwnerOf(self);
  JSAutoRealm realm(cx, state.page);
  if (!IsRuleObject(self)) {
    const auto& cache=SheetDataOf(self);
    if (!cache.originClean) return state.Throw("SecurityError", "Stylesheet rules are not origin-clean");
    if (cache.locked) return state.Throw("InvalidAccessError", "Sheet replacement is pending");
  }
  auto& rows=ContainerRows(self);
  if (index >= rows.size()) return state.Throw("IndexSizeError", "rule index out of range");
  if (!ShiftRuleWrappers(cx, self, index, false)) return false;
  rows.erase(rows.begin() + index);
  ++state.cssomGeneration;
  return true;
}
bool SheetDelete(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  JS::RootedObject self(cx);
  if (!CssReceiver(cx, args.thisv(), &kSheetClass, &self)) return false;
  if (!argc) return WebKit::Fail(cx, "deleteRule requires an index");
  uint32_t index;
  if (!JS::ToUint32(cx, args[0], &index) || !DeleteCssRule(cx, self, index)) return false;
  args.rval().setUndefined();return true;
}
void ParseSheet(SheetCache& cache, const document_model::StyleSheetSource& source) {
  auto parsed=ruyijs::domain::styling::ParseCssRuleValues(source.Text());
  cache.rules=std::move(parsed.rules);
  cache.digestGeneration=0;
}
bool ReplaceRuleRows(JSContext* cx, JS::HandleObject self, const std::string& text) {
  auto& cache = SheetDataOf(self);
  SheetCache parsed(cx);
  document_model::StyleSheetSource source;
  source.text = ruyijs::domain::styling::WithoutImports(text);
  ParseSheet(parsed, source);
  JS::RootedObject wrappers(cx, JS::NewArrayObject(cx, parsed.rules.size()));
  if (!wrappers || !JS_SetPrototype(cx, wrappers, nullptr)) return false;
  JS::RootedObject old(cx, &JS::GetReservedSlot(self, SheetWrappers).toObject());
  JS::RootedValue value(cx);
  for (uint32_t i = 0; i < cache.rules.size(); ++i) {
    if (!JS_GetElement(cx, old, i, &value)) return false;
    if (value.isObject()) { JS::RootedObject rule(cx,&value.toObject());if(!DetachRule(rule)) return false; }
  }
  cache.rules.swap(parsed.rules);
  cache.digestGeneration = 0;
  ++SheetOwnerOf(self).cssomGeneration;
  JS::SetReservedSlot(self, SheetWrappers, JS::ObjectValue(*wrappers));
  return true;
}
}  // namespace ruyijs::web_styling

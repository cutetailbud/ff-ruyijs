#include "web_styling/page_descriptors.h"
#include "domain/styling/css_page_value.h"
namespace ruyijs::web_styling {
namespace {
bool PageSelectorGet(JSContext* cx,unsigned argc,JS::Value* vp) {
  auto args=JS::CallArgsFromVp(argc,vp);JS::RootedObject self(cx);
  if(!CssReceiver(cx,args.thisv(),&kPageRuleClass,&self)) return false;
  const auto& text=RuleDataOf(self).selector;
  JS::RootedString value(cx,JS_NewStringCopyUTF8N(cx,JS::UTF8Chars(text.data(),text.size())));
  if(!value) return false;args.rval().setString(value);return true;
}
bool PageSelectorSet(JSContext* cx,unsigned argc,JS::Value* vp) {
  auto args=JS::CallArgsFromVp(argc,vp);JS::RootedObject self(cx);std::string text,parsed;
  if(!CssReceiver(cx,args.thisv(),&kPageRuleClass,&self)||!CssString(cx,args.get(0),&text)) return false;
  if(RuleSlotValue(self,RuleParent).isObject()&&ruyijs::domain::styling::CssPageSelector(text,&parsed)) {
    RuleDataOf(self).selector=std::move(parsed);
    ++RuleOwnerOf(self).cssomGeneration;
  }
  args.rval().setUndefined();return true;
}
}
bool InstallPageRules(State& state) {
  auto& kit=state.kit;
  if(!InstallPageDescriptors(state)||
      !kit.Define("CSSPageRule",&kPageRuleClass,state.groupingRule.prototype,WebKit::Reject,&state.pageRule)||
      !JS_SetPrototype(kit.cx,state.pageRule.constructor,state.groupingRule.constructor)) return false;
  return kit.Accessor(state.pageRule,"CSSPageRule","selectorText",PageSelectorGet,PageSelectorSet,0)&&
    kit.Accessor(state.pageRule,"CSSPageRule","style",RuleStyleGet,RuleStyleSet,3)&&
    OrderConstructor(kit,state.pageRule);
}
}

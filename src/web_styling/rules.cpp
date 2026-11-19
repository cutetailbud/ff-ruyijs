// CSS rule members use the receiver's canonical domain value and native factory.
#include "web_styling/cssom.h"
#include "web_styling/page_descriptors.h"
#include "web_styling/rule_exception.h"
#include "web_styling/rule_interface_members.h"
#include "domain/styling/css_components.h"
#include "domain/styling/css_declarations.h"
#include "domain/styling/css_selector.h"
#include "domain/styling/css_keyframes.h"
#include "js/Realm.h"
namespace ruyijs::web_styling {
namespace {
bool TypeGet(JSContext* cx,unsigned argc,JS::Value* vp) {
  auto args=JS::CallArgsFromVp(argc,vp);JS::RootedObject self(cx);
  if(!RuleReceiver(cx,args.thisv(),&self)) return false;
  // CSSRule.type is 0 for rule kinds that have no legacy type constant.
  const int kind=RuleDataOf(self).kind;
  args.rval().setInt32(kind>=ruyijs::domain::styling::kCssLayerBlock?0:kind);return true;
}
bool TextGet(JSContext* cx,unsigned argc,JS::Value* vp) {
  auto args=JS::CallArgsFromVp(argc,vp);int member=WebKit::Slot(args);
  JS::RootedObject self(cx);
  if(!RuleReceiver(cx,args.thisv(),&self,member==1)) return false;
  const auto& rule=RuleDataOf(self);auto text=member?rule.selector:rule.Text();
  JS::RootedString out(cx,JS_NewStringCopyUTF8N(cx,JS::UTF8Chars(text.data(),text.size())));
  if(!out) return false;
  args.rval().setString(out);return true;
}
bool TextSet(JSContext* cx,unsigned argc,JS::Value* vp) {
  auto args=JS::CallArgsFromVp(argc,vp);int member=WebKit::Slot(args);
  JS::RootedObject self(cx);std::string text;
  if(!RuleReceiver(cx,args.thisv(),&self,member==1)||!CssString(cx,args.get(0),&text)) return false;
  if(member&&ruyijs::domain::styling::CssSelectorSyntax(text,RuleDataOf(self).nested)) {
    RuleDataOf(self).selector=RuleDataOf(self).nested?ruyijs::domain::styling::CssNestedSelectorText(text):
      ruyijs::domain::styling::CssSelectorText(text);
    ++RuleOwnerOf(self).cssomGeneration;
  }
  args.rval().setUndefined();return true;
}
bool ParentGet(JSContext* cx,unsigned argc,JS::Value* vp) {
  auto args=JS::CallArgsFromVp(argc,vp);JS::RootedObject self(cx);
  if(!RuleReceiver(cx,args.thisv(),&self)) return false;
  args.rval().set(RuleSlotValue(self,WebKit::Slot(args)?RuleAncestor:RuleParent));
  return JS_WrapValue(cx,args.rval());
}
}
bool RuleStyleGet(JSContext* cx,unsigned argc,JS::Value* vp) {
  auto args=JS::CallArgsFromVp(argc,vp);JS::RootedObject self(cx);
  if(!CssReceiver(cx,args.thisv(),WebKit::Slot(args)==3?&kPageRuleClass:WebKit::Slot(args)==2?&kKeyframeRuleClass:WebKit::Slot(args)==1?&kNestedDeclarationsClass:&kStyleRuleClass,&self)) return false;
  State& state=RuleOwnerOf(self);JS::RootedValue result(cx);
  {
    JSAutoRealm realm(cx,state.page);result.set(JS::GetReservedSlot(self,RuleStyle));
    if(!result.isObject()) {
      JS::RootedObject view(cx);
      if(!state.NewStyleView("",&view,self)) return false;
      result.setObject(*view);JS::SetReservedSlot(self,RuleStyle,result);
    }
  }
  if(!JS_WrapValue(cx,&result)) return false;
  args.rval().set(result);return true;
}
bool RuleStyleSet(JSContext* cx,unsigned argc,JS::Value* vp) {
  auto args=JS::CallArgsFromVp(argc,vp);JS::RootedObject self(cx);
  if(!CssReceiver(cx,args.thisv(),WebKit::Slot(args)==3?&kPageRuleClass:WebKit::Slot(args)==2?&kKeyframeRuleClass:WebKit::Slot(args)==1?&kNestedDeclarationsClass:&kStyleRuleClass,&self)) return false;
  std::string text;if(!CssString(cx,args.get(0),&text)) return false;
  if(!CheckRuleDeclaration(*detail::Self(args),self)) return false;
  State& state=RuleOwnerOf(self);JSAutoRealm realm(cx,state.page);
  JS::RootedValue value(cx,JS::GetReservedSlot(self,RuleStyle));
  JS::RootedObject view(cx);
  if(value.isObject()) view=&value.toObject();
  else {
    if(!state.NewStyleView("",&view,self)) return false;
    JS::SetReservedSlot(self,RuleStyle,JS::ObjectValue(*view));
  }
  auto& row=RuleDataOf(self);
  if(row.kind==6) row.declarations=ruyijs::domain::styling::CssDeclarations(text,
    ruyijs::domain::styling::CssDeclarationContext::Page);
  else row.declarations=row.kind==8?ruyijs::domain::styling::CssKeyframeDeclarations(text):
    ruyijs::domain::styling::CssDeclarationText(text);
  ++state.cssomGeneration;
  args.rval().setUndefined();return true;
}
namespace {
bool Constant(JSContext* cx,JS::HandleObject target,const char* name,int value) {
  return JS_DefineProperty(cx,target,name,value,JSPROP_READONLY|JSPROP_PERMANENT|JSPROP_ENUMERATE);
}
}
bool InstallRules(State& state) {
  auto& kit=state.kit;JS::RootedObject plain(kit.cx,JS::GetRealmObjectPrototype(kit.cx));
  if(!InstallRuleException(state)||!kit.Define("CSSRule",&kRuleClass,plain,WebKit::Reject,&state.rule)) return false;
  if(!kit.Accessor(state.rule,"CSSRule","type",TypeGet,nullptr,0)||
      !kit.Accessor(state.rule,"CSSRule","cssText",TextGet,TextSet,0)||
      !kit.Accessor(state.rule,"CSSRule","parentRule",ParentGet,nullptr,1)||
      !kit.Accessor(state.rule,"CSSRule","parentStyleSheet",ParentGet,nullptr,0)) return false;
  struct Entry { const char* name;int value; };
  const Entry constants[]={
    {"STYLE_RULE",1},{"CHARSET_RULE",2},{"IMPORT_RULE",3},{"MEDIA_RULE",4},
    {"FONT_FACE_RULE",5},{"PAGE_RULE",6},{"NAMESPACE_RULE",10},{"KEYFRAMES_RULE",7},
    {"KEYFRAME_RULE",8},{"COUNTER_STYLE_RULE",11},{"SUPPORTS_RULE",12},{"FONT_FEATURE_VALUES_RULE",14}};
  for(const auto& c:constants)
    if(!Constant(kit.cx,state.rule.constructor,c.name,c.value)||
        !Constant(kit.cx,state.rule.prototype,c.name,c.value)) return false;
  if(!OrderConstructor(kit,state.rule)||!InstallGroupingRules(state)||
      !kit.Define("CSSStyleRule",&kStyleRuleClass,state.groupingRule.prototype,WebKit::Reject,&state.styleRule)||
      !JS_SetPrototype(kit.cx,state.styleRule.constructor,state.groupingRule.constructor)) return false;
  // Firefox exposes this vendor rule at the CSSConditionRule branch. It has
  // no own members beyond the WebIDL toStringTag; conditionText stays owned
  // by the existing CSSConditionRule installer.
  if(!kit.Define("CSSMozDocumentRule",&kConditionRuleClass,state.conditionRule.prototype,
                WebKit::Reject,&state.mozDocumentRule)||
      !JS_SetPrototype(kit.cx,state.mozDocumentRule.constructor,state.conditionRule.constructor)||
      !OrderConstructor(kit,state.mozDocumentRule)) return false;
  if(!kit.Define("CSSNestedDeclarations",&kNestedDeclarationsClass,state.rule.prototype,WebKit::Reject,&state.nestedDeclarations)||
      !JS_SetPrototype(kit.cx,state.nestedDeclarations.constructor,state.rule.constructor)) return false;
  return kit.Accessor(state.nestedDeclarations,"CSSNestedDeclarations","style",RuleStyleGet,RuleStyleSet,1)&&
      OrderConstructor(kit,state.nestedDeclarations)&&
      kit.Accessor(state.styleRule,"CSSStyleRule","selectorText",TextGet,TextSet,1)&&
      kit.Accessor(state.styleRule,"CSSStyleRule","style",RuleStyleGet,RuleStyleSet,0)&&
      OrderConstructor(kit,state.styleRule)&&InstallKeyframeRules(state)&&InstallAtRuleInterfaces(state)&&InstallCssRuleInterfaces(state)&&InstallPageRules(state);
}
}

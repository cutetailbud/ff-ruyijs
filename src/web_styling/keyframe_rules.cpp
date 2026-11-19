// Keyframe members share the original rule value, declaration and list factory.
#include "web_styling/cssom.h"
#include "domain/styling/css_keyframes.h"
#include "js/Realm.h"
#include "js/Symbol.h"
namespace ruyijs::web_styling {
namespace css=ruyijs::domain::styling;
namespace {
bool TextGet(JSContext* cx,unsigned argc,JS::Value* vp) {
  auto args=JS::CallArgsFromVp(argc,vp);JS::RootedObject self(cx);
  if(!CssReceiver(cx,args.thisv(),WebKit::Slot(args)?&kKeyframeRuleClass:&kKeyframesRuleClass,&self)) return false;
  const auto text=RuleDataOf(self).selector;
  JS::RootedString value(cx,JS_NewStringCopyUTF8N(cx,JS::UTF8Chars(text.data(),text.size())));
  if(!value) return false;
  args.rval().setString(value);return true;
}
bool TextSet(JSContext* cx,unsigned argc,JS::Value* vp) {
  auto args=JS::CallArgsFromVp(argc,vp);JS::RootedObject self(cx);std::string text;
  const bool key=WebKit::Slot(args)!=0;
  if(!CssReceiver(cx,args.thisv(),key?&kKeyframeRuleClass:&kKeyframesRuleClass,&self)||
      !CssString(cx,args.get(0),&text)) return false;
  std::vector<float> positions;
  if(!key) RuleDataOf(self).selector=std::move(text);
  else if(css::CssKeyPositions(text,&positions)) {
    RuleDataOf(self).selector=css::CssKeyText(positions);
    RuleDataOf(self).key_positions=std::move(positions);
  }
  ++RuleOwnerOf(self).cssomGeneration;
  args.rval().setUndefined();return true;
}
bool ListGet(JSContext* cx,unsigned argc,JS::Value* vp) {
  auto args=JS::CallArgsFromVp(argc,vp);JS::RootedObject self(cx);
  if(!CssReceiver(cx,args.thisv(),&kKeyframesRuleClass,&self)) return false;
  if(WebKit::Slot(args)) args.rval().setNumber(double(RuleDataOf(self).children.size()));
  else args.rval().set(RuleSlotValue(self,RuleList));
  return JS_WrapValue(cx,args.rval());
}
bool Call(JSContext* cx,unsigned argc,JS::Value* vp) {
  auto args=JS::CallArgsFromVp(argc,vp);JS::RootedObject self(cx);
  const int op=WebKit::Slot(args);
  if(!CssReceiver(cx,args.thisv(),&kKeyframesRuleClass,&self)) return false;
  if(!argc) return WebKit::Fail(cx,"Keyframe rule method requires one argument");
  std::string text;if(!CssString(cx,args[0],&text)) return false;
  State& state=RuleOwnerOf(self);JS::RootedValue result(cx);
  {
    JSAutoRealm realm(cx,state.page);auto& rows=RuleDataOf(self).children;
    if(op==0) {
      // Firefox requires the retained stylesheet's parser for appendRule.
      if(RuleSlotValue(self,RuleParent).isObject()) {
        auto parsed=css::ParseCssKeyframes(text,true);
        if(parsed.valid&&parsed.rules.size()==1) {
          if(rows.size()>=100000) return state.Throw("QuotaExceededError","Too many keyframes");
          rows.reserve(rows.size()+1);
          if(!ShiftRuleWrappers(cx,self,uint32_t(rows.size()),true)) return false;
          rows.push_back(std::move(parsed.rules[0]));
          ++state.cssomGeneration;
        }
      }
    } else {
      std::vector<float> key;size_t index=rows.size();
      if(css::CssKeyPositions(text,&key))
        for(size_t i=rows.size();i>0;--i) if(rows[i-1]->key_positions==key) {index=i-1;break;}
      if(op==1) {
        if(index<rows.size()&&!DeleteCssRule(cx,self,uint32_t(index))) return false;
      } else {
        result.setNull();
        if(index<rows.size()) {
          JS::RootedObject rule(cx);
          if(!detail::NewCssRule(state,self,index,&rule)) return false;
          result.setObject(*rule);
        }
      }
    }
  }
  if(!JS_WrapValue(cx,&result)) return false;
  args.rval().set(result);return true;
}
}
bool InstallKeyframeRules(State& state) {
  auto& kit=state.kit;
  if(!kit.Define("CSSKeyframesRule",&kKeyframesRuleClass,state.rule.prototype,WebKit::Reject,&state.keyframesRule)||
      !kit.Define("CSSKeyframeRule",&kKeyframeRuleClass,state.rule.prototype,WebKit::Reject,&state.keyframeRule)||
      !JS_SetPrototype(kit.cx,state.keyframesRule.constructor,state.rule.constructor)||
      !JS_SetPrototype(kit.cx,state.keyframeRule.constructor,state.rule.constructor)) return false;
  if(!kit.Method(state.keyframesRule,"CSSKeyframesRule","appendRule",Call,1,0)||
      !kit.Method(state.keyframesRule,"CSSKeyframesRule","deleteRule",Call,1,1)||
      !kit.Method(state.keyframesRule,"CSSKeyframesRule","findRule",Call,1,2)||
      !kit.Accessor(state.keyframesRule,"CSSKeyframesRule","name",TextGet,TextSet,0)||
      !kit.Accessor(state.keyframesRule,"CSSKeyframesRule","cssRules",ListGet,nullptr,0)||
      !kit.Accessor(state.keyframesRule,"CSSKeyframesRule","length",ListGet,nullptr,1)||
      !OrderConstructor(kit,state.keyframesRule)||
      !kit.Accessor(state.keyframeRule,"CSSKeyframeRule","keyText",TextGet,TextSet,1)||
      !kit.Accessor(state.keyframeRule,"CSSKeyframeRule","style",RuleStyleGet,RuleStyleSet,2)||
      !OrderConstructor(kit,state.keyframeRule)) return false;
  JS::RootedObject array(kit.cx,JS::GetRealmArrayPrototype(kit.cx));JS::RootedValue values(kit.cx);
  JS::RootedId id(kit.cx,JS::PropertyKey::Symbol(JS::GetWellKnownSymbol(kit.cx,JS::SymbolCode::iterator)));
  return array&&JS_GetProperty(kit.cx,array,"values",&values)&&
    JS_DefinePropertyById(kit.cx,state.keyframesRule.prototype,id,values,0);
}
}

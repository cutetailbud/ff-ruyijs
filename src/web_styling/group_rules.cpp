// Group operations use the same rule vector, index shifts and native factories.
#include "web_styling/cssom.h"
#include "js/Realm.h"
namespace ruyijs::web_styling {
namespace {
bool GroupReceiver(JSContext* cx,JS::HandleValue value,JS::MutableHandleObject out) {
  if(!RuleReceiver(cx,value,out)) return false;
  const auto* c=JS::GetClass(out);
  return c==&kStyleRuleClass||c==&kMediaRuleClass||c==&kSupportsRuleClass||c==&kPageRuleClass||
    c==&kLayerBlockRuleClass||WebKit::Fail(cx,"CSSGroupingRule requires a grouping rule");
}
}
bool RuleGroupGet(JSContext* cx,unsigned argc,JS::Value* vp) {
  auto args=JS::CallArgsFromVp(argc,vp);
  JS::RootedObject self(cx);
  if(!GroupReceiver(cx,args.thisv(),&self)) return false;
  args.rval().set(JS::GetReservedSlot(self,RuleList));
  return JS_WrapValue(cx,args.rval());
}
bool RuleGroupInsert(JSContext* cx,unsigned argc,JS::Value* vp) {
  auto args=JS::CallArgsFromVp(argc,vp);
  JS::RootedObject self(cx);
  if(!GroupReceiver(cx,args.thisv(),&self)) return false;
  if(!argc) return WebKit::Fail(cx,"insertRule requires a rule");
  std::string text;uint32_t index=0;
  if(!CssString(cx,args[0],&text)||(argc>1&&!JS::ToUint32(cx,args[1],&index))||
      !InsertCssRule(cx,self,text,index)) return false;
  args.rval().setNumber(index);return true;
}
bool RuleGroupDelete(JSContext* cx,unsigned argc,JS::Value* vp) {
  auto args=JS::CallArgsFromVp(argc,vp);
  JS::RootedObject self(cx);
  if(!GroupReceiver(cx,args.thisv(),&self)) return false;
  if(!argc) return WebKit::Fail(cx,"deleteRule requires an index");
  uint32_t index;
  if(!JS::ToUint32(cx,args[0],&index)||!DeleteCssRule(cx,self,index)) return false;
  args.rval().setUndefined();return true;
}
bool RuleConditionGet(JSContext* cx,unsigned argc,JS::Value* vp) {
  auto args=JS::CallArgsFromVp(argc,vp);
  JS::RootedObject self(cx);
  if(!RuleReceiver(cx,args.thisv(),&self,false,true)) return false;
  auto text=RuleDataOf(self).ConditionText();
  JS::RootedString value(cx,JS_NewStringCopyUTF8N(cx,JS::UTF8Chars(text.data(),text.size())));
  if(!value) return false;
  args.rval().setString(value);return true;
}
bool RuleMediaGet(JSContext* cx,unsigned argc,JS::Value* vp) {
  auto args=JS::CallArgsFromVp(argc,vp);
  JS::RootedObject self(cx);
  if(!CssReceiver(cx,args.thisv(),&kMediaRuleClass,&self)) return false;
  args.rval().set(JS::GetReservedSlot(self,RuleMedia));
  return JS_WrapValue(cx,args.rval());
}
bool RuleMediaSet(JSContext* cx,unsigned argc,JS::Value* vp) {
  auto args=JS::CallArgsFromVp(argc,vp);
  JS::RootedObject self(cx);
  if(!CssReceiver(cx,args.thisv(),&kMediaRuleClass,&self)) return false;
  std::string text;
  if(!args.get(0).isNull()&&!CssString(cx,args.get(0),&text)) return false;
  RuleDataOf(self).media.set(text);
  ++RuleOwnerOf(self).cssomGeneration;
  args.rval().setUndefined();return true;
}
}

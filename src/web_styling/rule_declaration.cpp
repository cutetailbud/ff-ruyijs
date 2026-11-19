// Rule declaration facade: the original shared Rule is its sole data source.
#include "web_styling/page_descriptors.h"
#include "domain/styling/css_page_value.h"
#include "js/Wrapper.h"
#include "domain/styling/css_declarations.h"
#include "domain/styling/css_keyframes.h"
#include "js/Realm.h"

namespace ruyijs::web_styling::detail {
namespace css = ruyijs::domain::styling;
bool ReadViewBlock(JSContext* cx, JS::HandleObject view, std::string* out) {
  JS::RootedObject native(cx);
  native=js::CheckedUnwrapStatic(view);
  if(!native||!OwnsDeclaration(native)) return WebKit::Fail(cx,"Invalid CSS declaration receiver");
  native=DeclarationStorage(native);
  const auto& slot = JS::GetReservedSlot(native, 2);
  if (slot.isObject()) {
    JS::RootedObject rule(cx, &slot.toObject());
    *out = RuleDataOf(rule).DeclarationText(); return true;
  }
  const auto& block = JS::GetReservedSlot(native, 0);
  if (!block.isString()) { out->clear(); return true; }
  JS::RootedString text(cx, block.toString());
  return StringToAscii(cx, text, out);
}
bool WriteRuleBlock(State& caller, JS::HandleObject view, JS::HandleValue value) {
  JSContext* cx=caller.cx;
  std::string text;
  if (!CssString(cx, value, &text)) return false;
  JS::RootedObject rule(cx, &JS::GetReservedSlot(DeclarationStorage(view), 2).toObject());
  if(!CheckRuleDeclaration(caller,rule)) return false;
  JSAutoRealm realm(cx, RuleOwnerOf(rule).page);
  auto& row=RuleDataOf(rule);
  if(row.kind==6) row.declarations=css::CssDeclarations(text,css::CssDeclarationContext::Page);
  else row.declarations=row.kind==8?css::CssKeyframeDeclarations(text):css::CssDeclarationText(text);
  ++RuleOwnerOf(rule).cssomGeneration;
  return true;
}
bool SetRuleProperty(State& caller, JS::HandleObject view, const std::string& name,
    JS::HandleValue value, JS::HandleValue priority, bool remove, JS::MutableHandleValue out) {
  JSContext* cx=caller.cx;
  std::string text, flag;
  const bool page=JS::GetClass(view)==&kPageDescriptorsClass;
  if (!remove && !value.isNull() && (page||!value.isUndefined()) && !CssString(cx, value, &text)) return false;
  if (!remove && !priority.isNullOrUndefined() && !CssString(cx, priority, &flag)) return false;
  flag = Lower(flag);
  out.setUndefined();
  if (!remove && !text.empty() && !flag.empty() && flag != "important") return true;
  JS::RootedObject rule(cx, &JS::GetReservedSlot(DeclarationStorage(view), 2).toObject());
  auto& row = RuleDataOf(rule);
  if(page&&!remove&&!KnownDeclarationName(name)) return true;
  if((!page||(!remove&&!text.empty()))&&!CheckRuleDeclaration(caller,rule)) return false;
  JSAutoRealm realm(cx, RuleOwnerOf(rule).page);
  std::string old = row.kind==6?css::CssPageSet(row.PageDeclarations(),name,text,flag=="important",remove):
    css::CssSetDeclaration(row.StringDeclarations(),name,text,flag=="important",remove);
  ++RuleOwnerOf(rule).cssomGeneration;
  if (remove) {
    JS::RootedString result(cx, JS_NewStringCopyUTF8N(cx, JS::UTF8Chars(old.data(), old.size())));
    if (!result) return false;
    out.setString(result);
  }
  return JS_WrapValue(cx, out);
}
}

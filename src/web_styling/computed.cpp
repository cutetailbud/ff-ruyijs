// getComputedStyle and the declaration views (T17): the computed block and a
// CSS rule's style. The cascade runs in the imported styling owner; this file
// turns an Element into the domain's cascade input, serializes the result into
// the view's one store, and answers the members of Document Model's single
// CSSStyleDeclaration/CSSStyleProperties prototype through the style port.
#include "web_styling/internal.h"
#include "web_styling/computed_source.h"
#include "web_styling/realm.h"
#include "web_styling/page_descriptors.h"
#include "domain/styling/css_page_value.h"
#include "domain/styling/css_components.h"
#include "document_model/style_port.h"
#include "document_model/state.h"
#include "js/Wrapper.h"

#include <memory>
#include <vector>

namespace ruyijs::web_styling {
namespace {
using document_model::StyleMember;
// The serialized declaration block of a computed style. Custom properties are
// already serialized once per shared map in ReadComputed (computed.customsText),
// so appending them here is a single string copy, not a per-element rebuild.
std::string BuildComputedBlock(const ComputedStyle& computed) {
  std::string block;
  for (const auto& entry : computed.values) {
    // -x-lang is the internal language of nsStyleFont, not a CSS property.
    if (entry.first == "-x-lang") continue;
    block += detail::Serialize(entry.first, entry.second, false);
  }
  block += computed.customsText;
  return block;
}

bool ViewName(JSContext* cx, JS::HandleValue value, std::string* out,bool page=false) {
  if (!detail::ToAscii(cx, value, out)) return false;
  *out = page?(out->starts_with("--")?*out:detail::Lower(*out)):detail::Dashed(*out);
  return true;
}
bool Text(JSContext* cx, const std::string& text, JS::MutableHandleValue out) {
  JSString* string = JS_NewStringCopyUTF8N(cx, JS::UTF8Chars(text.data(), text.size()));
  if (!string) return false;
  out.setString(string);
  return true;
}
}  // namespace

const JSClass kStyleViewClass = {"CSSStyleDeclaration", JSCLASS_HAS_RESERVED_SLOTS(3)};

bool OwnsDeclaration(JSObject* object) {
  return JS::GetClass(object)==&kStyleViewClass||JS::GetClass(object)==&kPageDescriptorsClass;
}

// The style declaration port: Document Model's one CSSStyleDeclaration and
// CSSStyleProperties prototype runs these members on a view. Slot 0 holds a
// computed block, slot 1 this owner's State, slot 2 the rule a rule view edits.
static bool DispatchDeclaration(State& caller, JSContext* cx, StyleMember member, JS::HandleObject self,
    const JS::CallArgs& args, const char* property, const char* accessor,bool pageNamed) {
  const bool page=JS::GetClass(self)==&kPageDescriptorsClass;
  if(page&&!pageNamed&&(member==StyleMember::NamedGet||member==StyleMember::NamedSet))
    return WebKit::Fail(cx,"CSSStyleProperties member requires CSSStyleProperties");
  JS::RootedObject storage(cx,DeclarationStorage(self));
  State& state = *static_cast<State*>(JS::GetReservedSlot(storage, 1).toPrivate());
  bool rule = JS::GetReservedSlot(storage, 2).isObject();
  if (member == StyleMember::CssTextSet) {
    // A computed style's cssText has no setter effect; assignment is ignored.
    if (rule && !detail::WriteRuleBlock(caller, self, args.get(0))) return false;
    args.rval().setUndefined();
    return true;
  }
  if (member == StyleMember::ParentRule) {
    args.rval().set(JS::GetReservedSlot(storage, 2));
    return JS_WrapValue(cx, args.rval());
  }
  if (member == StyleMember::Store || member == StyleMember::Remove) {
    if (!rule) return state.Throw("NoModificationAllowedError", "CSSStyleDeclaration is read-only");
    std::string name;
    if (!ViewName(cx, args.get(0), &name,page)) return false;
    return detail::SetRuleProperty(caller, self, name, args.get(1), args.get(2),
                                   member == StyleMember::Remove, args.rval());
  }
  if (member == StyleMember::NamedSet) {
    if (!rule) {
      std::string message = std::string("CSSStyleProperties.") + accessor +
          " setter: Can't set value for property '" + property + "' in computed style";
      return state.Throw("NoModificationAllowedError", message.c_str());
    }
    JS::RootedValue priority(cx, JS::UndefinedValue());
    return detail::SetRuleProperty(caller, self, property, args.get(0), priority, false, args.rval());
  }
  std::string block, name, value;
  if (!page&&!detail::ReadViewBlock(cx, self, &block)) return false;
  bool important = false;
  switch (member) {
    case StyleMember::CssTextGet:
      return Text(cx,page?ruyijs::domain::styling::CssPageText(PageDeclarationRows(cx,self)):block,args.rval());
    case StyleMember::Length:
      args.rval().setNumber(double(page?PageDeclarationRows(cx,self).size():detail::CountProperties(block)));
      return true;
    case StyleMember::Item: {
      uint32_t index = 0;
      if (!JS::ToUint32(cx, args.get(0), &index)) return false;
      if(page) {const auto& rows=PageDeclarationRows(cx,self);if(index<rows.size()) name=rows[index].name;}
      else detail::PropertyNameAt(block, index, &name);
      return Text(cx, name, args.rval());
    }
    case StyleMember::NamedGet:
      if(page) value=ruyijs::domain::styling::CssPageLookup(PageDeclarationRows(cx,self),property,&important);
      else detail::Lookup(block, property, &value, &important);
      return Text(cx, value, args.rval());
    default:
      if (!ViewName(cx, args.get(0), &name,page)) return false;
      if(page) value=ruyijs::domain::styling::CssPageLookup(PageDeclarationRows(cx,self),name,&important);
      else detail::Lookup(block, name, &value, &important);
      if (member == StyleMember::Priority) value = important ? "important" : "";
      return Text(cx, value, args.rval());
  }
}

bool CallDeclaration(void* context,JSContext* cx,StyleMember member,JS::HandleObject self,
    const JS::CallArgs& args,const char* property,const char* accessor) {
  return DispatchDeclaration(*static_cast<State*>(context),cx,member,self,args,property,accessor,false);
}
bool CallPageDeclaration(JSContext* cx,StyleMember member,JS::HandleObject self,
    const JS::CallArgs& args,const char* property,const char* accessor) {
  return DispatchDeclaration(*detail::Self(args),cx,member,self,args,property,accessor,true);
}

bool GetComputedStyle(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  State& state = *detail::Self(args);
  bool is_page = args.thisv().isUndefined() ||
                 (args.thisv().isObject() && &args.thisv().toObject() == state.page);
  if (!is_page) return WebKit::Fail(cx, "getComputedStyle called on an incompatible receiver");
  if (!state.chainPort || !state.sheetsPort)
    return WebKit::Fail(cx, "getComputedStyle is not available");
  if (args.length() == 0 || !args.get(0).isObject())
    return WebKit::Fail(cx, "getComputedStyle requires an Element");
  JS::RootedObject element(cx, &args.get(0).toObject());
  // The pseudo-element argument has no box to resolve against in this runtime,
  // so it is ignored and the element's own declaration is returned (D37).
  JSObject* raw = js::CheckedUnwrapStatic(element);
  document_model::Wrapper* wrapper = raw && JS::GetClass(raw) == &document_model::kNodeClass
      ? static_cast<document_model::Wrapper*>(JS::GetReservedSlot(raw, 0).toPrivate()) : nullptr;
  uint64_t revision = 0, stamp = 0;
  const bool cacheable = wrapper && wrapper->owner && !wrapper->dead &&
      CascadeStamp(state, element, &stamp, true);
  if (cacheable) revision = wrapper->owner->model.Revision();
  auto& cache = state.computedBlocks;
  if (cacheable) {
    if (!cache.ready || cache.document != wrapper->key.document ||
        cache.revision != revision || cache.stamp != stamp) {
      cache.blocks.clear();
      cache.reader = std::make_unique<ComputedStyleReader>(state);
      cache.document = wrapper->key.document;
      cache.revision = revision;
      cache.stamp = stamp;
      cache.ready = true;
    } else {
      auto found = cache.blocks.find(wrapper->key.node);
      if (found != cache.blocks.end()) {
        JS::RootedObject view(cx);
        if (!state.NewStyleView(found->second, &view)) return false;
        args.rval().setObject(*view);
        return true;
      }
    }
  }
  ComputedStyle computed;
  if (cacheable) {
    if (!cache.reader) cache.reader = std::make_unique<ComputedStyleReader>(state);
    if (!cache.reader->Read(element, &computed)) return false;
  } else {
    if (!ReadComputed(state, element, &computed)) return false;
  }
  std::string block = BuildComputedBlock(computed);
  if (cacheable) cache.blocks.insert_or_assign(wrapper->key.node, block);
  JS::RootedObject view(cx);
  if (!state.NewStyleView(block, &view)) return false;
  args.rval().setObject(*view);
  return true;
}

// Firefox getDefaultComputedStyle(elt, pseudoElt): Window-only, user-agent cascade.
bool GetDefaultComputedStyle(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  State& state = *detail::Self(args);
  bool is_page = args.thisv().isUndefined() ||
                 (args.thisv().isObject() && &args.thisv().toObject() == state.page);
  if (!is_page) return WebKit::Fail(cx, "'getDefaultComputedStyle' called on an object that does not implement interface Window.");
  if (args.length() == 0)
    return WebKit::Fail(cx, "Window.getDefaultComputedStyle: At least 1 argument required, but only 0 passed");
  bool isElement = false;
  JS::RootedObject element(cx, args[0].isObject() ? &args[0].toObject() : nullptr);
  if (element && state.chainPort && !state.chainPort(state.chainContext, element, nullptr, &isElement)) return false;
  if (!isElement)
    return WebKit::Fail(cx, "Window.getDefaultComputedStyle: Argument 1 does not implement interface Element.");
  if (args.length() > 1 && !args[1].isUndefined()) {  // DOMString pseudoElt conversion
    JS::RootedString pseudo(cx, JS::ToString(cx, args[1]));
    if (!pseudo) return false;
  }
  ComputedStyle computed;
  ComputedStyleReader reader(state);
  if (!reader.ReadDefault(element, &computed)) return false;
  std::string block = BuildComputedBlock(computed);
  JS::RootedObject view(cx);
  if (!state.NewStyleView(block, &view)) return false;
  args.rval().setObject(*view);
  return true;
}

bool InstallComputed(State& state) {
  return detail::MethodOn(state.kit, state.page, "Window", "getComputedStyle", GetComputedStyle, 1, 0) &&
         detail::MethodOn(state.kit, state.page, "Window", "getDefaultComputedStyle", GetDefaultComputedStyle, 1, 0);
}
}  // namespace ruyijs::web_styling

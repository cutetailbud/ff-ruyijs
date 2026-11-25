// getComputedStyle and the declaration views (T17): the computed block and a
// CSS rule's style. The cascade runs in the imported styling owner; this file
// turns an Element into the domain's cascade input, caches structured records
// keyed by document/cascade/viewport state, resolves layout-dependent box
// properties lazily, and answers live CSSStyleDeclaration/CSSStyleProperties
// members through the style port.
#include "web_styling/internal.h"
#include "web_styling/computed_source.h"
#include "web_styling/condition_environment.h"
#include "web_styling/realm.h"
#include "web_styling/page_descriptors.h"
#include "domain/styling/css_page_value.h"
#include "domain/styling/css_components.h"
#include "domain/styling/stylesheet.h"
#include "domain/styling/css_transform_matrix.h"
#include "document_model/style_port.h"
#include "document_model/state.h"
#include "js/Realm.h"
#include "js/Wrapper.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <string_view>
#include <vector>

namespace ruyijs::web_styling {
namespace {
using document_model::StyleMember;
namespace css = ruyijs::domain::styling;

std::string BuildStandardBlock(const ComputedStyle& computed) {
  std::string block;
  block.reserve(computed.values.size() * 24);
  for (const auto& entry : computed.values) {
    // -x-lang is the internal language of nsStyleFont, not a CSS property.
    if (entry.first == "-x-lang") continue;
    block += detail::Serialize(entry.first, entry.second, false);
  }
  return block;
}

const std::string& SerializedCustoms(
    const std::shared_ptr<const std::map<std::string, css::CssProperty>>& customs) {
  static const std::string kEmpty;
  if (!customs || customs->empty()) return kEmpty;
  struct CustomsMemo {
    const void* identity = nullptr;
    std::string text;
  };
  thread_local std::vector<CustomsMemo> memo;
  const void* id = customs.get();
  for (size_t i = 0; i < memo.size(); ++i) {
    if (memo[i].identity == id) {
      if (i) std::rotate(memo.begin(), memo.begin() + i, memo.begin() + i + 1);
      return memo.front().text;
    }
  }
  std::string text;
  text.reserve(customs->size() * 32);
  for (const auto& [name, prop] : *customs) {
    text += name + ": " + css::CssClean(prop.value) + ";";
  }
  if (memo.size() >= 8) memo.pop_back();
  memo.insert(memo.begin(), {id, std::move(text)});
  return memo.front().text;
}

std::string BuildComputedBlock(const ComputedStyle& computed) {
  std::string block = BuildStandardBlock(computed);
  block += SerializedCustoms(computed.customs);
  return block;
}

void RebuildStandardRows(ComputedRecord* record) {
  record->standardRows = css::CssDeclarations(BuildStandardBlock(record->style));
  record->standardIndex.clear();
  record->standardIndex.reserve(record->standardRows.size());
  for (size_t i = 0; i < record->standardRows.size(); ++i) {
    record->standardIndex.emplace(record->standardRows[i].name, i);
  }
  record->serializedReady = false;
  record->serializedBlock.clear();
}

// A used length as Firefox serializes it: px, up to six significant digits
// after rounding to app units (1/60 px), no trailing zeros.
std::string UsedPixels(double value) {
  value = std::round(value * 60) / 60;
  char buffer[48];
  std::snprintf(buffer, sizeof buffer, "%.6g", value);
  std::string text(buffer);
  if (text == "-0") text = "0";
  return text + "px";
}
// Whether a computed value is a plain absolute length ("21.44px", "0px"),
// which Firefox returns as computed rather than as the used value.
bool PlainLength(const std::string& value) {
  if (value.size() < 3 || value.compare(value.size() - 2, 2, "px")) return false;
  char* end = nullptr;
  std::strtod(value.c_str(), &end);
  return end == value.c_str() + value.size() - 2;
}
// CSSOM resolved values (§9.1), as nsComputedDOMStyle gives them for a
// rendered element: width and height are always the used size; a margin,
// padding or inset is the used value only when its computed value is not a
// plain length (a percentage, calc(), auto), so an em margin stays 21.44px
// and a collapsed body margin stays its own 8px (#42).
void ResolveUsed(State& state, JS::HandleObject element, ComputedStyle* computed) {
  if (!state.usedBoxPort) return;
  UsedBox used;
  bool present = false;
  if (!state.usedBoxPort(state.usedBoxContext, element, &used, &present)) {
    JS_ClearPendingException(state.cx);
    return;
  }
  // transform resolves to the matrix Firefox folds the list into, translation
  // percentages against the border box (an empty box without one).
  for (auto& entry : computed->values)
    if (entry.first == "transform") {
      const double width = present ? used.width + used.padding[1] + used.padding[3] + used.border[1] + used.border[3] : 0;
      const double height = present ? used.height + used.padding[0] + used.padding[2] + used.border[0] + used.border[2] : 0;
      entry.second = ruyijs::domain::styling::CssComputedTransformMatrix(entry.second, width, height);
    }
  if (!present) return;
  static const char* const sides[] = {"top", "right", "bottom", "left"};
  std::vector<std::pair<std::string, std::string>> resolved = {
      {"width", UsedPixels(used.width)}, {"height", UsedPixels(used.height)}};
  for (int i = 0; i < 4; ++i) {
    resolved.push_back({std::string("padding-") + sides[i], UsedPixels(used.padding[i])});
    resolved.push_back({std::string("margin-") + sides[i], UsedPixels(used.margin[i])});
  }
  std::string position;
  for (const auto& entry : computed->values) if (entry.first == "position") position = entry.second;
  if (!position.empty() && position != "static")
    for (int i = 0; i < 4; ++i) {
      // An auto inset stays auto unless the opposite side fixes it.
      for (const auto& entry : computed->values)
        if (entry.first == sides[i] && entry.second != "auto")
          resolved.push_back({sides[i], UsedPixels(used.inset[i])});
    }
  for (auto& entry : computed->values)
    for (const auto& [name, value] : resolved)
      if (entry.first == name) {
        const bool box_size = name == "width" || name == "height";
        if (box_size || !PlainLength(entry.second)) entry.second = value;
        break;
      }
}

bool NeedsUsedLayout(std::string_view canonicalName) {
  return canonicalName == "width" || canonicalName == "height" ||
         canonicalName == "padding" || canonicalName == "padding-top" ||
         canonicalName == "padding-right" || canonicalName == "padding-bottom" ||
         canonicalName == "padding-left" ||
         canonicalName == "margin" || canonicalName == "margin-top" ||
         canonicalName == "margin-right" || canonicalName == "margin-bottom" ||
         canonicalName == "margin-left" ||
         canonicalName == "inset" || canonicalName == "top" ||
         canonicalName == "right" || canonicalName == "bottom" ||
         canonicalName == "left" || canonicalName == "transform";
}

uint64_t ViewportFingerprint(State& state) {
  css::CssMediaEnvironment env;
  if (!ReadConditionEnvironment(state, &env)) {
    JS_ClearPendingException(state.cx);
    return 0;
  }
  uint64_t hash = 1469598103934665603ull;
  const double nums[] = {env.width, env.height, env.deviceWidth, env.deviceHeight, env.ratio, env.color};
  const auto* bytes = reinterpret_cast<const unsigned char*>(nums);
  for (size_t i = 0; i < sizeof nums; ++i) { hash ^= bytes[i]; hash *= 1099511628211ull; }
  hash ^= static_cast<uint64_t>(env.touch);
  hash *= 1099511628211ull;
  return hash;
}

void EnsureUsedResolved(State& state, JS::HandleObject element, ComputedRecord* record) {
  if (record->usedResolved) return;
  record->usedResolved = true;
  JSAutoRealm realm(state.cx, state.page);
  JS::RootedObject stateElement(state.cx, element);
  if (!JS_WrapObject(state.cx, &stateElement)) {
    JS_ClearPendingException(state.cx);
    return;
  }
  ResolveUsed(state, stateElement, &record->style);
  RebuildStandardRows(record);
}

const std::string& EnsureSerializedComputed(State& state, JS::HandleObject element,
                                            ComputedRecord* record) {
  EnsureUsedResolved(state, element, record);
  if (!record->serializedReady) {
    record->serializedBlock = BuildComputedBlock(record->style);
    record->serializedReady = true;
  }
  return record->serializedBlock;
}

bool ObtainComputedRecord(State& state, JS::HandleObject element,
                          std::shared_ptr<ComputedRecord>* out) {
  JSAutoRealm realm(state.cx, state.page);
  JS::RootedObject stateElement(state.cx, element);
  if (!JS_WrapObject(state.cx, &stateElement)) return false;

  JSObject* raw = js::CheckedUnwrapStatic(stateElement);
  document_model::Wrapper* wrapper = raw && JS::GetClass(raw) == &document_model::kNodeClass
      ? static_cast<document_model::Wrapper*>(JS::GetReservedSlot(raw, 0).toPrivate()) : nullptr;

  uint64_t revision = 0, stamp = 0, viewport = 0;
  bool connected = false;
  const bool cacheable = wrapper && wrapper->owner && !wrapper->dead &&
      CascadeStamp(state, stateElement, &stamp);
  if (cacheable) {
    connected = wrapper->owner->model.IsConnected(wrapper->key);
    revision = connected
        ? ((wrapper->owner->model.ConnectedRevision() << 1) | 1ull)
        : (wrapper->owner->model.Revision() << 1);
    viewport = ViewportFingerprint(state);
  }

  auto& cache = state.computedBlocks;
  if (cacheable && connected) {
    if (!cache.ready || cache.document != wrapper->key.document ||
        cache.revision != revision || cache.stamp != stamp || cache.viewport != viewport) {
      cache.records.clear();
      cache.reader = std::make_unique<ComputedStyleReader>(state);
      cache.document = wrapper->key.document;
      cache.revision = revision;
      cache.stamp = stamp;
      cache.viewport = viewport;
      cache.ready = true;
    } else {
      const auto found = cache.records.find(wrapper->key.node);
      if (found != cache.records.end()) {
        *out = found->second;
        return true;
      }
    }
  }

  auto record = std::make_shared<ComputedRecord>();
  if (cacheable && connected) {
    if (!cache.reader) cache.reader = std::make_unique<ComputedStyleReader>(state);
    if (!cache.reader->Read(stateElement, &record->style)) return false;
  } else {
    if (!ReadComputed(state, stateElement, &record->style)) return false;
    record->usedResolved = true;  // Disconnected elements have no UsedBox.
  }
  RebuildStandardRows(record.get());
  if (cacheable && connected) {
    cache.records.insert_or_assign(wrapper->key.node, record);
  }
  *out = std::move(record);
  return true;
}

std::string LookupComputedProperty(State& state, JS::HandleObject element,
                                   ComputedRecord* record, const std::string& inputName) {
  const auto wanted = detail::Lower(inputName);
  if (wanted.size() >= 2 && wanted[0] == '-' && wanted[1] == '-') {
    if (!record->style.customs) return {};
    const auto it = record->style.customs->find(inputName);
    return it == record->style.customs->end() ? std::string() : css::CssClean(it->second.value);
  }
  const auto canon = css::CssCanonicalDeclarationName(wanted);
  if (canon.empty()) return {};
  if (NeedsUsedLayout(canon)) EnsureUsedResolved(state, element, record);
  bool important = false;
  if (css::CssShorthandName(canon) || canon == "border-image") {
    return css::CssLookupDeclaration(record->standardRows, canon, &important);
  }
  const auto it = record->standardIndex.find(canon);
  return it == record->standardIndex.end() ? std::string() : record->standardRows[it->second].value;
}

size_t ComputedPropertyCount(const ComputedRecord& record) {
  return record.standardRows.size() + (record.style.customs ? record.style.customs->size() : 0);
}

std::string ComputedPropertyNameAt(const ComputedRecord& record, size_t index) {
  if (index < record.standardRows.size()) return record.standardRows[index].name;
  if (!record.style.customs) return {};
  size_t rem = index - record.standardRows.size();
  if (rem >= record.style.customs->size()) return {};
  auto it = record.style.customs->begin();
  std::advance(it, rem);
  return it->first;
}

bool ViewName(JSContext* cx, JS::HandleValue value, std::string* out,bool page=false) {
  if (!detail::ToAscii(cx, value, out)) return false;
  *out = page ? (out->starts_with("--") ? *out : detail::Lower(*out))
              : (out->starts_with("--") ? *out : detail::Dashed(*out));
  return true;
}
bool Text(JSContext* cx, const std::string& text, JS::MutableHandleValue out) {
  JSString* string = JS_NewStringCopyUTF8N(cx, JS::UTF8Chars(text.data(), text.size()));
  if (!string) return false;
  out.setString(string);
  return true;
}
}  // namespace

const JSClass kStyleViewClass = {"CSSStyleDeclaration", JSCLASS_HAS_RESERVED_SLOTS(4)};

bool OwnsDeclaration(JSObject* object) {
  return JS::GetClass(object)==&kStyleViewClass||JS::GetClass(object)==&kPageDescriptorsClass;
}

// The style declaration port: Document Model's one CSSStyleDeclaration and
// CSSStyleProperties prototype runs these members on a view. Slot 0 holds a
// static block (getDefaultComputedStyle), slot 1 this owner's State, slot 2 the
// rule a rule view edits, and slot 3 the Element of a live getComputedStyle view.
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

  const auto& elementSlot = JS::GetReservedSlot(storage, 3);
  if (!page && !rule && elementSlot.isObject()) {
    JS::RootedObject element(cx, &elementSlot.toObject());
    std::shared_ptr<ComputedRecord> record;
    if (!ObtainComputedRecord(state, element, &record)) return false;
    switch (member) {
      case StyleMember::CssTextGet:
        return Text(cx, EnsureSerializedComputed(state, element, record.get()), args.rval());
      case StyleMember::Length:
        args.rval().setNumber(double(ComputedPropertyCount(*record)));
        return true;
      case StyleMember::Item: {
        uint32_t index = 0;
        if (!JS::ToUint32(cx, args.get(0), &index)) return false;
        return Text(cx, ComputedPropertyNameAt(*record, index), args.rval());
      }
      case StyleMember::NamedGet:
        return Text(cx, LookupComputedProperty(state, element, record.get(), property), args.rval());
      default: {
        std::string name;
        if (!ViewName(cx, args.get(0), &name, false)) return false;
        if (member == StyleMember::Priority) return Text(cx, "", args.rval());
        return Text(cx, LookupComputedProperty(state, element, record.get(), name), args.rval());
      }
    }
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
  bool isElement = false;
  if (!state.chainPort(state.chainContext, element, nullptr, &isElement)) return false;
  if (!isElement) return WebKit::Fail(cx, "getComputedStyle requires an Element");
  if (args.length() > 1 && !args.get(1).isNullOrUndefined()) {
    JS::RootedString pseudo(cx, JS::ToString(cx, args[1]));
    if (!pseudo) return false;
  }
  JS::RootedObject view(cx);
  if (!state.NewStyleView("", &view, nullptr, element)) return false;
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

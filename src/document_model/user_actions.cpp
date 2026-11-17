// Default actions of host user input (ARCHITECTURE.md, host user input): text
// insertion into input/textarea, the click's activation behavior (form controls,
// hyperlinks) and Enter's implicit submission. Each runs in the node's own realm.
#include "document_model/editing.h"
#include "document_model/focus_internal.h"
#include "document_model/form_submission.h"
#include "document_model/model_hyperlink.h"
#include "browser_environment/environment.h"
#include "interaction/realm.h"
#include "runtime/runtime.h"
#include "js/PropertyAndElement.h"
#include "js/Realm.h"
#include <vector>

namespace ruyijs::document_model {
using namespace focus_detail;
namespace {
// Follows `key`'s hyperlink in the main document's browsing context.
bool FollowHyperlink(State& s, NodeKey key) {
  DomString raw;
  if (!Has(s, key, u"href", &raw) || !Same(OwnerDocument(s, key), s.main)) return true;
  JSContext* cx = s.cx;
  JS::RootedValue base(cx), source(cx);
  JS::RootedObject location(cx, s.environment.location());
  std::string baseText, rawText, href;
  if (!location || !s.environment.URL(&base) || !Value(cx, DomStringView(raw), &source)) return true;
  JS::RootedString baseString(cx, base.toString()), rawString(cx, source.toString());
  if (!ToUTF8(cx, baseString, &baseText) || !ToUTF8(cx, rawString, &rawText) ||
      !s.Check(ReadHyperlink(rawText, baseText, HyperlinkField::Href, &href))) return false;
  JS::RootedString text(cx, JS_NewStringCopyN(cx, href.data(), href.size()));
  if (!text) return false;
  JS::RootedValue url(cx, JS::StringValue(text));
  return JS_SetProperty(cx, location, "href", url);
}
}  // namespace

// An editing host (or content inside one) edits at the window's selection.
bool EditIn(State& s, NodeKey key, EditCommand command, DomStringView text, bool* done, bool* changed) {
  NodeKey host;
  *done = false;
  if (!EditHost(s, key, &host)) return false;
  if (!host.node) return true;
  *done = true;
  return EditRun(s, host, command, text, changed);
}
bool InsertText(JSContext* cx, JS::HandleObject element, DomStringView text, bool* inserted) {
  *inserted = false;
  Wrapper* entry = Element(cx, element);
  if (!entry) return false;
  State& s = *entry->owner;
  NodeSnapshot node;
  if (!s.Check(s.model.Inspect(entry->key, &node))) return false;
  const DomString tag = Tag(node);
  if (tag != u"input" && tag != u"textarea") {
    bool done = false;
    const bool lineBreak = text == DomStringView(u"\n");
    return EditIn(s, entry->key, lineBreak ? EditCommand::InsertParagraph : EditCommand::InsertText,
                  lineBreak ? DomStringView() : text, &done, inserted);
  }
  bool disabled = false, submit = false;
  if ((tag != u"input" && tag != u"textarea") || Has(s, entry->key, u"readonly") ||
      !s.Check(s.model.FormControlInfo(entry->key, &disabled, &submit)) || disabled) return true;
  if (tag == u"textarea") {
    *inserted = true;
    return s.Check(s.model.TextareaRange(entry->key, text, 0, 0, false, u"end"));
  }
  DomString value;
  if (!s.Check(s.model.InputText(entry->key, u"value", &value))) return false;
  const int32_t end = int32_t(value.size());
  *inserted = true;
  return s.Check(s.model.InputRange(entry->key, text, end, end, true));
}
// Whether Enter inserts a line break in `element`: a textarea or an editing
// host (where it splits the paragraph, inputType insertParagraph).
bool Multiline(JSContext* cx, JS::HandleObject element, bool* multiline) {
  *multiline = false;
  Wrapper* entry = Element(cx, element);
  if (!entry) return false;
  NodeSnapshot node;
  if (!entry->owner->Check(entry->owner->model.Inspect(entry->key, &node))) return false;
  NodeKey host;
  if (Tag(node) != u"input" && !EditHost(*entry->owner, entry->key, &host)) return false;
  *multiline = Tag(node) == u"textarea" || host.node;
  return true;
}
bool Editing(JSContext* cx, JS::HandleObject element, bool* editing) {
  *editing = false;
  Wrapper* entry = Element(cx, element);
  if (!entry) return false;
  NodeSnapshot node;
  if (!entry->owner->Check(entry->owner->model.Inspect(entry->key, &node))) return false;
  if (Tag(node) == u"input" || Tag(node) == u"textarea") return true;
  NodeKey host;
  if (!EditHost(*entry->owner, entry->key, &host)) return false;
  *editing = host.node;
  return true;
}
// Backspace's default action: removes the character before the caret.
bool DeleteBackward(JSContext* cx, JS::HandleObject element, bool* deleted) {
  *deleted = false;
  Wrapper* entry = Element(cx, element);
  if (!entry) return false;
  State& s = *entry->owner;
  NodeSnapshot node;
  if (!s.Check(s.model.Inspect(entry->key, &node))) return false;
  const DomString tag = Tag(node);
  if (tag != u"input" && tag != u"textarea") {
    bool done = false;
    return EditIn(s, entry->key, EditCommand::DeleteBackward, DomStringView(), &done, deleted);
  }
  bool disabled = false, submit = false;
  if (Has(s, entry->key, u"readonly") ||
      !s.Check(s.model.FormControlInfo(entry->key, &disabled, &submit)) || disabled) return true;
  if (tag == u"textarea") {
    double start = 0, end = 0;
    if (!s.Check(s.model.TextareaNumber(entry->key, u"selectionStart", &start)) ||
        !s.Check(s.model.TextareaNumber(entry->key, u"selectionEnd", &end))) return false;
    if (start == end && !start) return true;
    *deleted = true;
    const uint32_t from = start == end ? uint32_t(start) - 1 : uint32_t(start);
    return s.Check(s.model.TextareaRange(entry->key, u"", from, uint32_t(end), true, u"end"));
  }
  DomString value;
  if (!s.Check(s.model.InputText(entry->key, u"value", &value))) return false;
  if (value.empty()) return true;
  *deleted = true;
  const int32_t end = int32_t(value.size());
  return s.Check(s.model.InputRange(entry->key, u"", end - 1, end, true));
}
// The element ancestors of `target` (inclusive) from the root element down,
// the elements a pointer entering from outside the document enters.
bool ElementPath(JSContext* cx, JS::HandleObject target, JS::MutableHandleObjectVector path) {
  Wrapper* entry = Element(cx, target);
  if (!entry) return false;
  State& s = *entry->owner;
  std::vector<NodeKey> keys;
  for (NodeKey key = entry->key; key.node && keys.size() < 1024;) {
    NodeSnapshot node;
    if (!s.Check(s.model.Inspect(key, &node))) return false;
    keys.push_back(key);
    key = node.parent_element;
  }
  for (auto key = keys.rbegin(); key != keys.rend(); ++key) {
    JS::RootedValue value(cx);
    if (!s.Wrap(*key, &value) || !path.append(&value.toObject())) return false;
  }
  return true;
}
// The click's activation behavior: the first inclusive ancestor with one.
bool Activate(JSContext* cx, JS::HandleObject target) {
  Wrapper* entry = Element(cx, target);
  if (!entry) return false;
  State& s = *entry->owner;
  for (NodeKey key = entry->key; key.node;) {
    NodeSnapshot node;
    if (!s.Check(s.model.Inspect(key, &node))) return false;
    const DomString tag = Tag(node);
    if ((tag == u"a" || tag == u"area") && Has(s, key, u"href")) return FollowHyperlink(s, key);
    if (tag == u"button" || tag == u"input") {
      JS::RootedValue value(cx);
      if (!s.Wrap(key, &value)) return false;
      Wrapper* control = s.Find(&value.toObject());
      return !control || ActivateFormControl(*control);
    }
    key = node.parent_element;
  }
  return true;
}
// Enter in a text input: a trusted click on the form's default button, which
// submits it, else the form is submitted (HTMLInputElement::MaybeSubmitForm).
bool ImplicitSubmit(JSContext* cx, JS::HandleObject element, double time_ms) {
  Wrapper* entry = Element(cx, element);
  if (!entry) return false;
  State& s = *entry->owner;
  NodeKey form;
  if (!s.Check(s.model.FormOwner(entry->key, &form)) || !form.node) return true;
  std::vector<NodeKey> controls;
  if (!s.Check(s.model.FormNodes(form, nullptr, &controls))) return false;
  for (const NodeKey& key : controls) {
    bool disabled = false, submit = false;
    if (!s.Check(s.model.FormControlInfo(key, &disabled, &submit))) return false;
    if (!submit) continue;
    if (disabled) return true;
    JS::RootedValue value(cx);
    JS::RootedObject init(cx, JS_NewPlainObject(cx));
    if (!init || !s.Wrap(key, &value)) return false;
    JS::RootedObject button(cx, &value.toObject());
    bool allowed = false;
    if (!interaction::DispatchTrusted(cx, button, "click", init, time_ms, &allowed)) return false;
    return !allowed || Activate(cx, button);
  }
  JS::RootedValue value(cx);
  if (!s.Wrap(form, &value)) return false;
  Wrapper* owner = s.Find(&value.toObject());
  return !owner || SubmitForm(*owner, JS::NullHandleValue, true);
}
}  // namespace ruyijs::document_model

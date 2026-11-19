#include "runtime/resource_schema.h"
#include "runtime/resource_buffer.h"
#include "jsapi.h"
#include "js/Array.h"
#include "js/PropertyAndElement.h"
#include "js/String.h"

namespace ruyijs::resource_wire {
static JSString* String(JSContext* cx, const std::string& text, bool utf8 = false) {
  return utf8 ? JS_NewStringCopyUTF8N(cx, JS::UTF8Chars(text.data(), text.size())) :
                JS_NewStringCopyN(cx, text.data(), text.size());
}
bool Stats(JSContext* cx, const resource_network::ResourceStats& stats, JS::MutableHandleValue result) {
  JS::RootedObject object(cx, JS_NewPlainObject(cx));
  if (!object || !JS_DefineProperty(cx, object, "resources", double(stats.resources), JSPROP_ENUMERATE) ||
      !JS_DefineProperty(cx, object, "bytes", double(stats.bytes), JSPROP_ENUMERATE)) return false;
  result.setObject(*object);
  return true;
}
bool Snapshot(JSContext* cx, const resource_network::ResourceSnapshot& source,
              JS::MutableHandleValue result) {
  std::string encoded;
  if (!resource_network::Report(cx, EncodeBase64(source.body, &encoded))) return false;
  JS::RootedString data(cx, String(cx, encoded));
  JS::RootedString kind(cx, JS_NewStringCopyZ(cx,
      source.kind == resource_network::BodyKind::Text ? "text" : "bytes"));
  JS::RootedString url(cx, String(cx, source.url, true));
  JS::RootedObject body(cx, JS_NewPlainObject(cx)), object(cx, JS_NewPlainObject(cx));
  JS::RootedObject headers(cx, JS::NewArrayObject(cx, source.headers.size()));
  if (!data || !kind || !url || !body || !object || !headers) return false;
  for (size_t i = 0; i < source.headers.size(); ++i) {
    JS::RootedObject pair(cx, JS::NewArrayObject(cx, 2));
    JS::RootedString name(cx, String(cx, source.headers[i].first));
    JS::RootedString value(cx, String(cx, source.headers[i].second));
    if (!pair || !name || !value ||
        !JS_DefineElement(cx, pair, 0, name, JSPROP_ENUMERATE) ||
        !JS_DefineElement(cx, pair, 1, value, JSPROP_ENUMERATE) ||
        !JS_DefineElement(cx, headers, i, pair, JSPROP_ENUMERATE)) return false;
  }
  if (!JS_DefineProperty(cx, body, "kind", kind, JSPROP_ENUMERATE) ||
      !JS_DefineProperty(cx, body, "base64", data, JSPROP_ENUMERATE) ||
      !JS_DefineProperty(cx, object, "url", url, JSPROP_ENUMERATE) ||
      !JS_DefineProperty(cx, object, "body", body, JSPROP_ENUMERATE) ||
      !JS_DefineProperty(cx, object, "status", source.status, JSPROP_ENUMERATE) ||
      !JS_DefineProperty(cx, object, "headers", headers, JSPROP_ENUMERATE)) return false;
  result.setObject(*object);
  return true;
}
bool LogEntries(JSContext* cx, const resource_network::NetLogRecords& source,
                JS::MutableHandleValue result) {
  JS::RootedObject entries(cx, JS::NewArrayObject(cx, source.size()));
  if (!entries) return false;
  for (size_t i = 0; i < source.size(); ++i) {
    // Every allocation is checked before it is used: under memory pressure a
    // null string boxed by JS::StringValue is a string-tagged null that
    // JS_DefineProperty dereferences, ending the whole engine process (#46).
    JS::RootedObject entry(cx, JS_NewPlainObject(cx));
    if (!entry) return false;
    // Request fields come from the page ByteString conversion, so they are
    // copied back as Latin-1 rather than decoded as UTF-8.
    JS::RootedString method(cx, String(cx, source[i].method));
    JS::RootedString url(cx, String(cx, source[i].url));
    JS::RootedString body(cx, String(cx, source[i].requestBody));
    JS::RootedString error(cx, String(cx, source[i].error));
    if (!method || !url || !body || !error) return false;
    // Explicit BooleanValue: the bool overload of JS_DefineProperty resolves to
    // the numeric one and would publish matched as 1 or 0.
    JS::RootedValue matched(cx, JS::BooleanValue(source[i].matched));
    if(!source[i].cookieNames.empty()) {
      JS::RootedObject names(cx,JS::NewArrayObject(cx,source[i].cookieNames.size()));
      if(!names)return false;
      for(size_t j=0;j<source[i].cookieNames.size();++j) {
        JS::RootedString name(cx,String(cx,source[i].cookieNames[j]));
        if(!name||!JS_DefineElement(cx,names,j,name,JSPROP_ENUMERATE))return false;
      }
      if(!JS_DefineProperty(cx,entry,"cookieNames",names,JSPROP_ENUMERATE))return false;
    }
    if (!JS_DefineProperty(cx, entry, "id", double(source[i].id), JSPROP_ENUMERATE) ||
        !JS_DefineProperty(cx, entry, "method", method, JSPROP_ENUMERATE) ||
        !JS_DefineProperty(cx, entry, "url", url, JSPROP_ENUMERATE) ||
        !JS_DefineProperty(cx, entry, "requestBody", body, JSPROP_ENUMERATE) ||
        !JS_DefineProperty(cx, entry, "status", source[i].status, JSPROP_ENUMERATE) ||
        !JS_DefineProperty(cx, entry, "matched", matched, JSPROP_ENUMERATE) ||
        !JS_DefineProperty(cx, entry, "error", error, JSPROP_ENUMERATE) ||
        !JS_DefineElement(cx, entries, i, entry, JSPROP_ENUMERATE)) return false;
  }
  JS::RootedObject object(cx, JS_NewPlainObject(cx));
  if (!object || !JS_DefineProperty(cx, object, "entries", entries, JSPROP_ENUMERATE))
    return false;
  result.setObject(*object);
  return true;
}
}

#include "runtime/worker_nested.h"
#include "runtime/runtime.h"
#include "execution_scheduling/realm.h"
#include "web_platform/realm.h"
#include "browser_environment/environment.h"
#include "domain/browser_environment/url_value.h"
#include "web_workers/realm.h"
#include "web_workers/child.h"
#include "jsapi.h"
#include <memory>
#include <string>

namespace ruyijs {
namespace {
using UrlValue = domain::browser_environment::UrlValue;
bool Text(JSContext* cx, JS::HandleValue value, std::string* out) {
  if (!value.isString()) return false;
  JS::RootedString text(cx, value.toString());
  return ToUTF8(cx, text, out);
}
}  // namespace

bool InstallNestedWorkers(web_workers::Realm& workers, NestedWorkerPorts& ports) {
  if (!workers.Install()) return false;
  workers.SetTaskPort([](void* context, JS::HandleObject task, double delay) {
    return static_cast<execution_scheduling::Realm*>(context)->PostDelayedTask(task, delay);
  }, &ports.scheduling);
  workers.SetLogicalClockPort([](void* context) {
    return static_cast<execution_scheduling::Realm*>(context)->Inspect().logical;
  }, &ports.scheduling);
  workers.SetObjectUrlPort([](void* context, const std::string& url, std::string* bytes, std::string* type) {
    return static_cast<web_platform::Realm*>(context)->ReadObjectUrl(url, bytes, type);
  }, &ports.platform);
  workers.SetEnvironmentPort([](void* context, std::string* result) {
    return static_cast<browser_environment::Environment*>(context)->ExportWorkerProfile(result);
  }, &ports.environment);
  // The script URL resolves against this worker's own URL; same-origin with it.
  workers.SetScriptUrlPort([](void* context, const std::string& input, std::string* url, bool* sameOrigin) {
    auto& ports = *static_cast<NestedWorkerPorts*>(context);
    auto& environment = ports.environment;
    JSContext* cx = ports.cx;
    JS::RootedValue href(cx);
    std::string base;
    UrlValue resolved, own;
    if (!environment.URL(&href) || !Text(cx, href, &base) || !UrlValue::Resolve(input, base, &resolved)) {
      JS_ClearPendingException(cx);
      return false;
    }
    *url = resolved.href();
    *sameOrigin = UrlValue::Resolve(base, base, &own) && own.origin() == resolved.origin();
    return true;
  }, &ports);
  // A grandchild's service requests travel over this worker's own service channel.
  workers.SetServiceFactory([](void* context, web_workers::Service* result) {
    auto* child = static_cast<web_workers::Child*>(context);
    result->owner = std::shared_ptr<void>(child, [](void*) {});
    result->request = [](void* owner, std::string_view request, std::string* reply) {
      return static_cast<web_workers::Child*>(owner)->Exchange(request, reply);
    };
    return true;
  }, &ports.child);
  return true;
}

void UnbindNestedWorkers(web_workers::Realm& workers) {
  workers.Stop();
  workers.SetTaskPort(nullptr, nullptr);
  workers.SetLogicalClockPort(nullptr, nullptr);
  workers.SetObjectUrlPort(nullptr, nullptr);
  workers.SetEnvironmentPort(nullptr, nullptr);
  workers.SetScriptUrlPort(nullptr, nullptr);
  workers.SetServiceFactory(nullptr, nullptr);
}
}  // namespace ruyijs

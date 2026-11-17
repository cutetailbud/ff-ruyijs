#include "web_indexeddb/state.h"
#include "js/Object.h"
#include <algorithm>
#include <set>
namespace ruyijs::web_indexeddb {
bool Schedule(Store& store) {
  if (store.scheduled || store.jobs.empty()) return true;
  State& s=*store.anchor;
  if (!store.post) return WebKit::Fail(s.cx,"IndexedDB scheduling port is not installed");
  JSAutoRealm realm(s.cx,s.kit.page);
  if (!store.post(store.context,s.pump)) return false;
  store.scheduled=true;return true;
}
bool Pump(JSContext* cx,unsigned n,JS::Value* vp) {
  auto args=JS::CallArgsFromVp(n,vp);auto& anchor=*static_cast<State*>(WebKit::Owner(args));
  Store& store=*anchor.store;store.scheduled=false;
  std::set<Key> waiting;
  bool completed=false;
  size_t count=store.jobs.size();
  for (size_t i=0;i<count;++i) {
    Job& job=*store.jobs[i];
    if (job.done || (!job.kind && waiting.count(job.key))) continue;
    State& s=*job.owner;JSAutoRealm realm(cx,s.kit.page);
    if (job.upgrading) {
      if (UpgradePending(store,job)) { waiting.insert(job.key);continue; }
      if (!FinishUpgrade(job)) return false;
      completed=job.done;if(completed)break;
      continue;
    }
    if (job.kind) {
      if(!DataJob(job))return false;
      completed=job.done;if(completed)break;
      continue;
    }
    Result status=store.model.Inspect(job.key,job.version,job.remove);
    if (status.code==Code::Blocked && !job.notified) {
      job.notified=true;
      size_t connections=store.connections.size();
      for (size_t j=0;j<connections;++j) {
        Connection& connection=*store.connections[j];
        if (connection.key!=job.key || !store.model.Active(connection.id)) continue;
        if (!VersionEvent(*connection.owner,connection.object,"versionchange",status.oldVersion,
                          status.version,job.remove)) return false;
      }
      status=store.model.Inspect(job.key,job.version,job.remove);
    }
    if (status.code==Code::Blocked) {
      if (!job.blocked) {
        job.blocked=true;
        if (!VersionEvent(s,job.request,"blocked",status.oldVersion,status.version,job.remove)) return false;
      }
      // close() in a callback posts one retry; no busy retry while blocked.
      waiting.insert(job.key);continue;
    }
    if (!Complete(job)) return false;
    completed=job.done;
    if (completed) break; // Engine microtasks run between request completions.
  }
  store.jobs.erase(std::remove_if(store.jobs.begin(),store.jobs.end(),
      [](const auto& job){return job->done;}),store.jobs.end());
  args.rval().setUndefined();return !completed || Schedule(store);
}
// A later top-level document takes over the Context's one store before any of
// its scripts run: the store's task belongs to the active document, whose
// realm the Scheduling owner still runs (the replaced document's tasks are
// retired with it). The replaced document's pending requests end with it and
// its connections close, as Firefox aborts a discarded document's
// transactions and closes its databases. Records and versions persist.
bool Adopt(State& s) {
  Store& store=*s.store;
  for (const auto& job:store.jobs) {
    if (job->kind==3 && !job->done)
      { JS::RootedObject tx(s.cx,job->request); EndTransaction(s,tx,false); }
    if (job->upgrading) store.model.Revert(job->connection);
  }
  store.jobs.clear();
  for (const auto& connection:store.connections) store.model.Close(connection->id);
  store.connections.clear();
  JSAutoRealm realm(s.cx,s.kit.page);
  if (!s.pump) s.pump=s.kit.Function(Pump,"IndexedDB task",0,0);
  if (!s.pump) return false;
  store.anchor=&s;store.scheduled=false;
  return true;
}
}

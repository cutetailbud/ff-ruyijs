#include "runtime/worker_tasks.h"
#include "web_workers/realm.h"
#include "execution_scheduling/realm.h"
namespace ruyijs {
void BindWorkerTasks(web_workers::Realm& workers, execution_scheduling::Realm& scheduling) {
  workers.SetTaskPort([](void* context, JS::HandleObject task, double delay) {
    return static_cast<execution_scheduling::Realm*>(context)->PostDelayedTask(task, delay);
  }, &scheduling);
  workers.SetLogicalClockPort([](void* context) {
    return static_cast<execution_scheduling::Realm*>(context)->Inspect().logical;
  }, &scheduling);
}
}

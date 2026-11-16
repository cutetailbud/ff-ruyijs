#pragma once

#include "domain/shared/aggregate_root.h"
#include "domain/execution_scheduling/task.h"
#include "domain/execution_scheduling/time_point.h"
#include "domain/execution_scheduling/clock_state.h"

#include <algorithm>
#include <memory>
#include <set>
#include <unordered_map>
#include <vector>

namespace ruyijs::domain::execution_scheduling {

using EventLoopId = uint64_t;

/// Aggregate Root: EventLoop
/// Manages the micro/macro task queues and virtual time.
/// Implements the HTML spec event loop processing model:
///   1. Pick oldest due macro-task → execute
///   2. Drain all microtasks
///   3. (Optionally) run rAF callbacks
///   4. Repeat
class EventLoopAggregate : public shared::AggregateRoot<EventLoopId> {
public:
    explicit EventLoopAggregate(EventLoopId id, TimeMode mode = TimeMode::Logical)
        : AggregateRoot<EventLoopId>(id) { clock_.mode = mode; }

    // --- Time management ---

    TimePoint current_time() const noexcept { return clock_.elapsed; }
    TimeMode time_mode() const noexcept { return clock_.mode; }
    ClockState& clock() noexcept { return clock_; }
    const ClockState& clock() const noexcept { return clock_; }
    TaskId last_task_id() const noexcept { return next_task_id_; }
    std::shared_ptr<Task> find_task(TaskId id) const noexcept {
        auto found = tasks_.find(id);
        return found == tasks_.end() ? nullptr : found->second.task;
    }
    std::shared_ptr<Task> next_live_task() const noexcept {
        std::shared_ptr<Task> ordinary, idle, expired, web;
        for (const auto& key : ordered_) {
            const auto& task = at(key);
            if (!task->cancelled() && !task->running()) { ordinary = task; break; }
        }
        for (const auto& key : idle_) {
            const auto& task = at(key);
            if (task->cancelled() || task->running()) continue;
            if (!idle) idle = task;
            if (task->idle_timeout() <= clock_.elapsed.ms() &&
                (!expired || task->idle_timeout() < expired->idle_timeout())) expired = task;
        }
        if (ordinary && ordinary->type() == Task::Type::WebTask) {
            for (const auto& key : web_) {
                const auto& task = at(key);
                if (!task->is_due(clock_.elapsed)) continue;
                if(!web || task->web_priority()<web->web_priority() ||
                   (task->web_priority()==web->web_priority() && task->continuation() && !web->continuation()) ||
                   (task->web_priority()==web->web_priority() && task->continuation()==web->continuation() && task->id()<web->id()))web=task;
            }
        }
        if (expired && (!ordinary || !ordinary->is_due(clock_.elapsed) ||
                        expired->idle_timeout() < ordinary->deadline().ms())) return expired;
        if (ordinary && ordinary->is_due(clock_.elapsed)) return ordinary->type()==Task::Type::WebTask?web:ordinary;
        if (idle && (!ordinary || idle->deadline() <= ordinary->deadline())) return idle;
        return ordinary;
    }
    double idle_deadline() const noexcept {
        double end = clock_.elapsed.ms() + 50.0;
        for (const auto& key : ordered_) {
            const auto& task = at(key);
            if (!task->cancelled() && !task->running()) { end = std::min(end, task->deadline().ms()); break; }
        }
        return std::max(clock_.elapsed.ms(), end);
    }
    std::shared_ptr<Task> eligible_frame(TaskId cutoff, TimePoint timestamp) const noexcept {
        for (const auto& key : frames_) {
            if (TimePoint(key.deadline) > timestamp) break;
            const auto& task = at(key);
            if (!task->cancelled() && !task->running() && task->id() <= cutoff) return task;
        }
        return nullptr;
    }

    void advance_time(double ms) {
        clock_.elapsed = TimePoint(clock_.elapsed.ms() + ms);
    }

    void set_time(TimePoint t) { clock_.elapsed = t; }

    // --- Task scheduling ---

    TaskId schedule_task(Task::Type type, TaskPriority priority, double delay_ms) {
        TaskId id = next_task_id_ + 1;
        TimePoint deadline(clock_.elapsed.ms() + delay_ms);
        auto task = std::make_shared<Task>(id, type, priority, deadline);
        if (type == Task::Type::Microtask) {
            microtask_queue_.push_back(task);
        } else {
            insert(task);
        }
        next_task_id_ = id;
        return id;
    }

    TaskId schedule_interval(double interval_ms) {
        TaskId id = next_task_id_ + 1;
        TimePoint deadline(clock_.elapsed.ms() + interval_ms);
        auto task = std::make_shared<Task>(id, Task::Type::Interval, TaskPriority::Timer, deadline);
        task->set_interval(interval_ms);
        insert(task);
        next_task_id_ = id;
        return id;
    }

    void cancel_task(TaskId id) {
        auto found = tasks_.find(id);
        if (found != tasks_.end()) {
            if (!found->second.task->cancelled()) cancelled_.push_back(id);
            found->second.task->cancel();
            return;
        }
        for (auto& task : microtask_queue_) {
            if (task->id() == id) { task->cancel(); return; }
        }
    }

    // --- Task queries ---

    /// Get all due macro-tasks at current time (ordered by deadline + priority)
    std::vector<std::shared_ptr<Task>> get_due_macrotasks() const {
        std::vector<std::shared_ptr<Task>> due;
        for (const auto* lane : {&ordered_, &idle_}) {
            for (const auto& key : *lane) {
                if (TimePoint(key.deadline) > clock_.elapsed) break;
                if (at(key)->is_due(clock_.elapsed)) due.push_back(at(key));
            }
        }
        std::sort(due.begin(), due.end(), [](const auto& a, const auto& b) { return *a < *b; });
        return due;
    }

    /// Get all pending microtasks (FIFO order)
    std::vector<std::shared_ptr<Task>> get_pending_microtasks() const {
        std::vector<std::shared_ptr<Task>> pending;
        for (const auto& task : microtask_queue_) {
            if (!task->cancelled()) {
                pending.push_back(task);
            }
        }
        return pending;
    }

    /// Remove a task from the queue after execution
    void complete_task(TaskId id) {
        // Check if it's a repeating interval
        auto found = tasks_.find(id);
        if (found != tasks_.end()) {
            auto task = found->second.task;
            if (task->is_repeating() && !task->cancelled()) {
                // The caller may have moved the deadline; the lanes keep the
                // key the task was inserted with until it is relinked here.
                unlink(found->second);
                task->reschedule();
                found->second.key = key_of(*task);
                link(found->second);
            } else {
                erase(found);
            }
            return;
        }
        // Remove from microtask queue
        microtask_queue_.erase(
            std::remove_if(microtask_queue_.begin(), microtask_queue_.end(),
                [id](const auto& t) { return t->id() == id; }),
            microtask_queue_.end());
    }

    /// Remove all cancelled tasks
    void purge_cancelled() {
        // Every macro-task is cancelled through cancel_task, which records it.
        for (TaskId id : cancelled_) {
            auto found = tasks_.find(id);
            if (found != tasks_.end() && found->second.task->cancelled()) erase(found);
        }
        cancelled_.clear();
        microtask_queue_.erase(
            std::remove_if(microtask_queue_.begin(), microtask_queue_.end(),
                [](const auto& t) { return t->cancelled(); }),
            microtask_queue_.end());
    }

    // --- Statistics ---

    bool has_pending_tasks() const {
        return !tasks_.empty() || !microtask_queue_.empty();
    }

    size_t macrotask_count() const noexcept { return tasks_.size(); }
    size_t microtask_count() const noexcept { return microtask_queue_.size(); }

    /// Next deadline (earliest due time among macrotasks)
    TimePoint next_deadline() const {
        if (ordered_.empty() && idle_.empty()) return TimePoint(1e18);
        if (idle_.empty() || (!ordered_.empty() && *ordered_.begin() < *idle_.begin()))
            return TimePoint(ordered_.begin()->deadline);
        return TimePoint(idle_.begin()->deadline);
    }

    // --- Auto-advance configuration ---

    double auto_advance_step() const noexcept { return auto_advance_step_; }
    void set_auto_advance_step(double ms) noexcept { auto_advance_step_ = ms; }

    double date_advance_step() const noexcept { return date_advance_step_; }
    void set_date_advance_step(double ms) noexcept { date_advance_step_ = ms; }

private:
    // Macro-tasks are indexed by id and kept in (deadline, priority, id) order,
    // the order of the former sorted vector, so registration, lookup,
    // cancellation and completion stay logarithmic as the queue grows (#34).
    // Idle callbacks have their own lane; Scheduler tasks and animation frames
    // are also in sub-lanes read only by the selections that need them.
    struct Key {
        double deadline;
        uint8_t priority;
        TaskId id;
        bool operator<(const Key& other) const noexcept {
            if (deadline != other.deadline) return deadline < other.deadline;
            if (priority != other.priority) return priority < other.priority;
            return id < other.id;
        }
    };
    struct Entry {
        std::shared_ptr<Task> task;
        Key key;
    };
    static Key key_of(const Task& task) noexcept {
        return {task.deadline().ms(), static_cast<uint8_t>(task.priority()), task.id()};
    }
    const std::shared_ptr<Task>& at(const Key& key) const { return tasks_.at(key.id).task; }
    void insert(const std::shared_ptr<Task>& task) {
        auto found = tasks_.emplace(task->id(), Entry{task, key_of(*task)}).first;
        try {
            link(found->second);
        } catch (...) {
            erase(found);
            throw;
        }
    }
    void link(const Entry& entry) {
        const auto type = entry.task->type();
        (type == Task::Type::IdleCallback ? idle_ : ordered_).insert(entry.key);
        if (type == Task::Type::WebTask) web_.insert(entry.key);
        if (type == Task::Type::AnimationFrame) frames_.insert(entry.key);
    }
    void unlink(const Entry& entry) noexcept {
        ordered_.erase(entry.key);
        idle_.erase(entry.key);
        web_.erase(entry.key);
        frames_.erase(entry.key);
    }
    void erase(std::unordered_map<TaskId, Entry>::iterator found) noexcept {
        unlink(found->second);
        tasks_.erase(found);
    }

    ClockState clock_;
    uint64_t next_task_id_ = 0;
    std::unordered_map<TaskId, Entry> tasks_;
    std::set<Key> ordered_, idle_, web_, frames_;
    std::vector<TaskId> cancelled_;
    std::vector<std::shared_ptr<Task>> microtask_queue_;
    double auto_advance_step_ = 0.001;  // performance.now() increment per eval
    double date_advance_step_ = 1.0;    // Date.now() increment per eval
};

} // namespace ruyijs::domain::execution_scheduling

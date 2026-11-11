#pragma once

#include "domain/shared/aggregate_root.h"
#include "domain/execution_scheduling/task.h"
#include "domain/execution_scheduling/time_point.h"
#include "domain/execution_scheduling/clock_state.h"

#include <algorithm>
#include <map>
#include <memory>
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
        auto it = macrotasks_by_id_.find(id);
        if (it != macrotasks_by_id_.end()) return it->second.task;
        for (const auto& task : microtask_queue_) if (task->id() == id) return task;
        return nullptr;
    }
    std::shared_ptr<Task> next_live_task() const noexcept {
        Task* ordinary = nullptr;
        for (const auto& [key, task] : ordered_non_idle_) {
            (void)key;
            if (!task->cancelled() && !task->running()) {
                ordinary = task;
                break;
            }
        }
        Task* idle = nullptr;
        Task* expired = nullptr;
        if (!idle_tasks_.empty()) {
            const double now_ms = clock_.elapsed.ms();
            for (const auto& [key, task] : idle_tasks_) {
                (void)key;
                if (task->cancelled() || task->running()) continue;
                if (!idle) idle = task;
                if (task->idle_timeout() <= now_ms &&
                    (!expired || task->idle_timeout() < expired->idle_timeout())) {
                    expired = task;
                }
            }
        }
        if (expired && (!ordinary || !ordinary->is_due(clock_.elapsed) ||
                        expired->idle_timeout() < ordinary->deadline().ms())) {
            return shared_macrotask(expired->id());
        }
        if (ordinary && ordinary->is_due(clock_.elapsed)) {
            if (ordinary->type() == Task::Type::WebTask) {
                Task* web = nullptr;
                for (const auto& [key, task] : web_tasks_) {
                    if (key.deadline > clock_.elapsed) break;
                    if (task->cancelled() || task->running()) continue;
                    if (!web || task->web_priority() < web->web_priority() ||
                        (task->web_priority() == web->web_priority() && task->continuation() && !web->continuation()) ||
                        (task->web_priority() == web->web_priority() && task->continuation() == web->continuation() &&
                         task->id() < web->id())) {
                        web = task;
                    }
                }
                return web ? shared_macrotask(web->id()) : nullptr;
            }
            return shared_macrotask(ordinary->id());
        }
        if (idle && (!ordinary || idle->deadline() <= ordinary->deadline())) {
            return shared_macrotask(idle->id());
        }
        return ordinary ? shared_macrotask(ordinary->id()) : nullptr;
    }
    double idle_deadline() const noexcept {
        double end = clock_.elapsed.ms() + 50.0;
        for (const auto& [key, task] : ordered_non_idle_) {
            (void)key;
            if (!task->cancelled() && !task->running()) {
                end = std::min(end, task->deadline().ms());
                break;
            }
        }
        return std::max(clock_.elapsed.ms(), end);
    }
    std::shared_ptr<Task> eligible_frame(TaskId cutoff, TimePoint timestamp) const noexcept {
        for (const auto& [key, task] : frame_tasks_) {
            if (key.deadline > timestamp) break;
            if (!task->cancelled() && !task->running() && task->id() <= cutoff) {
                return shared_macrotask(task->id());
            }
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
            microtask_queue_.push_back(std::move(task));
        } else {
            insert_macrotask(std::move(task));
        }
        next_task_id_ = id;
        return id;
    }

    TaskId schedule_interval(double interval_ms) {
        TaskId id = next_task_id_ + 1;
        TimePoint deadline(clock_.elapsed.ms() + interval_ms);
        auto task = std::make_shared<Task>(id, Task::Type::Interval, TaskPriority::Timer, deadline);
        task->set_interval(interval_ms);
        insert_macrotask(std::move(task));
        next_task_id_ = id;
        return id;
    }

    void cancel_task(TaskId id) noexcept {
        auto it = macrotasks_by_id_.find(id);
        if (it != macrotasks_by_id_.end()) {
            if (!it->second.task->cancelled()) {
                it->second.task->cancel();
                try {
                    cancelled_macrotasks_.push_back(id);
                } catch (...) {
                    cancelled_overflow_ = true;
                }
            }
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
        for (const auto& [key, task] : ordered_macrotasks_) {
            if (key.deadline > clock_.elapsed) break;
            if (task->is_due(clock_.elapsed)) {
                if (auto owned = shared_macrotask(task->id())) {
                    due.push_back(std::move(owned));
                }
            }
        }
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
    void complete_task(TaskId id) noexcept {
        auto it = macrotasks_by_id_.find(id);
        if (it != macrotasks_by_id_.end()) {
            Task* raw = it->second.task.get();
            const MacroKey old_key = it->second.key;
            if (raw->is_repeating() && !raw->cancelled()) {
                raw->reschedule();
                const MacroKey new_key = make_key(*raw);
                it->second.key = new_key;
                rekey_map(ordered_macrotasks_, old_key, new_key);
                rekey_map(ordered_non_idle_, old_key, new_key);
            } else {
                erase_macrotask_indices(old_key, raw->type());
                macrotasks_by_id_.erase(it);
            }
            return;
        }
        if (!microtask_queue_.empty()) {
            microtask_queue_.erase(
                std::remove_if(microtask_queue_.begin(), microtask_queue_.end(),
                    [id](const auto& t) { return t->id() == id; }),
                microtask_queue_.end());
        }
    }

    /// Remove all cancelled tasks
    void purge_cancelled() noexcept {
        if (cancelled_overflow_) {
            cancelled_macrotasks_.clear();
            cancelled_overflow_ = false;
            for (auto it = macrotasks_by_id_.begin(); it != macrotasks_by_id_.end(); ) {
                if (it->second.task->cancelled()) {
                    erase_macrotask_indices(it->second.key, it->second.task->type());
                    it = macrotasks_by_id_.erase(it);
                } else {
                    ++it;
                }
            }
        } else if (!cancelled_macrotasks_.empty()) {
            for (TaskId id : cancelled_macrotasks_) {
                auto it = macrotasks_by_id_.find(id);
                if (it != macrotasks_by_id_.end() && it->second.task->cancelled()) {
                    erase_macrotask_indices(it->second.key, it->second.task->type());
                    macrotasks_by_id_.erase(it);
                }
            }
            cancelled_macrotasks_.clear();
        }
        if (!microtask_queue_.empty()) {
            microtask_queue_.erase(
                std::remove_if(microtask_queue_.begin(), microtask_queue_.end(),
                    [](const auto& t) { return t->cancelled(); }),
                microtask_queue_.end());
        }
    }

    // --- Statistics ---

    bool has_pending_tasks() const noexcept {
        return !macrotasks_by_id_.empty() || !microtask_queue_.empty();
    }

    size_t macrotask_count() const noexcept { return macrotasks_by_id_.size(); }
    size_t microtask_count() const noexcept { return microtask_queue_.size(); }

    /// Next deadline (earliest due time among macrotasks)
    TimePoint next_deadline() const noexcept {
        if (ordered_macrotasks_.empty()) return TimePoint(1e18);
        return ordered_macrotasks_.begin()->first.deadline;
    }

    // --- Auto-advance configuration ---

    double auto_advance_step() const noexcept { return auto_advance_step_; }
    void set_auto_advance_step(double ms) noexcept { auto_advance_step_ = ms; }

    double date_advance_step() const noexcept { return date_advance_step_; }
    void set_date_advance_step(double ms) noexcept { date_advance_step_ = ms; }

private:
    struct MacroKey {
        TimePoint deadline;
        uint8_t priority = 0;
        TaskId id = 0;
        bool operator<(const MacroKey& other) const noexcept {
            if (deadline != other.deadline) return deadline < other.deadline;
            if (priority != other.priority) return priority < other.priority;
            return id < other.id;
        }
    };

    struct MacroEntry {
        std::shared_ptr<Task> task;
        MacroKey key;
    };

    static MacroKey make_key(const Task& task) noexcept {
        return {task.deadline(), static_cast<uint8_t>(task.priority()), task.id()};
    }

    static void rekey_map(std::map<MacroKey, Task*>& index,
                          const MacroKey& old_key,
                          const MacroKey& new_key) noexcept {
        auto node = index.extract(old_key);
        if (!node.empty()) {
            node.key() = new_key;
            index.insert(std::move(node));
        }
    }

    std::shared_ptr<Task> shared_macrotask(TaskId id) const noexcept {
        auto it = macrotasks_by_id_.find(id);
        return it != macrotasks_by_id_.end() ? it->second.task : nullptr;
    }

    void insert_macrotask_indices(const MacroKey& key, Task* task) {
        ordered_macrotasks_.emplace(key, task);
        if (task->type() != Task::Type::IdleCallback) {
            ordered_non_idle_.emplace(key, task);
            if (task->type() == Task::Type::AnimationFrame) {
                frame_tasks_.emplace(key, task);
            } else if (task->type() == Task::Type::WebTask) {
                web_tasks_.emplace(key, task);
            }
        } else {
            idle_tasks_.emplace(key, task);
        }
    }

    void erase_macrotask_indices(const MacroKey& key, Task::Type type) noexcept {
        ordered_macrotasks_.erase(key);
        if (type != Task::Type::IdleCallback) {
            ordered_non_idle_.erase(key);
            if (type == Task::Type::AnimationFrame) {
                frame_tasks_.erase(key);
            } else if (type == Task::Type::WebTask) {
                web_tasks_.erase(key);
            }
        } else {
            idle_tasks_.erase(key);
        }
    }

    void insert_macrotask(std::shared_ptr<Task> task) {
        const MacroKey key = make_key(*task);
        Task* raw = task.get();
        auto [it, inserted] = macrotasks_by_id_.emplace(raw->id(), MacroEntry{std::move(task), key});
        (void)inserted;
        try {
            insert_macrotask_indices(key, raw);
        } catch (...) {
            erase_macrotask_indices(key, raw->type());
            macrotasks_by_id_.erase(it);
            throw;
        }
    }

    ClockState clock_;
    uint64_t next_task_id_ = 0;
    std::unordered_map<TaskId, MacroEntry> macrotasks_by_id_;
    std::map<MacroKey, Task*> ordered_macrotasks_;
    std::map<MacroKey, Task*> ordered_non_idle_;
    std::map<MacroKey, Task*> frame_tasks_;
    std::map<MacroKey, Task*> idle_tasks_;
    std::map<MacroKey, Task*> web_tasks_;
    std::vector<TaskId> cancelled_macrotasks_;
    bool cancelled_overflow_ = false;
    std::vector<std::shared_ptr<Task>> microtask_queue_;
    double auto_advance_step_ = 0.001;  // performance.now() increment per eval
    double date_advance_step_ = 1.0;    // Date.now() increment per eval
};

} // namespace ruyijs::domain::execution_scheduling

#pragma once

#include "task.hpp"
#include <vector>
#include <unordered_map>
#include <algorithm>
#include <iostream>

namespace cerberus {

struct RealTimeMutex {
    int mutex_id = 0;
    int owner_task_id = -1; // -1 if unlocked
    std::vector<uint32_t> waiting_task_ids;
    uint32_t lock_count = 0;
    uint32_t priority_boost_count = 0;
};

/**
 * @brief Priority Inheritance Protocol (PIP) Mutex Manager
 * Dynamically elevates owner task's priority to prevent Priority Inversion deadlocks.
 */
class PriorityInheritanceManager {
public:
    PriorityInheritanceManager() = default;

    void register_mutex(int mutex_id) {
        RealTimeMutex m;
        m.mutex_id = mutex_id;
        mutexes_[mutex_id] = m;
    }

    /**
     * @brief Task attempts to acquire mutex
     * Returns true if locked immediately, false if blocked (priority boosted if applicable).
     */
    bool acquire(int mutex_id, TaskControlBlock& task, std::unordered_map<uint32_t, TaskControlBlock>& all_tasks) {
        if (mutexes_.find(mutex_id) == mutexes_.end()) return false;
        auto& mtx = mutexes_[mutex_id];

        if (mtx.owner_task_id == -1) {
            // Unlocked: grant lock immediately
            mtx.owner_task_id = static_cast<int>(task.task_id);
            mtx.lock_count++;
            return true;
        }

        // Mutex already held by another task -> Block calling task
        task.state = TaskState::BLOCKED;
        mtx.waiting_task_ids.push_back(task.task_id);

        // Check Priority Inheritance: if calling task has higher priority (numerically smaller value)
        uint32_t owner_id = static_cast<uint32_t>(mtx.owner_task_id);
        if (all_tasks.find(owner_id) != all_tasks.end()) {
            auto& owner_task = all_tasks[owner_id];
            if (task.dynamic_priority < owner_task.dynamic_priority) {
                // Boost owner's priority to match high-priority waiter
                owner_task.dynamic_priority = task.dynamic_priority;
                mtx.priority_boost_count++;
            }
        }

        return false;
    }

    /**
     * @brief Task releases mutex
     * Restores owner's static priority and unblocks highest priority waiter.
     */
    void release(int mutex_id, TaskControlBlock& owner_task, std::unordered_map<uint32_t, TaskControlBlock>& all_tasks) {
        if (mutexes_.find(mutex_id) == mutexes_.end()) return;
        auto& mtx = mutexes_[mutex_id];

        if (mtx.owner_task_id != static_cast<int>(owner_task.task_id)) return;

        // Restore owner's priority back to static base priority
        owner_task.dynamic_priority = owner_task.static_priority;
        mtx.owner_task_id = -1;

        if (!mtx.waiting_task_ids.empty()) {
            // Find highest priority waiting task (lowest numerical priority value)
            auto best_it = std::min_element(
                mtx.waiting_task_ids.begin(),
                mtx.waiting_task_ids.end(),
                [&](uint32_t a, uint32_t b) {
                    return all_tasks[a].dynamic_priority < all_tasks[b].dynamic_priority;
                }
            );

            uint32_t next_owner_id = *best_it;
            mtx.waiting_task_ids.erase(best_it);

            mtx.owner_task_id = static_cast<int>(next_owner_id);
            all_tasks[next_owner_id].state = TaskState::READY;
        }
    }

    const RealTimeMutex* get_mutex(int mutex_id) const {
        auto it = mutexes_.find(mutex_id);
        return (it != mutexes_.end()) ? &it->second : nullptr;
    }

private:
    std::unordered_map<int, RealTimeMutex> mutexes_;
};

} // namespace cerberus

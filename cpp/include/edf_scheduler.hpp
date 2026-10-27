#pragma once

#include "task.hpp"
#include <vector>
#include <queue>
#include <unordered_map>
#include <iostream>

namespace cerberus {

struct EDFComparator {
    bool operator()(const TaskControlBlock* a, const TaskControlBlock* b) const {
        return a->absolute_deadline > b->absolute_deadline; // Min-heap by earliest deadline
    }
};

/**
 * @brief Earliest Deadline First (EDF) Dynamic Priority Scheduler
 * Optimal dynamic uniprocessor scheduler achieving up to 100% theoretical CPU utilization.
 */
class EDFScheduler {
public:
    EDFScheduler() = default;

    void add_task(const TaskControlBlock& t) {
        tasks_[t.task_id] = t;
    }

    double total_utilization() const {
        double u = 0.0;
        for (const auto& kv : tasks_) {
            u += static_cast<double>(kv.second.wcet_ticks) / kv.second.period_ticks;
        }
        return u;
    }

    bool is_schedulable() const {
        return total_utilization() <= 1.0;
    }

    /**
     * @brief Execute 1 discrete tick under EDF scheduling
     */
    int step(uint32_t tick) {
        // 1. Release periodic jobs
        for (auto& kv : tasks_) {
            auto& t = kv.second;
            if (tick % t.period_ticks == 0) {
                t.release_new_job(tick);
            }
        }

        // 2. Deadline checks
        for (auto& kv : tasks_) {
            auto& t = kv.second;
            if (t.state != TaskState::COMPLETED && t.remaining_exec > 0 && tick > t.absolute_deadline) {
                t.deadlines_missed++;
            }
        }

        // 3. Find task with earliest absolute deadline
        TaskControlBlock* selected = nullptr;
        for (auto& kv : tasks_) {
            auto& t = kv.second;
            if (t.state == TaskState::READY || t.state == TaskState::RUNNING) {
                if (!selected || t.absolute_deadline < selected->absolute_deadline) {
                    selected = &t;
                }
            }
        }

        if (!selected) {
            return -1; // Idle tick
        }

        selected->state = TaskState::RUNNING;
        selected->remaining_exec--;

        if (selected->remaining_exec == 0) {
            selected->state = TaskState::COMPLETED;
            selected->jobs_completed++;
            if (tick <= selected->absolute_deadline) {
                selected->deadlines_met++;
            }
        }

        return static_cast<int>(selected->task_id);
    }

    const std::unordered_map<uint32_t, TaskControlBlock>& get_tasks() const {
        return tasks_;
    }

private:
    std::unordered_map<uint32_t, TaskControlBlock> tasks_;
};

} // namespace cerberus

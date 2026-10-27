#pragma once

#include "task.hpp"
#include "sync_primitives.hpp"
#include <vector>
#include <unordered_map>
#include <cmath>
#include <iostream>
#include <algorithm>

namespace cerberus {

struct SchedulabilityReport {
    double total_utilization = 0.0;
    double liu_layland_bound = 0.0;
    bool passes_liu_layland = false;
    bool passes_exact_rta = true;
    std::unordered_map<uint32_t, uint32_t> worst_case_response_times;
};

/**
 * @brief Rate Monotonic Scheduling (RMS) Engine with Exact Response Time Analysis
 */
class RMSScheduler {
public:
    RMSScheduler() = default;

    void add_task(const TaskControlBlock& t) {
        tasks_[t.task_id] = t;
    }

    /**
     * @brief Formally analyzes taskset schedulability before execution
     */
    SchedulabilityReport analyze_schedulability() const {
        SchedulabilityReport rep;
        size_t n = tasks_.size();
        if (n == 0) return rep;

        // 1. Total CPU Utilization
        for (const auto& kv : tasks_) {
            rep.total_utilization += static_cast<double>(kv.second.wcet_ticks) / kv.second.period_ticks;
        }

        // 2. Liu & Layland Sufficient Bound: n * (2^(1/n) - 1)
        rep.liu_layland_bound = static_cast<double>(n) * (std::pow(2.0, 1.0 / n) - 1.0);
        rep.passes_liu_layland = (rep.total_utilization <= rep.liu_layland_bound);

        // 3. Exact Response Time Analysis (RTA) Fixed-Point Iteration
        std::vector<TaskControlBlock> sorted_tasks;
        for (const auto& kv : tasks_) sorted_tasks.push_back(kv.second);
        std::sort(sorted_tasks.begin(), sorted_tasks.end(), [](const TaskControlBlock& a, const TaskControlBlock& b) {
            return a.period_ticks < b.period_ticks; // RMS order: smaller period = higher priority
        });

        for (size_t i = 0; i < sorted_tasks.size(); ++i) {
            const auto& ti = sorted_tasks[i];
            uint32_t R = ti.wcet_ticks;
            bool schedulable = false;

            while (R <= ti.relative_deadline) {
                uint32_t interference = 0;
                for (size_t j = 0; j < i; ++j) {
                    const auto& tj = sorted_tasks[j];
                    interference += static_cast<uint32_t>(std::ceil(static_cast<double>(R) / tj.period_ticks)) * tj.wcet_ticks;
                }
                uint32_t R_next = ti.wcet_ticks + interference;
                if (R_next == R) {
                    schedulable = true;
                    break;
                }
                R = R_next;
            }

            rep.worst_case_response_times[ti.task_id] = R;
            if (!schedulable || R > ti.relative_deadline) {
                rep.passes_exact_rta = false;
            }
        }

        return rep;
    }

    /**
     * @brief Execute 1 discrete RTOS system tick
     */
    int step(uint32_t tick, PriorityInheritanceManager& pim) {
        // 1. Release periodic jobs
        for (auto& kv : tasks_) {
            auto& t = kv.second;
            if (tick % t.period_ticks == 0) {
                t.release_new_job(tick);
            }
        }

        // 2. Check for deadline misses among active jobs
        for (auto& kv : tasks_) {
            auto& t = kv.second;
            if ((t.state == TaskState::READY || t.state == TaskState::RUNNING || t.state == TaskState::BLOCKED) &&
                t.remaining_exec > 0 && tick > t.absolute_deadline) {
                t.deadlines_missed++;
            }
        }

        // 3. Find highest priority (lowest dynamic priority value) READY/RUNNING task
        TaskControlBlock* selected = nullptr;
        for (auto& kv : tasks_) {
            auto& t = kv.second;
            if (t.state == TaskState::READY || t.state == TaskState::RUNNING) {
                if (!selected || t.dynamic_priority < selected->dynamic_priority) {
                    selected = &t;
                }
            }
        }

        if (!selected) {
            return -1; // Idle tick
        }

        selected->state = TaskState::RUNNING;

        // Handle Mutex if task needs critical section access
        if (selected->required_mutex_id >= 0 && selected->critical_section_remaining > 0) {
            bool locked = pim.acquire(selected->required_mutex_id, *selected, tasks_);
            if (!locked) {
                // Task blocked waiting for mutex! Re-dispatch next ready task
                return step(tick, pim);
            }
            selected->critical_section_remaining--;
            if (selected->critical_section_remaining == 0) {
                pim.release(selected->required_mutex_id, *selected, tasks_);
            }
        }

        // Execute task for 1 tick
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

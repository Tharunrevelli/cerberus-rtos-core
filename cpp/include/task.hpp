#pragma once

#include <cstdint>
#include <string>

namespace cerberus {

enum class TaskState {
    READY,
    RUNNING,
    BLOCKED,
    COMPLETED
};

inline const char* task_state_str(TaskState s) {
    switch (s) {
        case TaskState::READY: return "READY";
        case TaskState::RUNNING: return "RUNNING";
        case TaskState::BLOCKED: return "BLOCKED";
        case TaskState::COMPLETED: return "COMPLETED";
        default: return "UNKNOWN";
    }
}

struct TaskControlBlock {
    uint32_t task_id = 0;
    std::string name;
    uint32_t static_priority = 0;   // Lower numerical value = higher priority
    uint32_t dynamic_priority = 0;  // Dynamically adjusted via PIP/PCP
    uint32_t period_ticks = 0;      // Periodic task period (T)
    uint32_t wcet_ticks = 0;        // Worst-Case Execution Time (C)
    uint32_t relative_deadline = 0; // Relative deadline (D <= T)

    // Dynamic runtime state
    uint32_t absolute_deadline = 0;
    uint32_t remaining_exec = 0;
    uint32_t release_tick = 0;
    TaskState state = TaskState::READY;

    // Mutex synchronization
    int required_mutex_id = -1; // -1 if none
    uint32_t critical_section_duration = 0;
    uint32_t critical_section_remaining = 0;

    // Statistics
    uint32_t jobs_released = 0;
    uint32_t jobs_completed = 0;
    uint32_t deadlines_met = 0;
    uint32_t deadlines_missed = 0;

    void release_new_job(uint32_t current_tick) {
        release_tick = current_tick;
        absolute_deadline = current_tick + relative_deadline;
        remaining_exec = wcet_ticks;
        critical_section_remaining = critical_section_duration;
        state = TaskState::READY;
        dynamic_priority = static_priority;
        jobs_released++;
    }
};

} // namespace cerberus

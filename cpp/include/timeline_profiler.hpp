#pragma once

#include <vector>
#include <string>
#include <cstdint>
#include <iostream>
#include <iomanip>

namespace cerberus {

struct TimelineSlice {
    uint32_t tick;
    int task_id; // -1 if IDLE
    std::string task_name;
    uint32_t current_priority;
    bool is_preemption;
    bool is_priority_boosted;
};

class TimelineProfiler {
public:
    TimelineProfiler() : total_ticks_(0), idle_ticks_(0), context_switches_(0), priority_boosts_(0) {}

    void record_tick(uint32_t tick, int task_id, const std::string& name, uint32_t priority, bool priority_boosted) {
        total_ticks_++;
        bool preemption = false;

        if (task_id == -1) {
            idle_ticks_++;
        } else if (!history_.empty() && history_.back().task_id != task_id && history_.back().task_id != -1) {
            context_switches_++;
            if (priority < history_.back().current_priority) {
                preemption = true; // High priority preempted lower
            }
        }

        if (priority_boosted) {
            priority_boosts_++;
        }

        history_.push_back({tick, task_id, name, priority, preemption, priority_boosted});
    }

    double cpu_utilization() const {
        if (total_ticks_ == 0) return 0.0;
        return (static_cast<double>(total_ticks_ - idle_ticks_) / total_ticks_) * 100.0;
    }

    uint32_t context_switches() const { return context_switches_; }
    uint32_t priority_boosts() const { return priority_boosts_; }
    const std::vector<TimelineSlice>& history() const { return history_; }

private:
    uint32_t total_ticks_;
    uint32_t idle_ticks_;
    uint32_t context_switches_;
    uint32_t priority_boosts_;
    std::vector<TimelineSlice> history_;
};

} // namespace cerberus

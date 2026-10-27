#include "memory_pool.hpp"
#include "task.hpp"
#include "sync_primitives.hpp"
#include "rms_scheduler.hpp"
#include "edf_scheduler.hpp"
#include "timeline_profiler.hpp"

#include <iostream>
#include <cassert>
#include <vector>

using namespace cerberus;

void test_memory_pool() {
    constexpr size_t BLOCK_SIZE = 64;
    constexpr size_t BLOCK_COUNT = 8;
    FixedMemoryPool<BLOCK_SIZE, BLOCK_COUNT> pool;

    assert(pool.free_count() == BLOCK_COUNT);
    assert(pool.allocated_count() == 0);

    std::vector<void*> ptrs;
    for (size_t i = 0; i < BLOCK_COUNT; ++i) {
        void* p = pool.allocate();
        assert(p != nullptr);
        ptrs.push_back(p);
    }

    assert(pool.free_count() == 0);
    assert(pool.allocate() == nullptr); // Deterministic exhaustion

    // Free all blocks
    for (void* p : ptrs) {
        assert(pool.deallocate(p));
    }

    assert(pool.free_count() == BLOCK_COUNT);

    // Foreign pointer test
    uint8_t dummy[128];
    assert(!pool.deallocate(dummy));

    std::cout << "[PASS] FixedMemoryPool deterministic allocation tests passed.\n";
}

void test_priority_inheritance() {
    PriorityInheritanceManager pim;
    pim.register_mutex(1); // SPI_BUS_MUTEX

    std::unordered_map<uint32_t, TaskControlBlock> task_map;

    // Task Low: static priority 10
    TaskControlBlock task_low;
    task_low.task_id = 1;
    task_low.name = "LOW_PRIORITY_TASK";
    task_low.static_priority = 10;
    task_low.dynamic_priority = 10;
    task_low.state = TaskState::RUNNING;
    task_map[1] = task_low;

    // Task High: static priority 2
    TaskControlBlock task_high;
    task_high.task_id = 2;
    task_high.name = "HIGH_PRIORITY_TASK";
    task_high.static_priority = 2;
    task_high.dynamic_priority = 2;
    task_high.state = TaskState::READY;
    task_map[2] = task_high;

    // 1. Task Low acquires mutex
    assert(pim.acquire(1, task_map[1], task_map));
    assert(pim.get_mutex(1)->owner_task_id == 1);
    assert(task_map[1].dynamic_priority == 10);

    // 2. Task High attempts to acquire -> BLOCKED
    assert(!pim.acquire(1, task_map[2], task_map));
    assert(task_map[2].state == TaskState::BLOCKED);

    // 3. Verify Priority Inheritance: Task Low dynamic priority boosted to 2!
    assert(task_map[1].dynamic_priority == 2);
    assert(pim.get_mutex(1)->priority_boost_count == 1);

    // 4. Task Low releases mutex
    pim.release(1, task_map[1], task_map);

    // 5. Verify Task Low restored to base priority 10, Task High now owner & READY
    assert(task_map[1].dynamic_priority == 10);
    assert(pim.get_mutex(1)->owner_task_id == 2);
    assert(task_map[2].state == TaskState::READY);

    std::cout << "[PASS] Priority Inheritance Protocol (PIP) tests passed.\n";
}

void test_schedulability_analysis() {
    RMSScheduler rms;

    // Classical 3-task set
    TaskControlBlock t1;
    t1.task_id = 1; t1.wcet_ticks = 1; t1.period_ticks = 4; t1.relative_deadline = 4;
    TaskControlBlock t2;
    t2.task_id = 2; t2.wcet_ticks = 2; t2.period_ticks = 6; t2.relative_deadline = 6;
    TaskControlBlock t3;
    t3.task_id = 3; t3.wcet_ticks = 3; t3.period_ticks = 12; t3.relative_deadline = 12;

    rms.add_task(t1);
    rms.add_task(t2);
    rms.add_task(t3);

    auto rep = rms.analyze_schedulability();
    // U = 1/4 + 2/6 + 3/12 = 0.25 + 0.333 + 0.25 = 0.8333
    assert(std::abs(rep.total_utilization - 0.8333) < 0.01);
    // Liu & Layland bound for n=3 is 3*(2^(1/3)-1) = 0.7797
    assert(!rep.passes_liu_layland); // U > bound
    // But exact RTA passes!
    assert(rep.passes_exact_rta);
    assert(rep.worst_case_response_times[1] == 1);
    assert(rep.worst_case_response_times[2] == 3);
    assert(rep.worst_case_response_times[3] == 10);

    std::cout << "[PASS] Schedulability Analysis and RTA recurrence solver passed.\n";
}

int main() {
    std::cout << "--- Running Cerberus-RT Unit Test Suite ---\n";
    test_memory_pool();
    test_priority_inheritance();
    test_schedulability_analysis();
    std::cout << "--- ALL CERBERUS-RT UNIT TESTS PASSED ---\n";
    return 0;
}

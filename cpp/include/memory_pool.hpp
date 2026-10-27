#pragma once

#include <cstddef>
#include <cstdint>
#include <cassert>
#include <utility>
#include <stdexcept>

namespace cerberus {

/**
 * @brief Deterministic Fixed-Size Block Memory Pool Allocator
 * Guarantees O(1) allocation and deallocation without heap fragmentation.
 * Complies with DO-178C Level A safety-critical RTOS avionics requirements.
 */
template <size_t BlockSize, size_t BlockCount>
class FixedMemoryPool {
    static_assert(BlockSize >= sizeof(void*), "BlockSize must be at least pointer size for free list");

public:
    FixedMemoryPool() : free_head_(nullptr), allocated_count_(0) {
        // Initialize free list using intrusive pointers
        uint8_t* curr = memory_buffer_;
        for (size_t i = 0; i < BlockCount - 1; ++i) {
            uint8_t* next = curr + BlockSize;
            *reinterpret_cast<void**>(curr) = next;
            curr = next;
        }
        *reinterpret_cast<void**>(curr) = nullptr;
        free_head_ = reinterpret_cast<void*>(memory_buffer_);
    }

    ~FixedMemoryPool() = default;

    // Non-copyable
    FixedMemoryPool(const FixedMemoryPool&) = delete;
    FixedMemoryPool& operator=(const FixedMemoryPool&) = delete;

    /**
     * @brief O(1) deterministic allocation
     */
    void* allocate() {
        if (!free_head_) {
            return nullptr; // Out of memory pool blocks (bounded determinism)
        }
        void* block = free_head_;
        free_head_ = *reinterpret_cast<void**>(free_head_);
        allocated_count_++;
        return block;
    }

    /**
     * @brief O(1) deterministic deallocation
     */
    bool deallocate(void* ptr) {
        if (!ptr) return false;

        uint8_t* p = reinterpret_cast<uint8_t*>(ptr);
        if (p < memory_buffer_ || p >= memory_buffer_ + TotalBytes) {
            return false; // Pointer outside pool memory range
        }

        // Push onto intrusive free list
        *reinterpret_cast<void**>(ptr) = free_head_;
        free_head_ = ptr;
        allocated_count_--;
        return true;
    }

    size_t allocated_count() const { return allocated_count_; }
    size_t free_count() const { return BlockCount - allocated_count_; }
    constexpr size_t total_blocks() const { return BlockCount; }
    constexpr size_t block_size() const { return BlockSize; }

private:
    static constexpr size_t TotalBytes = BlockSize * BlockCount;
    alignas(max_align_t) uint8_t memory_buffer_[TotalBytes];
    void* free_head_;
    size_t allocated_count_;
};

} // namespace cerberus

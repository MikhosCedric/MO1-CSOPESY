#pragma once
#ifndef IMEMORY_ALLOCATOR_H
#define IMEMORY_ALLOCATOR_H

#include <string>
#include <cstddef>

// The seatwork interface uses "String"; alias it to std::string so the
// abstract contract compiles as-is across the project.
using String = std::string;

// ============================================================================
// IMemoryAllocator
// ----------------------------------------------------------------------------
// Abstract contract that every memory-manager backend must satisfy. This is the
// exact interface provided in the Week 11 seatwork, with two minimal fixes:
//   * the nested MemoryBlock had "size_t size:" (a typo) -> "size_t size;"
//   * a virtual destructor was added so backends can be owned polymorphically
//     (e.g. std::unique_ptr<IMemoryAllocator>) without leaking.
//
// Two backends are anticipated via MemoryAllocatorType:
//   FLAT_MEMORY_ALLOCATOR - contiguous allocation (first/best/worst-fit holes)
//   PAGING                - fixed-size frames + page tables (implemented here)
// ============================================================================
class IMemoryAllocator {
public:
    enum MemoryAllocatorType {
        FLAT_MEMORY_ALLOCATOR,
        PAGING,
    };

    virtual ~IMemoryAllocator() = default;

    // Reserve `size` bytes and return a handle (a virtual base address) to it.
    // Returns nullptr on failure (0 bytes requested, or no room can be made).
    virtual void* allocate(size_t size) = 0;

    // Release a block previously returned by allocate().
    virtual void deallocate(void* ptr) = 0;

    // Human-readable snapshot of the current memory state.
    virtual String visualizeMemory() = 0;

protected:
    MemoryAllocatorType memoryAllocatorType;

    // A contiguous span of memory [start, start + size). This is the natural
    // bookkeeping unit for the FLAT allocator (a "hole"); it is intentionally
    // NOT the core structure of the paging backend (see PagingAllocator).
    struct MemoryBlock {
        size_t start;
        size_t size;

        bool operator<(const MemoryBlock& other) const {
            return start < other.start;
        }
    };

    size_t maximumSize = 0;          // total bytes the allocator manages
    size_t currentAllocatedSize = 0; // bytes currently handed out
};

#endif // IMEMORY_ALLOCATOR_H

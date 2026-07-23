# MO1-CSOPESY — Memory Allocator Paging Implementation

**Branch:** `feature/paging-allocator`

---

## Files Created/Modified

| File | What |
|------|------|
| `IMemoryAllocator.h` | **NEW** — interface from the instructions |
| `PagingAllocator.h` | **NEW** — paging allocator header |
| `PagingAllocator.cpp` | **NEW** — allocate, deallocate, page replacement, backing store |
| `ConfigManager.h` / `.cpp` | Added `memory-size` and `page-size` config fields |
| `Process.h` / `.cpp` | Each process now gets its own `PagingAllocator` |
| `ConsoleManager.cpp` / `Scheduler.cpp` | Pass memory config when making processes |
| `config.txt` | Added `memory-size 65536`, `page-size 4096` |
| `MO1.vcxproj` | Included new files in build |

---

### 1. Show how your memory manager class can be implemented with an "IMemoryAllocator" interface that contains the following code above.

`PagingAllocator` inherits from `IMemoryAllocator` and overrides its three pure virtual functions: `allocate`, `deallocate`, and `visualizeMemory`. In the constructor we set `memoryAllocatorType = PAGING`, and we use `maximumSize` and `currentAllocatedSize` to keep track of total memory versus used memory. It's a straightforward interface implementation — any allocator (flat, paging, segmented) can plug into the same contract.

---

### 2. The memory manager class must specifically implement a paging implementation. Discuss how your paging implementation is implemented, with the allocate and deallocate functions.

**Allocate:** We first check edge cases (zero size or running out of memory). Then we figure out how many pages we need by rounding up: `numPages = ceil(size / pageSize)`. For each page, we look for a free physical frame. If we find one, great — we map the virtual page to that frame in the page table. If not, we evict a frame by saving its data to the backing store (`pageOut`, increments `numPagedOut`) and reuse that frame. If the page we're mapping was previously evicted, we record a page-in event (`pageIn`, increments `numPagedIn`). Finally, we return a virtual address computed as `virtualPageNumber * pageSize`.

**Deallocate:** We cast the pointer to a number to get the virtual address, divide by page size to get the VPN, look up the page table entry, grab the frame number, mark the frame as free, and invalidate the PTE. The backing store is untouched — the page just gets evicted naturally later if a new allocation needs the frame.

The page table is a flat `vector<PageTableEntry>` indexed by VPN. Each entry stores `valid`, `frameNumber`, `dirty`, and `referenced` flags. Physical frames are tracked with a `vector<bool>` bitmap.

---

### 3. What is the role of the MemoryBlock, and would this be wise to use in a paging environment? Yes/no, then provide an explanation and alternative implementation, if applicable.

`MemoryBlock` is a struct with `start` and `size` — it's meant for tracking variable-size chunks of memory, like you'd see in a flat allocator or `malloc`'s free list. You'd use it to keep a list of free holes sorted by address, splitting and merging as you allocate and free.

**No**, it's not wise to use in a paging environment. In paging, everything is fixed-size pages, so there's no need to track variable-size blocks. Instead of `MemoryBlock`, we use a **frame bitmap** (`frameFree[i]` says whether frame `i` is free) and a **page table** (`pageTable[VPN]` maps to a frame). Bitmaps are simpler, faster, and more natural for paging than block lists. We never actually instantiate `MemoryBlock` in our code — it lives in the interface but goes unused.

---

### 4. Specifically, point to where exactly in your memory manager class the backing store implementation will be implemented, and justify why.

The backing store is `std::map<uint32_t, std::vector<char>> backingStore` in `PagingAllocator.h:34`. The key is `(processId << 16) | virtualPageNumber`, which keeps each process's evicted pages separate.

It's written to in `pageOut()` (called during eviction when no free frames exist) and checked in `pageIn()` (called when we map a page that might have been previously evicted). It belongs inside the allocator because the allocator owns the page life cycle — it decides when to evict, when to bring back, and when to free. No other part of the emulator needs to touch the backing store, so keeping it private makes sense.

---

### 5. Given a "void* ptr" reference, how does your memory manager map this pointer back to its corresponding Page Table Entry (PTE) and physical frame to successfully free the memory?

In `deallocate`, we do two things:
1. **Virtual address → VPN:** `virtualPage = (size_t)ptr / pageSize`
2. **VPN → PTE → frame:** `pageTable[virtualPage]` gives us the PTE, we read `frameNumber` from it, free that frame, and invalidate the PTE.

This is classic paging address translation — the virtual address is split into a page number (used to index the page table) and an offset (byte within the page). Our addresses are always page-aligned so the offset is always zero.

---

### 6. Explain how internal fragmentation occurs in a paging environment.

When you request memory, the allocator rounds up to the nearest page boundary. So if you ask for 5000 bytes with 4096-byte pages, you get 2 pages (8192 bytes). The 3192 bytes you didn't need are wasted inside the last page — that's internal fragmentation.

It's called "internal" because the waste is inside the allocated region (as opposed to external fragmentation, where gaps form between allocations). The fix would be smaller pages, but that means bigger page tables and more page faults. It's a trade-off.

---

### 7. Describe where in your code, will num-paged-in and num-paged-out be tallied.

Both are private `int` members in `PagingAllocator.h:36-37`, starting at zero.

- **numPagedOut** increments in `pageOut()` (`PagingAllocator.cpp:163`) — every time a frame is evicted to the backing store
- **numPagedIn** increments in `pageIn()` (`PagingAllocator.cpp:154`) — every time a previously-evicted page is brought back

They're printed by `visualizeMemory()` and exposed via `getNumPagedIn()` / `getNumPagedOut()`.

---

### 8. In your OS emulator, the page table is likely implemented as a simple flat array or a single-level lookup map. Research and explain why the Linux kernel uses a multi-level structure instead of a single flat page table array for a 64-bit architecture. Go to the Linux kernel on GitHub and point exactly to which code, lines of code, depict the multi-page implementation.

A flat page table for 64-bit x86 would need 2^52 entries (4.5 quadrillion) per process — that's 36 petabytes. Obviously impossible.

Linux uses a **multi-level hierarchy** where the virtual address is split into indices: PGD → PUD → PMD → PTE, each indexing a small 512-entry table. Only the levels covering used address space are actually allocated, so a process using a few pages only needs about 16 KB of page tables instead of 36 PB. It also lets processes share entire subtrees (like kernel space).

**Where to find it in the Linux kernel:**
- `arch/x86/include/asm/pgtable_types.h` — defines `PAGE_SHIFT` (12), `PMD_SHIFT` (21), `PUD_SHIFT` (30), `PGDIR_SHIFT` (39) that encode the bit split
- `arch/x86/include/asm/pgtable.h` — traversal functions like `pgd_offset()`, `pud_offset()`, `pmd_offset()`, `pte_offset_kernel()`

---

### 9. Identify/enumerate codes from the Linux kernel that resemble the MemoryBlock implementation from the OS emulator. Explain the codes you selected from the Linux kernel, and why they are related to the MemoryBlock implementation.

`MemoryBlock` is `{start, size}` — a contiguous region. Linux has several of these:

- **`struct memblock_region`** (`include/linux/memblock.h`) — `{base, size}`, used during boot to track physical memory banks. Closest match.
- **`struct vm_area_struct`** (`include/linux/mm_types.h`) — `{vm_start, vm_end}`, describes virtual memory areas like heap and stack. Size = `vm_end - vm_start`.
- **`struct resource`** (`include/linux/ioport.h`) — `{start, end}`, used for device I/O and MMIO regions.

All are fundamentally the same idea: a pair of numbers answering "where does this region start and how big is it?"

---

### 10. Check the following function from the Linux kernel and explain what it does: static int walk_pud_range(p4d_t p4d, unsigned long addr, unsigned long end, struct mm_walk *walk)

`walk_pud_range` lives in `mm/pagewalk.c` and is part of the kernel's generic page table walker. It iterates over every PUD entry covering the address range `[addr, end)`. For each entry that's present, it descends into the next level by calling `walk_pmd_range()`. For gaps (unmapped regions), it fires a callback to notify the walker.

The full chain is: `walk_page_range` → `walk_p4d_range` → `walk_pud_range` → `walk_pmd_range` → `walk_pte_range`.

It's used by `/proc/pid/smaps` generation, page migration, memory compaction, and other subsystems that need to inspect page tables. In our emulator, walking the page table is just a `for` loop over one array — Linux needs this recursive descent because its page table has multiple levels.

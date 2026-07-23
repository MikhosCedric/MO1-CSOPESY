#pragma once
#ifndef BACKING_STORE_H
#define BACKING_STORE_H

#include <string>
#include <vector>
#include <cstdint>
#include <cstddef>

// ============================================================================
// BackingStore
// ----------------------------------------------------------------------------
// Simulates secondary storage (the swap area / "backing store") as a directory
// of plain-text files, one per paged-out page. When physical memory is full and
// the paging allocator must evict a page, the page's bytes are written here
// (page-out); when a swapped page is referenced again, its bytes are read back
// (page-in). This mirrors the notes: "Possible Implementation: Text files saved
// in a certain directory ... done to free main memory usage."
//
// Each page is addressed by a stable, unique `pageId` assigned by the allocator.
// ============================================================================
class BackingStore {
public:
    explicit BackingStore(const std::string& directory);

    // Persist one page's worth of bytes to disk under `pageId` (page-out target).
    void store(size_t pageId, const std::vector<uint8_t>& data);

    // Read a previously stored page back into memory (page-in source).
    // Returns an empty vector if the page is not present on disk.
    std::vector<uint8_t> load(size_t pageId);

    // Drop a page from the store (called when its owning block is freed).
    void remove(size_t pageId);

    // True if the given page currently resides in the backing store.
    bool contains(size_t pageId) const;

    // Wipe the whole store (used on shutdown / reset).
    void clear();

private:
    std::string dir;
    std::string pathFor(size_t pageId) const;
};

#endif // BACKING_STORE_H

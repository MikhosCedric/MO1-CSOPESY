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
// A page is addressed by the pair (process id, virtual page number), so a page
// belongs to a named process rather than to an anonymous allocation.
//
// Phase 4 replaces this directory with the single csopesy-backing-store.txt the
// spec requires; the (pid, vpn) key is what that file's records are built on.
// ============================================================================
class BackingStore {
public:
    explicit BackingStore(const std::string& directory);

    // Persist one page's worth of bytes to disk (page-out target).
    void store(uint32_t pid, uint32_t vpn, const std::vector<uint8_t>& data);

    // Read a previously stored page back into memory (page-in source).
    // Returns an empty vector if the page is not present on disk.
    std::vector<uint8_t> load(uint32_t pid, uint32_t vpn) const;

    // Drop a page from the store (called when its owning process is freed).
    void remove(uint32_t pid, uint32_t vpn);

    // True if the given page currently resides in the backing store.
    bool contains(uint32_t pid, uint32_t vpn) const;

    // Wipe the whole store (used on startup / reset).
    void clear();

private:
    std::string dir;
    std::string pathFor(uint32_t pid, uint32_t vpn) const;
};

#endif // BACKING_STORE_H

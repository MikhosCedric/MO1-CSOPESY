#pragma once
#ifndef BACKING_STORE_H
#define BACKING_STORE_H

#include <cstdint>
#include <map>
#include <string>
#include <utility>
#include <vector>

// ============================================================================
// BackingStore
// ----------------------------------------------------------------------------
// Secondary storage for demand paging, projected onto the single plain-text
// file the spec requires - csopesy-backing-store.txt - which is readable at any
// point during a run.
//
// The file is an append-only EVENT LOG: one line per page movement, in the
// format the course sample shows -
//
//   Process 2 page 1 paged in to backing store at Tue Aug  5 22:09:05 2025
//   Process 2 page 1 evicted to backing store at Tue Aug  5 22:09:06 2025
//
// The pages themselves live in memory and the file records what happened to
// them, for two reasons:
//   * Correctness is unaffected - page-in reads the same bytes either way, and
//     it drops per-page file I/O from the fault path entirely.
//   * Re-rendering the whole store inside each individual page-out is
//     quadratic. A 20-second batch run holds ~10,000 swapped-out pages across
//     ~200 live processes, so a rewrite per byte-array write means tens of
//     millions of lines and the emulator grinds to a halt.
// Events are buffered and flushIfDirty() appends them. The scheduler calls it
// once per tick, so the log is at most one tick behind and the destructor
// appends a final time on shutdown.
// ============================================================================
class BackingStore {
public:
    explicit BackingStore(const std::string& path);
    ~BackingStore();

    // Register a process and put all of its (zero-filled) pages in the store.
    // A new process starts with every page swapped out and marked invalid.
    void addProcess(uint32_t pid, const std::string& name, uint32_t memorySize,
                    uint32_t numPages, uint32_t pageSize);
    void removeProcess(uint32_t pid);

    // Record the line the process is currently on.
    void setCommandCounter(uint32_t pid, uint32_t line);

    void store(uint32_t pid, uint32_t vpn, const std::vector<uint8_t>& data);
    std::vector<uint8_t> load(uint32_t pid, uint32_t vpn) const;
    void remove(uint32_t pid, uint32_t vpn);

    // Pages currently swapped out, across every process.
    size_t getPageCount() const { return pages.size(); }

    // Append any events recorded since the last call.
    void flushIfDirty();

    // Drop everything and truncate the log (startup / reset).
    void clear();

private:
    struct Record {
        std::string name;
        uint32_t memorySize = 0;
        uint32_t numPages = 0;
        uint32_t commandCounter = 0;
    };

    // "Process <pid> page <vpn> <what> to backing store at <timestamp>"
    void logEvent(uint32_t pid, uint32_t vpn, const char* what);
    void appendEvents();

    std::string path;
    std::map<uint32_t, Record> procs;                                   // pid -> info
    std::map<std::pair<uint32_t, uint32_t>, std::vector<uint8_t>> pages; // (pid, vpn) -> bytes

    std::vector<std::string> pendingEvents; // written by the next appendEvents()
};

#endif // BACKING_STORE_H

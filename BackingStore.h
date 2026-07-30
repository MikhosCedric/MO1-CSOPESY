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
// A record carries the process info the notes call for (id, name, memory size,
// page count, and the command counter - the current instruction line, which is
// what makes a swapped-out process resumable) plus every page of that process
// currently swapped out, and that page's bytes.
//
// The authoritative state is in memory and the file is a rendering of it, for
// two reasons:
//   * Correctness is unaffected - page-in reads the same bytes either way, and
//     it drops per-page file I/O from the fault path entirely.
//   * Rewriting the file inside each individual page-out is quadratic. A
//     20-second batch run holds ~10,000 swapped-out pages across ~200 live
//     processes, so a rewrite per byte-array write means tens of millions of
//     lines and the emulator grinds to a halt.
// A mutation marks the store dirty and flushIfDirty() rewrites the file. The
// scheduler calls it once per tick, so the file is at most one tick behind and
// the destructor writes a final time on shutdown.
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

    // Rewrite the file if anything has changed since the last rewrite.
    void flushIfDirty();

    // Drop everything and truncate the file (startup / reset).
    void clear();

private:
    struct Record {
        std::string name;
        uint32_t memorySize = 0;
        uint32_t numPages = 0;
        uint32_t commandCounter = 0;
    };

    void writeFile() const;

    std::string path;
    std::map<uint32_t, Record> procs;                                   // pid -> info
    std::map<std::pair<uint32_t, uint32_t>, std::vector<uint8_t>> pages; // (pid, vpn) -> bytes

    bool dirty = false;
};

#endif // BACKING_STORE_H

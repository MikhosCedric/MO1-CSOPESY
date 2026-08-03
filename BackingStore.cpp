#include "BackingStore.h"

#include <algorithm>
#include <ctime>
#include <fstream>

// "Tue Aug  5 22:09:05 2025" - the format the course sample logs events in.
static std::string eventTimestamp() {
    const std::time_t now = std::time(nullptr);
    std::string s = std::ctime(&now);
    if (!s.empty() && s.back() == '\n') s.pop_back();
    return s;
}

BackingStore::BackingStore(const std::string& path)
    : path(path)
{
}

BackingStore::~BackingStore() {
    // Do not lose the last tick's events on shutdown.
    appendEvents();
}

void BackingStore::addProcess(uint32_t pid, const std::string& name, uint32_t memorySize,
                              uint32_t numPages, uint32_t pageSize) {
    Record rec;
    rec.name = name;
    rec.memorySize = memorySize;
    rec.numPages = numPages;
    procs[pid] = rec;

    // Every page starts swapped out and zero-filled.
    const std::vector<uint8_t> blank(pageSize, 0);
    for (uint32_t vpn = 0; vpn < numPages; ++vpn) {
        pages[{ pid, vpn }] = blank;
    }
}

void BackingStore::removeProcess(uint32_t pid) {
    procs.erase(pid);

    for (auto it = pages.begin(); it != pages.end(); ) {
        it = (it->first.first == pid) ? pages.erase(it) : std::next(it);
    }
}

void BackingStore::setCommandCounter(uint32_t pid, uint32_t line) {
    auto it = procs.find(pid);
    if (it == procs.end()) return;
    it->second.commandCounter = line;
}

void BackingStore::store(uint32_t pid, uint32_t vpn, const std::vector<uint8_t>& data) {
    pages[{ pid, vpn }] = data;
    logEvent(pid, vpn, "evicted");
}

std::vector<uint8_t> BackingStore::load(uint32_t pid, uint32_t vpn) const {
    auto it = pages.find({ pid, vpn });
    if (it == pages.end()) return {};
    return it->second;
}

void BackingStore::remove(uint32_t pid, uint32_t vpn) {
    if (pages.erase({ pid, vpn }) > 0) logEvent(pid, vpn, "paged in");
}

void BackingStore::clear() {
    procs.clear();
    pages.clear();
    pendingEvents.clear();

    // Start each run with an empty log rather than appending to the last one's.
    std::ofstream out(path, std::ios::trunc);
}

void BackingStore::flushIfDirty() {
    appendEvents();
}

void BackingStore::logEvent(uint32_t pid, uint32_t vpn, const char* what) {
    pendingEvents.push_back("Process " + std::to_string(pid)
                            + " page " + std::to_string(vpn)
                            + " " + what + " to backing store at "
                            + eventTimestamp());
}

void BackingStore::appendEvents() {
    if (pendingEvents.empty()) return;

    std::ofstream out(path, std::ios::app);
    if (out) {
        for (const std::string& e : pendingEvents) {
            out << e << "\n";
        }
        out.flush();
    }

    // Dropped either way: a log that cannot be opened must not grow without
    // bound for the rest of the run.
    pendingEvents.clear();
}

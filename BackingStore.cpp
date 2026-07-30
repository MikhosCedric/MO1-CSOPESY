#include "BackingStore.h"

#include <algorithm>
#include <fstream>

BackingStore::BackingStore(const std::string& path)
    : path(path)
{
}

BackingStore::~BackingStore() {
    // Leave the file reflecting the final state of the run.
    writeFile();
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
    dirty = true;
}

void BackingStore::removeProcess(uint32_t pid) {
    procs.erase(pid);

    for (auto it = pages.begin(); it != pages.end(); ) {
        it = (it->first.first == pid) ? pages.erase(it) : std::next(it);
    }
    dirty = true;
}

void BackingStore::setCommandCounter(uint32_t pid, uint32_t line) {
    auto it = procs.find(pid);
    if (it == procs.end() || it->second.commandCounter == line) return;
    it->second.commandCounter = line;
    dirty = true;
}

void BackingStore::store(uint32_t pid, uint32_t vpn, const std::vector<uint8_t>& data) {
    pages[{ pid, vpn }] = data;
    dirty = true;
}

std::vector<uint8_t> BackingStore::load(uint32_t pid, uint32_t vpn) const {
    auto it = pages.find({ pid, vpn });
    if (it == pages.end()) return {};
    return it->second;
}

void BackingStore::remove(uint32_t pid, uint32_t vpn) {
    if (pages.erase({ pid, vpn }) > 0) dirty = true;
}

void BackingStore::clear() {
    procs.clear();
    pages.clear();
    dirty = false;
    writeFile();
}

void BackingStore::flushIfDirty() {
    if (!dirty) return;
    dirty = false;
    writeFile();
}

void BackingStore::writeFile() const {
    std::ofstream out(path, std::ios::trunc);
    if (!out) return;

    out << "CSOPESY BACKING STORE\n";
    out << "Processes: " << procs.size() << "\n";
    out << "Pages swapped out: " << pages.size() << "\n";
    out << "A page listed as <zeroed> holds nothing but zero bytes.\n";

    for (const auto& entry : procs) {
        const uint32_t pid = entry.first;
        const Record& rec = entry.second;

        out << "\n[process"
            << " id=" << pid
            << " name=" << rec.name
            << " memory-size=" << rec.memorySize
            << " num-pages=" << rec.numPages
            << " command-counter=" << rec.commandCounter
            << "]\n";

        // Pages of this process, in page order (the map is keyed (pid, vpn)).
        for (auto it = pages.lower_bound({ pid, 0 });
             it != pages.end() && it->first.first == pid; ++it) {
            const std::vector<uint8_t>& bytes = it->second;

            out << "page " << it->first.second << ": ";
            if (std::all_of(bytes.begin(), bytes.end(),
                            [](uint8_t b) { return b == 0; })) {
                out << "<zeroed>\n";
                continue;
            }
            for (size_t i = 0; i < bytes.size(); ++i) {
                out << static_cast<int>(bytes[i]);
                out << ((i + 1 < bytes.size()) ? ' ' : '\n');
            }
        }
    }

    out.flush();
}

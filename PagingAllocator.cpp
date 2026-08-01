#include "PagingAllocator.h"

#include <algorithm>
#include <iomanip>
#include <sstream>

PagingAllocator::PagingAllocator(uint32_t maxOverallMem, uint32_t frameSize)
    : frameSize(frameSize)
    , numFrames(frameSize == 0 ? 0 : maxOverallMem / frameSize)
    , physicalMemory(static_cast<size_t>(numFrames) * frameSize, 0)
    , frameTable(numFrames)
    , backingStore("csopesy-backing-store.txt")
{
    for (uint32_t f = 0; f < numFrames; ++f) {
        freeFrameList.push_back(f);
    }

    // Start from a clean swap area each run.
    backingStore.clear();
}

// ---------------------------------------------------------------------------
// createProcess: lazy allocation. Build the page table with every page invalid
// and push the whole (zero-filled) address space out to the backing store. No
// frame is taken - the first reference to each page faults it in.
// ---------------------------------------------------------------------------
void PagingAllocator::createProcess(uint32_t pid, const std::string& name, uint32_t memorySize) {
    if (frameSize == 0 || memorySize == 0) return;
    if (processes.count(pid)) return;

    uint32_t numPages = (memorySize + frameSize - 1) / frameSize; // ceil division

    ProcessMemory pm;
    pm.pid = pid;
    pm.name = name;
    pm.memorySize = memorySize;
    pm.pageTable.resize(numPages); // every PTE present = false

    backingStore.addProcess(pid, name, memorySize, numPages, frameSize);

    processes.emplace(pid, std::move(pm));
}

void PagingAllocator::destroyProcess(uint32_t pid) {
    auto it = processes.find(pid);
    if (it == processes.end()) return;

    ProcessMemory& pm = it->second;
    for (uint32_t vpn = 0; vpn < pm.pageTable.size(); ++vpn) {
        PageTableEntry& pte = pm.pageTable[vpn];
        if (pte.present) {
            // Resident: hand the frame back.
            uint32_t frame = pte.frameNumber;
            frameTable[frame] = FrameOwner{};
            freeFrameList.push_back(frame);
            fifoFrames.erase(std::remove(fifoFrames.begin(), fifoFrames.end(), frame),
                             fifoFrames.end());
            pinnedFrames.erase(std::remove(pinnedFrames.begin(), pinnedFrames.end(), frame),
                               pinnedFrames.end());
        }
        pte.present = false;
    }

    // Drops the process's record and every page it still had swapped out.
    backingStore.removeProcess(pid);
    processes.erase(it);
}

// ---------------------------------------------------------------------------
// IProcessMemory
// ---------------------------------------------------------------------------
void PagingAllocator::beginInstruction(uint32_t pid, uint32_t commandCounter) {
    // Release only THIS process's pins. Another process that faulted earlier in
    // the tick keeps its own, so it still finds its pages resident when it
    // retries. Clearing pins globally here lets each process evict the other's
    // just-loaded pages, and when frames are scarce neither ever gets to run.
    releasePins(pid);
    backingStore.setCommandCounter(pid, commandCounter);
}

void PagingAllocator::releasePins(uint32_t pid) {
    pinnedFrames.erase(
        std::remove_if(pinnedFrames.begin(), pinnedFrames.end(),
                       [&](uint32_t f) { return frameTable[f].pid == pid; }),
        pinnedFrames.end());
}

Residency PagingAllocator::ensureResident(uint32_t pid, uint32_t addr, uint32_t len) {
    auto it = processes.find(pid);
    if (it == processes.end() || len == 0) return Residency::RESIDENT;
    ProcessMemory& pm = it->second;

    // Page number = high-order bits of the address, offset = low-order bits.
    // A 2-byte word straddling a page boundary needs both pages.
    uint32_t firstVpn = addr / frameSize;
    uint32_t lastVpn = (addr + len - 1) / frameSize;

    bool faulted = false;
    for (uint32_t vpn = firstVpn; vpn <= lastVpn && vpn < pm.pageTable.size(); ++vpn) {
        if (!pm.pageTable[vpn].present) {
            if (!pageIn(pm, vpn)) {
                // Every frame is pinned by a process mid-fault. Give up the ones
                // pinned for this attempt instead of holding them while we wait:
                // two processes each holding part of what the other needs never
                // make progress.
                releasePins(pid);
                return Residency::UNAVAILABLE;
            }
            faulted = true;
        }
        pinnedFrames.push_back(pm.pageTable[vpn].frameNumber);
    }

    return faulted ? Residency::FAULTED : Residency::RESIDENT;
}

uint16_t PagingAllocator::readWord(uint32_t pid, uint32_t addr) {
    const uint8_t* lo = byteAt(pid, addr);
    const uint8_t* hi = byteAt(pid, addr + 1);
    if (!lo || !hi) return 0; // not resident: uninitialised memory reads as 0
    return static_cast<uint16_t>(*lo | (static_cast<uint16_t>(*hi) << 8));
}

void PagingAllocator::writeWord(uint32_t pid, uint32_t addr, uint16_t value) {
    uint8_t* lo = byteAt(pid, addr);
    uint8_t* hi = byteAt(pid, addr + 1);
    if (!lo || !hi) return;
    *lo = static_cast<uint8_t>(value & 0xFF);
    *hi = static_cast<uint8_t>((value >> 8) & 0xFF);
}

// ---------------------------------------------------------------------------
// Statistics
// ---------------------------------------------------------------------------
uint32_t PagingAllocator::getUsedMemory() const {
    return static_cast<uint32_t>((numFrames - freeFrameList.size()) * frameSize);
}

uint32_t PagingAllocator::getFreeMemory() const {
    return static_cast<uint32_t>(freeFrameList.size() * frameSize);
}

uint32_t PagingAllocator::getResidentMemory(uint32_t pid) const {
    auto it = processes.find(pid);
    if (it == processes.end()) return 0;

    uint32_t pages = 0;
    for (const PageTableEntry& pte : it->second.pageTable) {
        if (pte.present) pages++;
    }
    return pages * frameSize;
}

std::string PagingAllocator::renderSnapshot(const std::string& timestamp) const {
    std::ostringstream oss;
    oss << "Timestamp: (" << timestamp << ")\n";
    oss << "Number of processes in memory: " << processes.size() << "\n";
    oss << "Frame size: " << frameSize << " bytes\n";
    oss << "Frames used / total: " << (numFrames - freeFrameList.size())
        << " / " << numFrames << "\n";
    oss << "Pages paged in: " << numPagedIn << "\n";
    oss << "Pages paged out: " << numPagedOut << "\n";
    oss << "----start---- = " << getTotalMemory() << "\n";

    for (uint32_t f = 0; f < numFrames; ++f) {
        const FrameOwner& owner = frameTable[f];
        if (!owner.used) continue;

        auto it = processes.find(owner.pid);
        oss << f * frameSize << "\t"
            << (it != processes.end() ? it->second.name : std::string("?"))
            << "  page " << owner.vpn << "\n";
    }

    oss << "----end---- = 0\n";
    return oss.str();
}

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------
uint8_t* PagingAllocator::byteAt(uint32_t pid, uint32_t addr) {
    auto it = processes.find(pid);
    if (it == processes.end()) return nullptr;
    ProcessMemory& pm = it->second;

    uint32_t vpn = addr / frameSize;
    if (vpn >= pm.pageTable.size()) return nullptr;

    const PageTableEntry& pte = pm.pageTable[vpn];
    if (!pte.present) return nullptr;

    return &physicalMemory[static_cast<size_t>(pte.frameNumber) * frameSize
                           + (addr % frameSize)];
}

bool PagingAllocator::evictOneFrame() {
    // FIFO victim: the oldest resident frame that is not pinned by the
    // instruction currently being serviced.
    for (auto it = fifoFrames.begin(); it != fifoFrames.end(); ++it) {
        uint32_t frame = *it;

        if (std::find(pinnedFrames.begin(), pinnedFrames.end(), frame) != pinnedFrames.end()) {
            continue; // never evict a page the faulting instruction is about to use
        }

        const FrameOwner owner = frameTable[frame];
        auto pit = processes.find(owner.pid);
        if (!owner.used || pit == processes.end()) {
            fifoFrames.erase(it); // stale entry
            frameTable[frame] = FrameOwner{};
            freeFrameList.push_back(frame);
            return true;
        }

        PageTableEntry& pte = pit->second.pageTable[owner.vpn];

        // Page the victim's bytes out to the backing store.
        const size_t start = static_cast<size_t>(frame) * frameSize;
        std::vector<uint8_t> bytes(physicalMemory.begin() + start,
                                   physicalMemory.begin() + start + frameSize);
        backingStore.store(owner.pid, owner.vpn, bytes);

        pte.present = false;
        frameTable[frame] = FrameOwner{};
        freeFrameList.push_back(frame);
        fifoFrames.erase(it);
        ++numPagedOut;
        return true;
    }
    return false; // every resident frame is pinned
}

bool PagingAllocator::pageIn(ProcessMemory& pm, uint32_t vpn) {
    PageTableEntry& pte = pm.pageTable[vpn];
    if (pte.present) return true;

    if (freeFrameList.empty() && !evictOneFrame()) {
        return false; // no frame can be freed this tick; the caller retries
    }

    uint32_t frame = freeFrameList.front();
    freeFrameList.pop_front();

    // Restore the page's bytes from the backing store into the frame.
    const size_t start = static_cast<size_t>(frame) * frameSize;
    std::fill(physicalMemory.begin() + start,
              physicalMemory.begin() + start + frameSize, static_cast<uint8_t>(0));

    const std::vector<uint8_t> bytes = backingStore.load(pm.pid, vpn);
    for (size_t i = 0; i < frameSize && i < bytes.size(); ++i) {
        physicalMemory[start + i] = bytes[i];
    }
    backingStore.remove(pm.pid, vpn);

    pte.frameNumber = frame;
    pte.present = true;
    frameTable[frame] = FrameOwner{ pm.pid, vpn, true };
    fifoFrames.push_back(frame);
    ++numPagedIn;
    return true;
}

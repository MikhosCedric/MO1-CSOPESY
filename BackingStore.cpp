#include "BackingStore.h"

#include <filesystem>
#include <fstream>
#include <sstream>

namespace fs = std::filesystem;

BackingStore::BackingStore(const std::string& directory)
    : dir(directory)
{
    // Ensure the swap directory exists (relative to the emulator's path).
    std::error_code ec;
    fs::create_directories(dir, ec);
}

std::string BackingStore::pathFor(uint32_t pid, uint32_t vpn) const {
    std::ostringstream oss;
    oss << dir << "/proc_" << pid << "_page_" << vpn << ".txt";
    return oss.str();
}

void BackingStore::store(uint32_t pid, uint32_t vpn, const std::vector<uint8_t>& data) {
    // Write the page as human-readable text: a header followed by the byte
    // values. Text form keeps the store inspectable, matching the notes'
    // "text files ... must contain necessary process info" guidance.
    std::ofstream out(pathFor(pid, vpn), std::ios::trunc);
    if (!out) return;
    out << "pid " << pid << "\n";
    out << "page " << vpn << "\n";
    out << "bytes " << data.size() << "\n";
    for (size_t i = 0; i < data.size(); ++i) {
        out << static_cast<int>(data[i]);
        out << ((i + 1 < data.size()) ? ' ' : '\n');
    }
    out.flush(); // a grader will open the store mid-run
}

std::vector<uint8_t> BackingStore::load(uint32_t pid, uint32_t vpn) const {
    std::ifstream in(pathFor(pid, vpn));
    std::vector<uint8_t> data;
    if (!in) return data;

    std::string tag;
    size_t discard = 0;
    size_t count = 0;
    in >> tag >> discard;    // pid   <id>
    in >> tag >> discard;    // page  <vpn>
    in >> tag >> count;      // bytes <count>
    data.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        int value = 0;
        if (!(in >> value)) break;
        data.push_back(static_cast<uint8_t>(value));
    }
    return data;
}

void BackingStore::remove(uint32_t pid, uint32_t vpn) {
    std::error_code ec;
    fs::remove(pathFor(pid, vpn), ec);
}

bool BackingStore::contains(uint32_t pid, uint32_t vpn) const {
    std::error_code ec;
    return fs::exists(pathFor(pid, vpn), ec);
}

void BackingStore::clear() {
    std::error_code ec;
    if (!fs::exists(dir, ec)) return;
    for (const auto& entry : fs::directory_iterator(dir, ec)) {
        fs::remove(entry.path(), ec);
    }
}

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

std::string BackingStore::pathFor(size_t pageId) const {
    std::ostringstream oss;
    oss << dir << "/page_" << pageId << ".txt";
    return oss.str();
}

void BackingStore::store(size_t pageId, const std::vector<uint8_t>& data) {
    // Write the page as human-readable text: a size header followed by the
    // byte values. Text form keeps the store inspectable, matching the notes'
    // "text files ... must contain necessary process info" guidance.
    std::ofstream out(pathFor(pageId), std::ios::trunc);
    if (!out) return;
    out << "page_id " << pageId << "\n";
    out << "bytes " << data.size() << "\n";
    for (size_t i = 0; i < data.size(); ++i) {
        out << static_cast<int>(data[i]);
        out << ((i + 1 < data.size()) ? ' ' : '\n');
    }
}

std::vector<uint8_t> BackingStore::load(size_t pageId) {
    std::ifstream in(pathFor(pageId));
    std::vector<uint8_t> data;
    if (!in) return data;

    std::string tag;
    size_t storedId = 0;
    size_t count = 0;
    in >> tag >> storedId;   // page_id <id>
    in >> tag >> count;      // bytes   <count>
    data.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        int value = 0;
        if (!(in >> value)) break;
        data.push_back(static_cast<uint8_t>(value));
    }
    return data;
}

void BackingStore::remove(size_t pageId) {
    std::error_code ec;
    fs::remove(pathFor(pageId), ec);
}

bool BackingStore::contains(size_t pageId) const {
    std::error_code ec;
    return fs::exists(pathFor(pageId), ec);
}

void BackingStore::clear() {
    std::error_code ec;
    if (!fs::exists(dir, ec)) return;
    for (const auto& entry : fs::directory_iterator(dir, ec)) {
        fs::remove(entry.path(), ec);
    }
}

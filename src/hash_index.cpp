#include "../include/hash_index.h"

namespace bitcask {

void HashIndex::put(const std::string& key, const IndexEntry& entry) {
    index_[key] = entry;
}

std::optional<IndexEntry> HashIndex::get(const std::string& key) const {
    auto it = index_.find(key);
    if (it == index_.end()) {
        return std::nullopt;
    }
    
    // Don't return tombstones
    if (it->second.is_tombstone()) {
        return std::nullopt;
    }
    
    return it->second;
}

void HashIndex::remove(const std::string& key, uint32_t timestamp) {
    index_[key] = IndexEntry::create_tombstone(timestamp);
}

bool HashIndex::contains(const std::string& key) const {
    auto entry = get(key);
    return entry.has_value();
}

std::vector<std::string> HashIndex::keys() const {
    std::vector<std::string> result;
    result.reserve(index_.size());
    
    for (const auto& [key, entry] : index_) {
        if (!entry.is_tombstone()) {
            result.push_back(key);
        }
    }
    
    return result;
}

size_t HashIndex::size() const {
    size_t count = 0;
    for (const auto& [key, entry] : index_) {
        if (!entry.is_tombstone()) {
            count++;
        }
    }
    return count;
}

void HashIndex::clear() {
    index_.clear();
}

std::vector<HashIndex::HintEntry> HashIndex::export_hints() const {
    std::vector<HintEntry> hints;
    hints.reserve(index_.size());
    
    for (const auto& [key, entry] : index_) {
        if (!entry.is_tombstone()) {
            hints.push_back({key, entry});
        }
    }
    
    return hints;
}

} // namespace bitcask
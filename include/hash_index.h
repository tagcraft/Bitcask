#ifndef BITCASK_HASH_INDEX_H
#define BITCASK_HASH_INDEX_H

#include "types.h"
#include <unordered_map>
#include <string>
#include <optional>
#include <vector>

namespace bitcask {

// In-memory hash index mapping keys to log file positions
class HashIndex {
public:
    HashIndex() = default;
    
    // Insert or update a key in the index
    void put(const std::string& key, const IndexEntry& entry);
    
    // Get index entry for a key
    std::optional<IndexEntry> get(const std::string& key) const;
    
    // Remove a key (mark as tombstone)
    void remove(const std::string& key, uint32_t timestamp);
    
    // Check if key exists and is not deleted
    bool contains(const std::string& key) const;
    
    // Get all keys (for merge operations)
    std::vector<std::string> keys() const;
    
    // Get number of keys (excluding tombstones)
    size_t size() const;
    
    // Clear the index
    void clear();
    
    // Export index to hint file format
    struct HintEntry {
        std::string key;
        IndexEntry entry;
    };
    std::vector<HintEntry> export_hints() const;
    
private:
    std::unordered_map<std::string, IndexEntry> index_;
};

} // namespace bitcask

#endif // BITCASK_HASH_INDEX_H
#ifndef BITCASK_H
#define BITCASK_H

#include "types.h"
#include "log_file.h"
#include "hash_index.h"
#include <memory>
#include <vector>
#include <string>

namespace bitcask {

// Main Bitcask database class
class Bitcask {
public:
    // Open or create a Bitcask database
    static Result<std::unique_ptr<Bitcask>> open(const Config& config);
    
    // Destructor ensures proper cleanup
    ~Bitcask();
    
    // Put a key-value pair
    Result<void> put(const std::string& key, const std::string& value);
    
    // Get a value by key
    Result<std::string> get(const std::string& key);
    
    // Delete a key
    Result<void> del(const std::string& key);
    
    // List all keys
    std::vector<std::string> list_keys();
    
    // Merge (compact) log files
    Result<void> merge();
    
    // Sync active file to disk
    Result<void> sync();
    
private:
    Bitcask(const Config& config);
    
    Config config_;
    HashIndex index_;
    std::vector<std::unique_ptr<LogFile>> old_files_;  // Immutable files
    std::unique_ptr<LogFile> active_file_;             // Current writable file
    uint32_t next_file_id_;
    
    // Initialize database (create directory, load existing data)
    Result<void> initialize();
    
    // Load existing log files and rebuild index
    Result<void> load_existing_files();
    
    // Create a new active file
    Result<void> rotate_active_file();
    
    // Get current timestamp
    uint32_t get_timestamp() const;
    
    // Write hint file for a log file
    Result<void> write_hint_file(uint32_t file_id, 
                                  const std::vector<HashIndex::HintEntry>& hints);
    
    // Read hint file if exists
    Result<bool> read_hint_file(uint32_t file_id);
    
    // Get list of all log file IDs in directory
    std::vector<uint32_t> get_log_file_ids() const;
};

} // namespace bitcask

#endif // BITCASK_H
#ifndef BITCASK_LOG_FILE_H
#define BITCASK_LOG_FILE_H

#include "types.h"
#include <fstream>
#include <string>
#include <vector>

namespace bitcask {

// Represents a single log file in the Bitcask database
class LogFile {
public:
    LogFile(uint32_t file_id, const std::string& directory, bool read_only = false);
    ~LogFile();
    
    // Write a key-value entry to the log
    Result<uint64_t> append(const std::string& key, const std::string& value, uint32_t timestamp);
    
    // Read a value at a specific position
    Result<std::string> read_value(uint64_t pos, uint32_t value_size);
    
    // Get current file size
    uint64_t size() const { return current_size_; }
    
    // Get file ID
    uint32_t id() const { return file_id_; }
    
    // Close the file
    void close();
    
    // Check if file is active (writable)
    bool is_active() const { return !read_only_; }
    
    // Calculate CRC-32 checksum
    static uint32_t calculate_crc32(const uint8_t* data, size_t length);
    
    // Validate and read all entries (for recovery/merge)
    struct EntryMetadata {
        std::string key;
        uint64_t value_pos;
        uint32_t value_size;
        uint32_t timestamp;
    };
    Result<std::vector<EntryMetadata>> read_all_entries();
    
private:
    uint32_t file_id_;
    std::string filepath_;
    std::fstream file_;
    bool read_only_;
    uint64_t current_size_;
    
    std::string get_filepath(uint32_t file_id, const std::string& directory);
    
    // Pack header and data for CRC calculation
    std::vector<uint8_t> pack_entry(uint32_t timestamp, const std::string& key, 
                                    const std::string& value);
};

} // namespace bitcask

#endif // BITCASK_LOG_FILE_H
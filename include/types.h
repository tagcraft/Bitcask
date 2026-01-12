#ifndef BITCASK_TYPES_H
#define BITCASK_TYPES_H

#include <cstdint>
#include <string>
#include <limits>
#include <utility>

namespace bitcask {

// Log entry header structure (on-disk format)
struct LogEntryHeader {
    uint32_t crc;           // CRC-32 checksum for data integrity
    uint32_t timestamp;     // Unix timestamp
    uint32_t key_size;      // Size of key in bytes
    uint32_t value_size;    // Size of value in bytes
} __attribute__((packed));  // Prevent padding for binary consistency

// Hash index metadata (in-memory)
struct IndexEntry {
    uint32_t file_id;       // Which log file contains this entry
    uint64_t value_pos;     // Byte offset to value in file
    uint32_t value_size;    // Size of value for reading
    uint32_t timestamp;     // Timestamp of entry
    
    // Tombstone check: deleted entries have max values
    bool is_tombstone() const {
        return file_id == std::numeric_limits<uint32_t>::max() &&
               value_pos == std::numeric_limits<uint64_t>::max() &&
               value_size == std::numeric_limits<uint32_t>::max();
    }
    
    // Create a tombstone entry
    static IndexEntry create_tombstone(uint32_t ts) {
        return {
            std::numeric_limits<uint32_t>::max(),
            std::numeric_limits<uint64_t>::max(),
            std::numeric_limits<uint32_t>::max(),
            ts
        };
    }
};

// Configuration for Bitcask instance
struct Config {
    std::string directory;              // Database directory path
    uint64_t max_file_size = 2ULL * 1024 * 1024 * 1024;  // 2GB default
    
    Config(const std::string& dir) : directory(dir) {}
};

// Tag type for error constructor disambiguation
struct ErrorTag {};
inline constexpr ErrorTag error_tag{};

// Result type for error handling
template<typename T>
struct Result {
    T value;
    std::string error;
    bool success;
    
    // Success constructor - uses move semantics for non-copyable types
    Result(T&& val) : value(std::move(val)), success(true) {}
    Result(const T& val) : value(val), success(true) {}
    
    // Error constructor - uses tag to disambiguate
    Result(ErrorTag, const std::string& err) : error(err), success(false) {}
    
    // Factory methods for clarity
    static Result Ok(T&& val) { return Result(std::forward<T>(val)); }
    static Result Ok(const T& val) { return Result(val); }
    static Result Err(const std::string& err) { return Result(error_tag, err); }
    
    bool ok() const { return success; }
    const std::string& err() const { return error; }
};

// Specialization for void operations
template<>
struct Result<void> {
    std::string error;
    bool success;
    
    Result() : success(true) {}
    Result(ErrorTag, const std::string& err) : error(err), success(false) {}
    
    static Result Ok() { return Result(); }
    static Result Err(const std::string& err) { return Result(error_tag, err); }
    
    bool ok() const { return success; }
    const std::string& err() const { return error; }
};

} // namespace bitcask

#endif // BITCASK_TYPES_H
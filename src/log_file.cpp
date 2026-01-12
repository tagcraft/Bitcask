#include "../include/log_file.h"
#include <sys/stat.h>
#include <ctime>
#include <cstring>
#include <sstream>
#include <iomanip>

namespace bitcask {

LogFile::LogFile(uint32_t file_id, const std::string& directory, bool read_only)
    : file_id_(file_id), read_only_(read_only), current_size_(0) {
    
    filepath_ = get_filepath(file_id, directory);
    
    std::ios_base::openmode mode = std::ios::binary;
    if (read_only) {
        mode |= std::ios::in;
    } else {
        mode |= std::ios::in | std::ios::out | std::ios::app;
    }
    
    file_.open(filepath_, mode);
    
    // If file doesn't exist and we're writing, create it
    if (!file_.is_open() && !read_only) {
        file_.open(filepath_, std::ios::binary | std::ios::out);
        file_.close();
        file_.open(filepath_, mode);
    }
    
    // Get current file size
    if (file_.is_open()) {
        file_.seekg(0, std::ios::end);
        current_size_ = file_.tellg();
        file_.seekg(0, std::ios::beg);
    }
}

LogFile::~LogFile() {
    close();
}

void LogFile::close() {
    if (file_.is_open()) {
        file_.close();
    }
}

std::string LogFile::get_filepath(uint32_t file_id, const std::string& directory) {
    std::ostringstream oss;
    oss << directory << "/cask." << file_id;
    return oss.str();
}

Result<uint64_t> LogFile::append(const std::string& key, const std::string& value, 
                                  uint32_t timestamp) {
    if (read_only_) {
        return Result<uint64_t>::Err("Cannot append to read-only file");
    }
    
    if (!file_.is_open()) {
        return Result<uint64_t>::Err("File not open");
    }
    
    // Pack the entry for CRC calculation
    auto packed = pack_entry(timestamp, key, value);
    
    // Calculate CRC (excluding the CRC field itself)
    uint32_t crc = calculate_crc32(packed.data() + 4, packed.size() - 4);
    
    // Write CRC at the beginning
    std::memcpy(packed.data(), &crc, sizeof(crc));
    
    // Get position where value starts (for index)
    uint64_t value_pos = current_size_ + sizeof(LogEntryHeader) + key.size();
    
    // Write to file
    file_.seekp(0, std::ios::end);
    file_.write(reinterpret_cast<const char*>(packed.data()), packed.size());
    file_.flush();
    
    current_size_ += packed.size();
    
    return Result<uint64_t>::Ok(value_pos);
}

std::vector<uint8_t> LogFile::pack_entry(uint32_t timestamp, const std::string& key,
                                          const std::string& value) {
    LogEntryHeader header;
    header.crc = 0;  // Will be calculated
    header.timestamp = timestamp;
    header.key_size = key.size();
    header.value_size = value.size();
    
    size_t total_size = sizeof(LogEntryHeader) + key.size() + value.size();
    std::vector<uint8_t> packed(total_size);
    
    // Copy header
    std::memcpy(packed.data(), &header, sizeof(LogEntryHeader));
    
    // Copy key
    std::memcpy(packed.data() + sizeof(LogEntryHeader), key.data(), key.size());
    
    // Copy value
    std::memcpy(packed.data() + sizeof(LogEntryHeader) + key.size(), 
                value.data(), value.size());
    
    return packed;
}

Result<std::string> LogFile::read_value(uint64_t pos, uint32_t value_size) {
    if (!file_.is_open()) {
        return Result<std::string>::Err("File not open");
    }
    
    std::vector<char> buffer(value_size);
    
    file_.seekg(pos, std::ios::beg);
    file_.read(buffer.data(), value_size);
    
    if (!file_.good() && !file_.eof()) {
        return Result<std::string>::Err("Failed to read value from file");
    }
    
    return Result<std::string>::Ok(std::string(buffer.begin(), buffer.end()));
}

// CRC-32 implementation (polynomial 0xEDB88320)
uint32_t LogFile::calculate_crc32(const uint8_t* data, size_t length) {
    uint32_t crc = 0xFFFFFFFF;
    
    for (size_t i = 0; i < length; ++i) {
        crc ^= data[i];
        for (int j = 0; j < 8; ++j) {
            if (crc & 1) {
                crc = (crc >> 1) ^ 0xEDB88320;
            } else {
                crc >>= 1;
            }
        }
    }
    
    return ~crc;
}

Result<std::vector<LogFile::EntryMetadata>> LogFile::read_all_entries() {
    if (!file_.is_open()) {
        return Result<std::vector<EntryMetadata>>::Err("File not open");
    }
    
    std::vector<EntryMetadata> entries;
    file_.seekg(0, std::ios::beg);
    
    while (file_.tellg() < static_cast<std::streampos>(current_size_)) {
        uint64_t entry_start = file_.tellg();
        
        // Read header
        LogEntryHeader header;
        file_.read(reinterpret_cast<char*>(&header), sizeof(LogEntryHeader));
        
        if (!file_.good()) {
            break;  // Incomplete entry, likely from crash
        }
        
        // Read key
        std::vector<char> key_buffer(header.key_size);
        file_.read(key_buffer.data(), header.key_size);
        
        // Read value (just to validate CRC, we don't store it)
        std::vector<char> value_buffer(header.value_size);
        file_.read(value_buffer.data(), header.value_size);
        
        if (!file_.good() && !file_.eof()) {
            break;  // Incomplete entry
        }
        
        // Validate CRC
        std::vector<uint8_t> data_for_crc(sizeof(LogEntryHeader) - 4 + 
                                          header.key_size + header.value_size);
        std::memcpy(data_for_crc.data(), 
                    reinterpret_cast<uint8_t*>(&header) + 4, 
                    sizeof(LogEntryHeader) - 4);
        std::memcpy(data_for_crc.data() + sizeof(LogEntryHeader) - 4,
                    key_buffer.data(), header.key_size);
        std::memcpy(data_for_crc.data() + sizeof(LogEntryHeader) - 4 + header.key_size,
                    value_buffer.data(), header.value_size);
        
        uint32_t calculated_crc = calculate_crc32(data_for_crc.data(), data_for_crc.size());
        
        if (calculated_crc != header.crc) {
            // Corrupted entry, stop reading
            break;
        }
        
        // Valid entry
        EntryMetadata meta;
        meta.key = std::string(key_buffer.begin(), key_buffer.end());
        meta.value_pos = entry_start + sizeof(LogEntryHeader) + header.key_size;
        meta.value_size = header.value_size;
        meta.timestamp = header.timestamp;
        
        entries.push_back(meta);
    }
    
    return Result<std::vector<EntryMetadata>>::Ok(entries);
}

} // namespace bitcask
#include "../include/bitcask.h"
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#include <algorithm>
#include <ctime>
#include <iostream>
#include <fstream>
#include <sstream>
#include <map>

namespace bitcask {

Bitcask::Bitcask(const Config& config) 
    : config_(config), next_file_id_(0) {
}

Bitcask::~Bitcask() {
    // Ensure all files are closed
    active_file_.reset();
    old_files_.clear();
}

Result<std::unique_ptr<Bitcask>> Bitcask::open(const Config& config) {
    auto db = std::unique_ptr<Bitcask>(new Bitcask(config));
    
    auto init_result = db->initialize();
    if (!init_result.ok()) {
        return Result<std::unique_ptr<Bitcask>>::Err(init_result.err());
    }
    
    return Result<std::unique_ptr<Bitcask>>::Ok(std::move(db));
}

Result<void> Bitcask::initialize() {
    // Create directory if it doesn't exist
    struct stat st;
    if (stat(config_.directory.c_str(), &st) != 0) {
        #ifdef _WIN32
        if (mkdir(config_.directory.c_str()) != 0) {
        #else
        if (mkdir(config_.directory.c_str(), 0755) != 0) {
        #endif
            return Result<void>::Err("Failed to create database directory");
        }
    }
    
    // Load existing files
    auto load_result = load_existing_files();
    if (!load_result.ok()) {
        return load_result;
    }
    
    // Create initial active file if none exists
    if (!active_file_) {
        auto rotate_result = rotate_active_file();
        if (!rotate_result.ok()) {
            return rotate_result;
        }
    }
    
    return Result<void>::Ok();
}

Result<void> Bitcask::load_existing_files() {
    auto file_ids = get_log_file_ids();
    
    if (file_ids.empty()) {
        return Result<void>::Ok();
    }
    
    // Sort file IDs to process in order
    std::sort(file_ids.begin(), file_ids.end());
    
    for (size_t i = 0; i < file_ids.size(); ++i) {
        uint32_t file_id = file_ids[i];
        bool is_last = (i == file_ids.size() - 1);
        
        // Try to load from hint file first
        auto hint_result = read_hint_file(file_id);
        if (hint_result.ok() && hint_result.value) {
            // Successfully loaded from hint file
            if (!is_last) {
                old_files_.push_back(
                    std::make_unique<LogFile>(file_id, config_.directory, true)
                );
            } else {
                active_file_ = std::make_unique<LogFile>(file_id, config_.directory, false);
            }
            continue;
        }
        
        // Load from log file directly
        auto log_file = std::make_unique<LogFile>(file_id, config_.directory, true);
        auto entries_result = log_file->read_all_entries();
        
        if (!entries_result.ok()) {
            return Result<void>::Err("Failed to read log file: " + entries_result.err());
        }
        
        // Rebuild index from entries
        for (const auto& entry : entries_result.value) {
            IndexEntry idx_entry;
            idx_entry.file_id = file_id;
            idx_entry.value_pos = entry.value_pos;
            idx_entry.value_size = entry.value_size;
            idx_entry.timestamp = entry.timestamp;
            
            index_.put(entry.key, idx_entry);
        }
        
        // Last file becomes active
        if (is_last) {
            // Reopen as writable
            log_file.reset();
            active_file_ = std::make_unique<LogFile>(file_id, config_.directory, false);
        } else {
            old_files_.push_back(std::move(log_file));
        }
    }
    
    if (!file_ids.empty()) {
        next_file_id_ = file_ids.back() + 1;
    }
    
    return Result<void>::Ok();
}

std::vector<uint32_t> Bitcask::get_log_file_ids() const {
    std::vector<uint32_t> file_ids;
    
    DIR* dir = opendir(config_.directory.c_str());
    if (!dir) {
        return file_ids;
    }
    
    struct dirent* entry;
    while ((entry = readdir(dir)) != nullptr) {
        std::string filename = entry->d_name;
        
        // Look for files matching "cask.N" pattern
        if (filename.find("cask.") == 0 && filename.find(".hint") == std::string::npos) {
            try {
                uint32_t file_id = std::stoul(filename.substr(5));
                file_ids.push_back(file_id);
            } catch (...) {
                // Ignore files that don't match the pattern
            }
        }
    }
    
    closedir(dir);
    return file_ids;
}

Result<void> Bitcask::rotate_active_file() {
    if (active_file_) {
        // Move current active to old files
        uint32_t old_id = active_file_->id();
        active_file_->close();
        old_files_.push_back(
            std::make_unique<LogFile>(old_id, config_.directory, true)
        );
    }
    
    // Create new active file
    active_file_ = std::make_unique<LogFile>(next_file_id_++, config_.directory, false);
    
    return Result<void>::Ok();
}

uint32_t Bitcask::get_timestamp() const {
    return static_cast<uint32_t>(std::time(nullptr));
}

Result<void> Bitcask::put(const std::string& key, const std::string& value) {
    if (key.empty()) {
        return Result<void>::Err("Key cannot be empty");
    }
    
    uint32_t timestamp = get_timestamp();
    
    // Append to active file
    auto append_result = active_file_->append(key, value, timestamp);
    if (!append_result.ok()) {
        return Result<void>::Err(append_result.err());
    }
    
    // Update index
    IndexEntry entry;
    entry.file_id = active_file_->id();
    entry.value_pos = append_result.value;
    entry.value_size = value.size();
    entry.timestamp = timestamp;
    
    index_.put(key, entry);
    
    // Check if we need to rotate
    if (active_file_->size() >= config_.max_file_size) {
        auto rotate_result = rotate_active_file();
        if (!rotate_result.ok()) {
            return rotate_result;
        }
    }
    
    return Result<void>::Ok();
}

Result<std::string> Bitcask::get(const std::string& key) {
    auto index_entry = index_.get(key);
    if (!index_entry.has_value()) {
        return Result<std::string>::Err("Key not found");
    }
    
    const auto& entry = index_entry.value();
    
    // Find the right file
    LogFile* target_file = nullptr;
    
    if (active_file_ && active_file_->id() == entry.file_id) {
        target_file = active_file_.get();
    } else {
        for (const auto& file : old_files_) {
            if (file->id() == entry.file_id) {
                target_file = file.get();
                break;
            }
        }
    }
    
    if (!target_file) {
        return Result<std::string>::Err("File not found for key");
    }
    
    return target_file->read_value(entry.value_pos, entry.value_size);
}

Result<void> Bitcask::del(const std::string& key) {
    if (!index_.contains(key)) {
        return Result<void>::Err("Key not found");
    }
    
    uint32_t timestamp = get_timestamp();
    
    // Write tombstone to log (empty value)
    auto append_result = active_file_->append(key, "", timestamp);
    if (!append_result.ok()) {
        return Result<void>::Err(append_result.err());
    }
    
    // Mark as deleted in index
    index_.remove(key, timestamp);
    
    return Result<void>::Ok();
}

std::vector<std::string> Bitcask::list_keys() {
    return index_.keys();
}

Result<void> Bitcask::sync() {
    // In C++, flush is already called in append
    // This is here for API completeness
    return Result<void>::Ok();
}

Result<void> Bitcask::merge() {
    if (old_files_.empty()) {
        return Result<void>::Ok();  // Nothing to merge
    }
    
    // Create a temporary directory for merged files
    std::string merge_dir = config_.directory + "/.merge";
    struct stat st;
    if (stat(merge_dir.c_str(), &st) != 0) {
        #ifdef _WIN32
        mkdir(merge_dir.c_str());
        #else
        mkdir(merge_dir.c_str(), 0755);
        #endif
    }
    
    // Get all current live keys
    auto live_keys = index_.keys();
    
    // Group keys by file
    std::map<uint32_t, std::vector<std::string>> keys_by_file;
    for (const auto& key : live_keys) {
        auto entry = index_.get(key);
        if (entry.has_value() && entry->file_id != active_file_->id()) {
            keys_by_file[entry->file_id].push_back(key);
        }
    }
    
    // Process each old file
    std::vector<uint32_t> merged_file_ids;
    for (const auto& [file_id, keys] : keys_by_file) {
        if (keys.empty()) continue;
        
        // Create new merged file
        uint32_t new_file_id = next_file_id_++;
        auto merged_file = std::make_unique<LogFile>(new_file_id, merge_dir, false);
        
        std::vector<HashIndex::HintEntry> hints;
        
        // Copy live entries
        for (const auto& key : keys) {
            auto value_result = get(key);
            if (!value_result.ok()) continue;
            
            auto entry = index_.get(key);
            if (!entry.has_value()) continue;
            
            uint32_t timestamp = entry->timestamp;
            auto append_result = merged_file->append(key, value_result.value, timestamp);
            
            if (append_result.ok()) {
                // Update hint
                IndexEntry hint_entry;
                hint_entry.file_id = new_file_id;
                hint_entry.value_pos = append_result.value;
                hint_entry.value_size = value_result.value.size();
                hint_entry.timestamp = timestamp;
                
                // Construct HintEntry properly
                HashIndex::HintEntry hint;
                hint.key = key;
                hint.entry = hint_entry;
                hints.push_back(hint);
            }
        }
        
        merged_file->close();
        
        // Write hint file
        write_hint_file(new_file_id, hints);
        
        merged_file_ids.push_back(new_file_id);
    }
    
    // Move merged files to main directory and delete old files
    for (size_t i = 0; i < old_files_.size(); ++i) {
        uint32_t old_id = old_files_[i]->id();
        std::string old_path = config_.directory + "/cask." + std::to_string(old_id);
        remove(old_path.c_str());
    }
    
    for (uint32_t merged_id : merged_file_ids) {
        std::string src = merge_dir + "/cask." + std::to_string(merged_id);
        std::string dst = config_.directory + "/cask." + std::to_string(merged_id);
        std::rename(src.c_str(), dst.c_str());
        
        std::string src_hint = merge_dir + "/cask." + std::to_string(merged_id) + ".hint";
        std::string dst_hint = config_.directory + "/cask." + std::to_string(merged_id) + ".hint";
        std::rename(src_hint.c_str(), dst_hint.c_str());
    }
    
    // Reload old files
    old_files_.clear();
    for (uint32_t file_id : merged_file_ids) {
        old_files_.push_back(
            std::make_unique<LogFile>(file_id, config_.directory, true)
        );
    }
    
    return Result<void>::Ok();
}

Result<void> Bitcask::write_hint_file(uint32_t file_id, 
                                       const std::vector<HashIndex::HintEntry>& hints) {
    std::string hint_path = config_.directory + "/cask." + std::to_string(file_id) + ".hint";
    std::ofstream hint_file(hint_path, std::ios::binary);
    
    if (!hint_file.is_open()) {
        return Result<void>::Err("Failed to open hint file for writing");
    }
    
    for (const auto& hint : hints) {
        // Write: timestamp, key_size, value_size, value_pos, key
        hint_file.write(reinterpret_cast<const char*>(&hint.entry.timestamp), 
                       sizeof(hint.entry.timestamp));
        
        uint32_t key_size = hint.key.size();
        hint_file.write(reinterpret_cast<const char*>(&key_size), sizeof(key_size));
        hint_file.write(reinterpret_cast<const char*>(&hint.entry.value_size), 
                       sizeof(hint.entry.value_size));
        hint_file.write(reinterpret_cast<const char*>(&hint.entry.value_pos), 
                       sizeof(hint.entry.value_pos));
        hint_file.write(hint.key.data(), hint.key.size());
    }
    
    hint_file.close();
    return Result<void>::Ok();
}

Result<bool> Bitcask::read_hint_file(uint32_t file_id) {
    std::string hint_path = config_.directory + "/cask." + std::to_string(file_id) + ".hint";
    std::ifstream hint_file(hint_path, std::ios::binary);
    
    if (!hint_file.is_open()) {
        return Result<bool>::Ok(false);  // Hint file doesn't exist
    }
    
    while (hint_file.peek() != EOF) {
        IndexEntry entry;
        entry.file_id = file_id;
        
        hint_file.read(reinterpret_cast<char*>(&entry.timestamp), sizeof(entry.timestamp));
        
        uint32_t key_size;
        hint_file.read(reinterpret_cast<char*>(&key_size), sizeof(key_size));
        hint_file.read(reinterpret_cast<char*>(&entry.value_size), sizeof(entry.value_size));
        hint_file.read(reinterpret_cast<char*>(&entry.value_pos), sizeof(entry.value_pos));
        
        std::vector<char> key_buffer(key_size);
        hint_file.read(key_buffer.data(), key_size);
        
        if (!hint_file.good() && !hint_file.eof()) {
            return Result<bool>::Err("Corrupted hint file");
        }
        
        std::string key(key_buffer.begin(), key_buffer.end());
        index_.put(key, entry);
    }
    
    return Result<bool>::Ok(true);
}

} // namespace bitcask
#include "../include/bitcask.h"
#include <iostream>
#include <string>
#include <vector>

using namespace bitcask;

void print_usage(const char* program_name) {
    std::cerr << "Usage: " << program_name << " -db <directory> <command> [args...]\n\n";
    std::cerr << "Commands:\n";
    std::cerr << "  set <key> <value>   Set a key-value pair\n";
    std::cerr << "  get <key>           Get value for a key\n";
    std::cerr << "  del <key>           Delete a key\n";
    std::cerr << "  list                List all keys\n";
    std::cerr << "  merge               Compact log files\n\n";
    std::cerr << "Examples:\n";
    std::cerr << "  " << program_name << " -db ./mydb set user:1 alice\n";
    std::cerr << "  " << program_name << " -db ./mydb get user:1\n";
    std::cerr << "  " << program_name << " -db ./mydb del user:1\n";
    std::cerr << "  " << program_name << " -db ./mydb merge\n";
}

int main(int argc, char* argv[]) {
    // Parse command line arguments
    if (argc < 4) {
        print_usage(argv[0]);
        return 1;
    }
    
    std::string db_flag = argv[1];
    std::string db_dir = argv[2];
    std::string command = argv[3];
    
    if (db_flag != "-db") {
        std::cerr << "Error: First argument must be -db\n\n";
        print_usage(argv[0]);
        return 1;
    }
    
    // Open database
    Config config(db_dir);
    // For testing, use smaller file size (1MB instead of 2GB)
    config.max_file_size = 1024 * 1024;  // 1MB
    
    auto db_result = Bitcask::open(config);
    if (!db_result.ok()) {
        std::cerr << "Error opening database: " << db_result.err() << "\n";
        return 1;
    }
    
    auto& db = db_result.value;
    
    // Execute command
    if (command == "set") {
        if (argc < 6) {
            std::cerr << "Error: 'set' requires key and value arguments\n";
            print_usage(argv[0]);
            return 1;
        }
        
        std::string key = argv[4];
        std::string value = argv[5];
        
        auto result = db->put(key, value);
        if (!result.ok()) {
            std::cerr << "Error: " << result.err() << "\n";
            return 1;
        }
        
        std::cout << "OK\n";
        
    } else if (command == "get") {
        if (argc < 5) {
            std::cerr << "Error: 'get' requires key argument\n";
            print_usage(argv[0]);
            return 1;
        }
        
        std::string key = argv[4];
        
        auto result = db->get(key);
        if (!result.ok()) {
            std::cout << "(nil)\n";
            return 0;  // Not an error, just key doesn't exist
        }
        
        std::cout << result.value << "\n";
        
    } else if (command == "del") {
        if (argc < 5) {
            std::cerr << "Error: 'del' requires key argument\n";
            print_usage(argv[0]);
            return 1;
        }
        
        std::string key = argv[4];
        
        auto result = db->del(key);
        if (!result.ok()) {
            std::cerr << "Error: " << result.err() << "\n";
            return 1;
        }
        
        std::cout << "OK\n";
        
    } else if (command == "list") {
        auto keys = db->list_keys();
        
        if (keys.empty()) {
            std::cout << "(empty)\n";
        } else {
            for (const auto& key : keys) {
                std::cout << key << "\n";
            }
        }
        
    } else if (command == "merge") {
        std::cout << "Starting merge process...\n";
        
        auto result = db->merge();
        if (!result.ok()) {
            std::cerr << "Error during merge: " << result.err() << "\n";
            return 1;
        }
        
        std::cout << "Merge completed successfully\n";
        
    } else {
        std::cerr << "Error: Unknown command '" << command << "'\n\n";
        print_usage(argv[0]);
        return 1;
    }
    
    return 0;
}
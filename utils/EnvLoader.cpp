#include "utils/EnvLoader.h"
#include <fstream>
#include <iostream>
#include <sstream>
#include <cstdlib>

std::unordered_map<std::string, std::string> EnvLoader::env_vars_;
bool EnvLoader::loaded_ = false;

bool EnvLoader::load_env_file(const std::string& file_path) {
    if (loaded_) {
        return true; // Already loaded
    }

    std::ifstream file(file_path);
    if (!file.is_open()) {
        std::cerr << "[ENV] Could not open .env file: " << file_path << std::endl;
        return false;
    }

    std::string line;
    int line_number = 0;
    while (std::getline(file, line)) {
        line_number++;
        
        // Skip empty lines and comments
        if (line.empty() || line[0] == '#') {
            continue;
        }

        // Find the equals sign
        size_t pos = line.find('=');
        if (pos == std::string::npos) {
            std::cerr << "[ENV] Invalid line " << line_number << " in .env file: " << line << std::endl;
            continue;
        }

        // Extract key and value
        std::string key = line.substr(0, pos);
        std::string value = line.substr(pos + 1);

        // Trim whitespace
        key.erase(0, key.find_first_not_of(" \t"));
        key.erase(key.find_last_not_of(" \t") + 1);
        value.erase(0, value.find_first_not_of(" \t"));
        value.erase(value.find_last_not_of(" \t") + 1);

        // Remove quotes if present
        if (value.length() >= 2 && 
            ((value[0] == '"' && value[value.length()-1] == '"') ||
             (value[0] == '\'' && value[value.length()-1] == '\''))) {
            value = value.substr(1, value.length() - 2);
        }

        if (!key.empty()) {
            env_vars_[key] = value;
            std::cerr << "[ENV] Loaded: " << key << " = " << (key.find("KEY") != std::string::npos ? "[HIDDEN]" : value) << std::endl;
        }
    }

    loaded_ = true;
    std::cerr << "[ENV] Loaded " << env_vars_.size() << " environment variables from .env file" << std::endl;
    return true;
}

std::string EnvLoader::get_env(const std::string& key, const std::string& default_value) {
    // First check our loaded env vars
    auto it = env_vars_.find(key);
    if (it != env_vars_.end()) {
        return it->second;
    }

    // Fallback to system environment
    const char* env_value = std::getenv(key.c_str());
    if (env_value) {
        return std::string(env_value);
    }

    return default_value;
}

void EnvLoader::set_env(const std::string& key, const std::string& value) {
    env_vars_[key] = value;
} 
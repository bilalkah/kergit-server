#include "utils/EnvLoader.h"

#include <cstdlib>
#include <fstream>
#include <iostream>
#include <sstream>
#include <vector>

namespace utils {

std::unordered_map<std::string, std::string> EnvLoader::env_vars_;
bool EnvLoader::loaded_ = false;

std::optional<std::string> EnvLoader::lookup_env(const std::string& key) {
    auto it = env_vars_.find(key);
    if (it != env_vars_.end()) {
        return it->second;
    }

    const char* env_value = std::getenv(key.c_str());
    if (env_value) {
        return std::string(env_value);
    }

    return std::nullopt;
}

bool EnvLoader::load_env_file(const std::string& file_path) {
    if (loaded_) {
        return true;  // Already loaded
    }

    std::ifstream file(file_path);
    if (!file.is_open()) {
        logger().log(LogLevel::WARN, "Could not open .env file: ", file_path);
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
            logger().log(LogLevel::WARN, "Invalid line in .env file at line ", line_number, ": ",
                         line);
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
        if (value.length() >= 2 && ((value[0] == '"' && value[value.length() - 1] == '"') ||
                                    (value[0] == '\'' && value[value.length() - 1] == '\''))) {
            value = value.substr(1, value.length() - 2);
        }

        if (!key.empty()) {
            env_vars_[key] = value;
        }
    }

    loaded_ = true;
    logger().log(LogLevel::INFO, "Loaded " + std::to_string(env_vars_.size()) +
                                     " environment variables from .env file");
    return true;
}

std::string EnvLoader::get_env(const std::string& key, const std::string& default_value) {
    if (auto value = lookup_env(key)) {
        return *value;
    }

    return default_value;
}

std::vector<std::string> EnvLoader::get_env_list(const std::string& key,
                                                 const std::vector<std::string>& default_values) {
    auto trim = [](std::string& value) {
        value.erase(0, value.find_first_not_of(" \t\n\r"));
        value.erase(value.find_last_not_of(" \t\n\r") + 1);
    };

    std::string raw = get_env(key, "");
    if (raw.empty()) {
        return default_values;
    }

    trim(raw);
    if (raw.size() >= 2 && raw.front() == '[' && raw.back() == ']') {
        raw = raw.substr(1, raw.size() - 2);
        trim(raw);
    }

    std::vector<std::string> values;
    std::stringstream ss(raw);
    std::string item;
    while (std::getline(ss, item, ',')) {
        trim(item);
        if (item.size() >= 2 && ((item.front() == '"' && item.back() == '"') ||
                                 (item.front() == '\'' && item.back() == '\''))) {
            item = item.substr(1, item.size() - 2);
        }
        trim(item);
        if (!item.empty()) {
            values.push_back(item);
        }
    }

    if (values.empty()) {
        return default_values;
    }
    return values;
}

void EnvLoader::set_env(const std::string& key, const std::string& value) {
    env_vars_[key] = value;
}

void EnvLoader::clear_env() {
    env_vars_.clear();
    loaded_ = false;
}

}  // namespace utils

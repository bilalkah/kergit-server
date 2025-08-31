#pragma once
#include <string>
#include <unordered_map>

class EnvLoader {
public:
    static bool load_env_file(const std::string& file_path = ".env");
    static std::string get_env(const std::string& key, const std::string& default_value = "");
    static void set_env(const std::string& key, const std::string& value);

private:
    static std::unordered_map<std::string, std::string> env_vars_;
    static bool loaded_;
}; 
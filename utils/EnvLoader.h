#ifndef UTILS_ENVLOADER_H
#define UTILS_ENVLOADER_H

#include "utils/Loggable.h"

#include <string>
#include <unordered_map>

namespace utils {

class EnvLoader : public Loggable {
   public:
    static bool load_env_file(const std::string& file_path = ".env");
    static std::string get_env(const std::string& key, const std::string& default_value = "");
    static void set_env(const std::string& key, const std::string& value);
    static void clear_env();

   private:
    EnvLoader() : Loggable() {}
    static EnvLoader& logger() {
        static EnvLoader inst;  // Meyers singleton — only for logging
        return inst;
    }
    static std::unordered_map<std::string, std::string> env_vars_;
    static bool loaded_;
};

}  // namespace utils

#endif  // UTILS_ENVLOADER_H

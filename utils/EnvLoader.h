#ifndef UTILS_ENVLOADER_H
#define UTILS_ENVLOADER_H

#include "utils/Loggable.h"

#include <algorithm>
#include <cctype>
#include <charconv>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <unordered_map>
#include <vector>

namespace utils {

class EnvLoader : public Loggable {
   public:
    static bool load_env_file(const std::string& file_path = ".env");
    static std::string get_env(const std::string& key, const std::string& default_value = "");
    template <typename T>
    static T get(const std::string& key, T default_value = {}) {
        using ValueType = std::remove_cvref_t<T>;

        if constexpr (std::is_same_v<ValueType, std::string>) {
            return get_env(key, default_value);
        } else {
            auto raw = lookup_env(key);
            if (!raw.has_value()) {
                return default_value;
            }
            return parse<ValueType>(key, *raw);
        }
    }
    static std::vector<std::string> get_env_list(
        const std::string& key, const std::vector<std::string>& default_values = {});
    static void set_env(const std::string& key, const std::string& value);
    static void clear_env();

   private:
    template <typename>
    static constexpr bool kAlwaysFalse = false;

    static std::string_view trim(std::string_view value) {
        while (!value.empty() && std::isspace(static_cast<unsigned char>(value.front()))) {
            value.remove_prefix(1);
        }
        while (!value.empty() && std::isspace(static_cast<unsigned char>(value.back()))) {
            value.remove_suffix(1);
        }
        return value;
    }

    static bool parse_bool(const std::string& key, std::string_view raw) {
        std::string normalized(trim(raw));
        std::transform(normalized.begin(), normalized.end(), normalized.begin(),
                       [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

        if (normalized == "1" || normalized == "true" || normalized == "yes" ||
            normalized == "on") {
            return true;
        }
        if (normalized == "0" || normalized == "false" || normalized == "no" ||
            normalized == "off") {
            return false;
        }
        throw std::invalid_argument("Invalid boolean value for env key '" + key +
                                    "': " + std::string(raw));
    }

    template <typename T>
    static T parse_integral(const std::string& key, std::string_view raw) {
        const auto trimmed = trim(raw);
        T value{};
        const auto* begin = trimmed.data();
        const auto* end = trimmed.data() + trimmed.size();
        const auto [ptr, ec] = std::from_chars(begin, end, value);
        if (ec != std::errc{} || ptr != end) {
            throw std::invalid_argument("Invalid integer value for env key '" + key +
                                        "': " + std::string(raw));
        }
        return value;
    }

    template <typename T>
    static T parse_floating(const std::string& key, std::string_view raw) {
        std::istringstream input{std::string(trim(raw))};
        input >> std::noskipws;
        T value{};
        input >> value;
        if (input.fail() || !input.eof()) {
            throw std::invalid_argument("Invalid floating-point value for env key '" + key +
                                        "': " + std::string(raw));
        }
        return value;
    }

    template <typename T>
    static T parse(const std::string& key, std::string_view raw) {
        if constexpr (std::is_same_v<T, bool>) {
            return parse_bool(key, raw);
        } else if constexpr (std::is_integral_v<T>) {
            return parse_integral<T>(key, raw);
        } else if constexpr (std::is_floating_point_v<T>) {
            return parse_floating<T>(key, raw);
        } else {
            static_assert(kAlwaysFalse<T>, "Unsupported EnvLoader::get<T> type");
        }
    }

    EnvLoader() : Loggable() {}
    static EnvLoader& logger() {
        static EnvLoader inst;  // Meyers singleton — only for logging
        return inst;
    }
    static std::optional<std::string> lookup_env(const std::string& key);
    static std::unordered_map<std::string, std::string> env_vars_;
    static bool loaded_;
};

}  // namespace utils

#endif  // UTILS_ENVLOADER_H

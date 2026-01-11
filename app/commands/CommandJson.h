#ifndef APP_COMMANDS_COMMANDJSON_H
#define APP_COMMANDS_COMMANDJSON_H

#include <charconv>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>

#include <nlohmann/json.hpp>

namespace app::commands {

inline std::optional<uint64_t> read_uint64(const nlohmann::json& obj, std::string_view key) {
    auto it = obj.find(key);
    if (it == obj.end() || it->is_null()) return std::nullopt;
    if (it->is_number_unsigned()) return it->get<uint64_t>();
    if (it->is_number_integer()) {
        auto val = it->get<int64_t>();
        if (val < 0) return std::nullopt;
        return static_cast<uint64_t>(val);
    }
    if (it->is_string()) {
        const auto& raw = it->get_ref<const std::string&>();
        if (raw.empty()) return std::nullopt;
        uint64_t value = 0;
        auto [ptr, ec] = std::from_chars(raw.data(), raw.data() + raw.size(), value);
        if (ec != std::errc() || ptr != raw.data() + raw.size()) return std::nullopt;
        return value;
    }
    return std::nullopt;
}

}  // namespace app::commands

#endif  // APP_COMMANDS_COMMANDJSON_H

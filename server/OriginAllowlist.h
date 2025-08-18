#pragma once
#include <array>
#include <string_view>

inline bool origin_allowed(std::string_view origin) {
    static constexpr std::array allowed{
        "http://localhost:8080",
        "https://localhost:8080",  // keep both in case you switch to https dev
        "http://localhost:5173", "https://localhost:5173"};
    for (auto a : allowed)
        if (origin == a) return true;
    return false;
}

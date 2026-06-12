#include "utils/Uuid.h"

#include <array>
#include <random>

namespace utils {
namespace {
thread_local std::mt19937_64 rng([] {
    std::random_device rd;
    std::seed_seq seq{rd(), rd(), rd(), rd(), rd(), rd(), rd(), rd()};
    return std::mt19937_64(seq);
}());

inline char hex_digit(uint8_t v) {
    constexpr char kHex[] = "0123456789abcdef";
    return kHex[v & 0x0F];
}
}  // namespace

std::string generate_uuid_v4() {
    std::array<uint8_t, 16> bytes{};
    std::uniform_int_distribution<uint32_t> dist(0, 255);
    for (auto& b : bytes) {
        b = static_cast<uint8_t>(dist(rng));
    }

    // RFC 4122 variant and version.
    bytes[6] = static_cast<uint8_t>((bytes[6] & 0x0F) | 0x40);
    bytes[8] = static_cast<uint8_t>((bytes[8] & 0x3F) | 0x80);

    std::string out;
    out.reserve(36);
    for (std::size_t i = 0; i < bytes.size(); ++i) {
        if (i == 4 || i == 6 || i == 8 || i == 10) {
            out.push_back('-');
        }
        out.push_back(hex_digit(bytes[i] >> 4));
        out.push_back(hex_digit(bytes[i]));
    }
    return out;
}

}  // namespace utils

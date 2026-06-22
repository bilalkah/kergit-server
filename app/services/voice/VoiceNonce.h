#ifndef APP_SERVICES_VOICE_VOICENONCE_H_
#define APP_SERVICES_VOICE_VOICENONCE_H_

#include <cstdint>
#include <random>
#include <string>

namespace app::services::voice {

// Generates a 32-char lowercase-hex random nonce (128 bits of entropy). Shared by the
// command-path intent nonce and the one-time voice resume ids.
inline std::string generate_nonce_hex() {
    thread_local std::mt19937_64 rng(std::random_device{}());
    constexpr char kHex[] = "0123456789abcdef";

    std::string out;
    out.resize(32);
    for (size_t i = 0; i < 16; ++i) {
        const uint8_t byte = static_cast<uint8_t>(rng() & 0xFFu);
        out[i * 2] = kHex[(byte >> 4) & 0x0Fu];
        out[i * 2 + 1] = kHex[byte & 0x0Fu];
    }
    return out;
}

}  // namespace app::services::voice

#endif  // APP_SERVICES_VOICE_VOICENONCE_H_

#include "livekit/crypto/E2EEKeyManager.h"

#include <jwt-cpp/base.h>
#include <openssl/rand.h>
#include <stdexcept>

namespace livekit {

std::string E2EEKeyManager::get_or_create_key(const ChannelId& channel) {
    std::lock_guard lock(mutex_);

    auto it = channels_.find(channel);
    if (it != channels_.end() && !it->second.key.empty()) return it->second.key;

    auto& state = channels_[channel];
    state.key = generate_key();
    return state.key;
}

std::optional<std::string> E2EEKeyManager::get_key(const ChannelId& channel) const {
    std::lock_guard lock(mutex_);
    const auto it = channels_.find(channel);
    if (it == channels_.end() || it->second.key.empty()) return std::nullopt;
    return it->second.key;
}

void E2EEKeyManager::set_key(const ChannelId& channel, std::string key) {
    std::lock_guard lock(mutex_);
    auto& state = channels_[channel];
    state.key = std::move(key);
}

void E2EEKeyManager::clear_key(const ChannelId& channel) {
    std::lock_guard lock(mutex_);
    channels_.erase(channel);
}

std::string E2EEKeyManager::generate_key() const {
    unsigned char buf[32];
    if (RAND_bytes(buf, sizeof(buf)) != 1)
        throw std::runtime_error("E2EEKeyManager: RAND_bytes failed");

    std::string raw(reinterpret_cast<char*>(buf), 32);
    return jwt::base::encode<jwt::alphabet::base64>(raw);
}

}  // namespace livekit

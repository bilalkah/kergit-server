#include "livekit/crypto/E2EEKeyManager.h"

#include <jwt-cpp/base.h>
#include <openssl/rand.h>
#include <stdexcept>

namespace livekit {

E2EEKeyManager::ChannelKey E2EEKeyManager::get_or_create_key(const ChannelId& channel) {
    std::lock_guard lock(mutex_);

    auto it = channels_.find(channel);
    if (it != channels_.end() && !it->second.key.empty()) {
        return {it->second.key, it->second.key_index};
    }

    auto& state = channels_[channel];
    state.key = generate_key();
    state.key_index = 0;
    return {state.key, state.key_index};
}

std::optional<E2EEKeyManager::ChannelKey> E2EEKeyManager::get_key(const ChannelId& channel) const {
    std::lock_guard lock(mutex_);
    const auto it = channels_.find(channel);
    if (it == channels_.end() || it->second.key.empty()) return std::nullopt;
    return ChannelKey{it->second.key, it->second.key_index};
}

void E2EEKeyManager::set_key(const ChannelId& channel, std::string key, uint32_t key_index) {
    std::lock_guard lock(mutex_);
    auto& state = channels_[channel];
    state.key = std::move(key);
    state.key_index = key_index % kKeyringSize;
}

E2EEKeyManager::ChannelKey E2EEKeyManager::rotate_key(const ChannelId& channel) {
    std::lock_guard lock(mutex_);
    auto& state = channels_[channel];
    const bool had_key = !state.key.empty();
    state.key = generate_key();
    state.key_index = had_key ? (state.key_index + 1) % kKeyringSize : 0;
    return {state.key, state.key_index};
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

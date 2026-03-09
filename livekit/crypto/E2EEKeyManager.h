#ifndef LIVEKIT_CRYPTO_E2EEKEYMANAGER_H_
#define LIVEKIT_CRYPTO_E2EEKEYMANAGER_H_

#include "domains/ids/Ids.h"

#include <mutex>
#include <string>
#include <unordered_map>

namespace livekit {

/**
 * E2EEKeyManager
 *
 * Maintains encryption keys for voice channels.
 *
 * Keys exist only while a channel has active participants.
 * When the last participant leaves, the key is destroyed.
 */
class E2EEKeyManager {
public:
    /// Returns the current key or creates a new one if none exists.
    std::string get_or_create_key(const ChannelId& channel);

    /// Increment participant count.
    void increment_participant(const ChannelId& channel);

    /// Decrement participant count and remove key if empty.
    void decrement_participant(const ChannelId& channel);

    /// Immediately evict the key for a channel (e.g. when room is forcibly closed).
    void clear_key(const ChannelId& channel);

private:
    struct ChannelState {
        std::string key;
        size_t participants = 0;
    };

    std::string generate_key() const;

    mutable std::mutex mutex_;
    std::unordered_map<ChannelId, ChannelState> channels_;
};

} // namespace livekit

#endif
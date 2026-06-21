#ifndef LIVEKIT_CRYPTO_E2EEKEYMANAGER_H_
#define LIVEKIT_CRYPTO_E2EEKEYMANAGER_H_

#include "domains/ids/Ids.h"

#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>

namespace livekit {

/**
 * E2EEKeyManager
 *
 * Maintains encryption keys for voice channels.
 *
 * Key lifecycle is owned by VoiceService: keys are created on first join into an
 * empty channel and evicted via clear_key() when the channel empties, is rekeyed,
 * or is forcibly closed. This class is just the in-memory key store.
 */
class E2EEKeyManager {
   public:
    /// Returns the current key or creates a new one if none exists.
    std::string get_or_create_key(const ChannelId& channel);

    /// Returns current key if present.
    std::optional<std::string> get_key(const ChannelId& channel) const;

    /// Set/replace key for channel.
    void set_key(const ChannelId& channel, std::string key);

    /// Immediately evict the key for a channel (e.g. when room is forcibly closed).
    void clear_key(const ChannelId& channel);

   private:
    struct ChannelState {
        std::string key;
    };

    std::string generate_key() const;

    mutable std::mutex mutex_;
    std::unordered_map<ChannelId, ChannelState> channels_;
};

}  // namespace livekit

#endif

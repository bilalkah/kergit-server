#ifndef LIVEKIT_CRYPTO_E2EEKEYMANAGER_H_
#define LIVEKIT_CRYPTO_E2EEKEYMANAGER_H_

#include "domains/ids/Ids.h"

#include <cstdint>
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
    /// LiveKit ExternalE2EEKeyProvider keyring size. Key indices wrap within this so
    /// recently-rotated keys stay resolvable during the overlap window.
    static constexpr uint32_t kKeyringSize = 16;

    struct ChannelKey {
        std::string key;
        uint32_t key_index = 0;
    };

    /// Returns the current key+index, creating a fresh key at index 0 if none exists.
    ChannelKey get_or_create_key(const ChannelId& channel);

    /// Returns current key+index if present.
    std::optional<ChannelKey> get_key(const ChannelId& channel) const;

    /// Set/replace key for channel at an explicit index (recovery / load from storage).
    void set_key(const ChannelId& channel, std::string key, uint32_t key_index);

    /// Generate a new key and advance the index (mod keyring size); creates at index 0
    /// if the channel had no key. Returns the new key+index. Atomic.
    ChannelKey rotate_key(const ChannelId& channel);

    /// Immediately evict the key for a channel (e.g. when room is forcibly closed).
    void clear_key(const ChannelId& channel);

   private:
    struct ChannelState {
        std::string key;
        uint32_t key_index = 0;
    };

    std::string generate_key() const;

    mutable std::mutex mutex_;
    std::unordered_map<ChannelId, ChannelState> channels_;
};

}  // namespace livekit

#endif

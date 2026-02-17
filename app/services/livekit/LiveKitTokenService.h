#ifndef APP_SERVICES_LIVEKIT_LIVEKITTOKENSERVICE_H_
#define APP_SERVICES_LIVEKIT_LIVEKITTOKENSERVICE_H_

#include "domains/ids/Ids.h"

#include <chrono>
#include <mutex>
#include <string>
#include <unordered_map>

namespace app::services::livekit {

class LiveKitTokenService {
   public:
    struct TokenRequest {
        UserId identity;   // user id
        ChannelId room;    // voice room name
        bool can_publish;  // audio
        bool can_subscribe;
        std::chrono::seconds ttl;
    };

    explicit LiveKitTokenService(std::string api_key, std::string api_secret);

    std::string mint_token(const TokenRequest& req) const;

    /// Get or create E2EE key for a voice channel.
    /// @param channel The voice channel ID
    /// @param is_channel_empty If true and no key exists, creates a new unique key.
    ///                         If false and no key exists, creates a new unique key.
    /// @return base64-encoded 32-byte encryption key
    std::string get_or_create_e2ee_key(const ChannelId& channel, bool is_channel_empty);

    /// Clear E2EE key for a channel (call when last participant leaves).
    void clear_e2ee_key(const ChannelId& channel);

   private:
    std::string api_key_;
    std::string api_secret_;

    /// Cached E2EE keys per channel (active voice sessions only)
    mutable std::mutex e2ee_mutex_;
    std::unordered_map<ChannelId, std::string> e2ee_keys_;

    /// Generate a new unique E2EE key with timestamp
    std::string generate_unique_e2ee_key(const ChannelId& channel) const;
};

}  // namespace app::services::livekit

#endif  // APP_SERVICES_LIVEKIT_LIVEKITTOKENSERVICE_H_

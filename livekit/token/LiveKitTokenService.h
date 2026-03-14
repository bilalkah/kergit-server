#ifndef LIVEKIT_TOKEN_LIVEKITTOKENSERVICE_H_
#define LIVEKIT_TOKEN_LIVEKITTOKENSERVICE_H_

#include "domains/ids/Ids.h"

#include <chrono>
#include <cstdint>
#include <optional>
#include <string>

namespace livekit {

/**
 * LiveKitTokenService
 *
 * Stateless service responsible for generating LiveKit JWT tokens.
 *
 * Two token types exist:
 *
 * 1. Participant tokens
 *    Used by clients to join a voice room.
 *
 * 2. Admin tokens
 *    Used by backend services when calling LiveKit RoomService APIs.
 */
class LiveKitTokenService {
   public:
    struct ParticipantTokenRequest {
        UserId identity;
        ChannelId room;
        std::string node_id;
        uint64_t app_session_id = 0;
        std::string intent_nonce;

        bool can_publish;
        bool can_subscribe;

        std::chrono::seconds ttl;
    };

    struct AdminTokenRequest {
        std::optional<ChannelId> room;
        bool room_admin = false;
        bool room_create = false;
        bool room_list = false;
        std::chrono::seconds ttl = std::chrono::seconds(60);
    };

    explicit LiveKitTokenService(std::string api_key, std::string api_secret);

    /// Creates a participant token used by clients to join a room.
    std::string mint_participant_token(const ParticipantTokenRequest& req) const;

    /// Creates an admin token for RoomService API calls using the provided grant fields.
    std::string mint_admin_token(const AdminTokenRequest& req) const;

   private:
    std::string api_key_;
    std::string api_secret_;
};

}  // namespace livekit

#endif  // LIVEKIT_TOKEN_LIVEKITTOKENSERVICE_H_

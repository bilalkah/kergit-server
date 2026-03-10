#ifndef LIVEKIT_TOKEN_LIVEKITTOKENSERVICE_H_
#define LIVEKIT_TOKEN_LIVEKITTOKENSERVICE_H_

#include "domains/ids/Ids.h"

#include <chrono>
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

        bool can_publish;
        bool can_subscribe;

        std::chrono::seconds ttl;
    };

    explicit LiveKitTokenService(std::string api_key, std::string api_secret);

    /// Creates a participant token used by clients to join a room.
    std::string mint_participant_token(const ParticipantTokenRequest& req) const;

    /// Creates a short-lived admin token for RoomService API calls.
    std::string mint_admin_token(std::chrono::seconds ttl = std::chrono::seconds(60)) const;

   private:
    std::string api_key_;
    std::string api_secret_;
};

}  // namespace livekit

#endif  // LIVEKIT_TOKEN_LIVEKITTOKENSERVICE_H_
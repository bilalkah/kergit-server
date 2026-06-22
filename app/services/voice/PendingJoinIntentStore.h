#ifndef APP_SERVICES_VOICE_PENDINGJOININTENTSTORE_H_
#define APP_SERVICES_VOICE_PENDINGJOININTENTSTORE_H_

#include "app/managers/session/SessionId.h"
#include "domains/ids/Ids.h"
#include "infra/redis/RedisClient.h"

#include <cstdint>
#include <optional>
#include <string>

namespace app::services::voice {

/**
 * A client's staged intent to join (or switch to) a voice channel. Written on the
 * command path and correlated against the authoritative LiveKit join/leave webhooks.
 */
struct PendingJoinIntent {
    SessionId session_id = 0;
    std::string intent_nonce;

    ChannelId to_channel;
    ChannelId from_channel;
    bool has_from_channel = false;

    bool muted = false;
    bool deafened = false;

    bool old_leave_seen = false;
    bool new_join_seen = false;

    uint64_t expires_at_unix = 0;
};

/**
 * PendingJoinIntentStore
 *
 * Redis-backed store of pending join intents (one per user), with a short TTL tied to
 * the issued voice token's lifetime.
 */
class PendingJoinIntentStore {
   public:
    explicit PendingJoinIntentStore(infra::redis::RedisClient& redis);

    bool stage(const UserId& user, const PendingJoinIntent& intent, uint64_t expires_in_seconds);
    std::optional<PendingJoinIntent> read(const UserId& user) const;
    bool update(const UserId& user, const PendingJoinIntent& intent);
    void clear(const UserId& user);

   private:
    static std::string storage_key(const UserId& user);

    infra::redis::RedisClient& redis_;
};

}  // namespace app::services::voice

#endif  // APP_SERVICES_VOICE_PENDINGJOININTENTSTORE_H_

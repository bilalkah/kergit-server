#ifndef APP_SERVICES_VOICE_VOICERESUMEREGISTRY_H_
#define APP_SERVICES_VOICE_VOICERESUMEREGISTRY_H_

#include "domains/ids/Ids.h"
#include "infra/redis/RedisClient.h"

#include <chrono>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>

namespace app::services::voice {

/**
 * VoiceResumeRegistry
 *
 * Per-user one-time "resume id": a short-lived token that lets a reconnecting client
 * reclaim its voice ownership. Held both in memory (authoritative for the running
 * process) and in Redis (so it survives a server restart).
 */
class VoiceResumeRegistry {
   public:
    explicit VoiceResumeRegistry(infra::redis::RedisClient& redis);

    /// Generate, store, persist and return a fresh resume id for the user.
    std::string rotate(const UserId& user);

    /// Current in-memory resume id, if any.
    std::optional<std::string> read(const UserId& user) const;

    /// Load the resume id from Redis into memory (recovery); returns it if present.
    std::optional<std::string> load_from_storage(const UserId& user);

    /// Drop the resume id from memory and Redis.
    void clear(const UserId& user);

    /// Drop all in-memory resume ids (used on restart recovery before reloading).
    void clear_all();

   private:
    static std::string storage_key(const UserId& user);

    infra::redis::RedisClient& redis_;
    mutable std::mutex mutex_;
    std::unordered_map<UserId, std::string> user_resume_ids_;
    static constexpr std::chrono::seconds kTtl{24 * 60 * 60};
};

}  // namespace app::services::voice

#endif  // APP_SERVICES_VOICE_VOICERESUMEREGISTRY_H_

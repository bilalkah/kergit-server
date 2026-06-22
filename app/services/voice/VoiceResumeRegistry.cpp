#include "app/services/voice/VoiceResumeRegistry.h"

#include "app/services/voice/VoiceNonce.h"
#include "utils/EventLogger.h"

#include <string>

namespace app::services::voice {

VoiceResumeRegistry::VoiceResumeRegistry(infra::redis::RedisClient& redis) : redis_(redis) {}

std::string VoiceResumeRegistry::storage_key(const UserId& user) {
    return "voice:resume_id:" + user.value;
}

std::string VoiceResumeRegistry::rotate(const UserId& user) {
    const auto next = generate_nonce_hex();
    {
        std::lock_guard lock(mutex_);
        user_resume_ids_[user] = next;
    }

    try {
        redis_.setex(storage_key(user), kTtl, next);
    } catch (const std::exception& ex) {
        utils::EventLogger::instance().log(utils::EventCategory::VOICE, user.value,
                                           "resume_id_persist_failed", 0,
                                           std::string("error=") + ex.what());
    }

    return next;
}

std::optional<std::string> VoiceResumeRegistry::load_from_storage(const UserId& user) {
    try {
        const auto stored = redis_.get(storage_key(user));
        if (!stored.has_value() || stored->empty()) return std::nullopt;

        std::lock_guard lock(mutex_);
        user_resume_ids_[user] = *stored;
        return stored;
    } catch (const std::exception& ex) {
        utils::EventLogger::instance().log(utils::EventCategory::VOICE, user.value,
                                           "resume_id_load_failed", 0,
                                           std::string("error=") + ex.what());
        return std::nullopt;
    }
}

std::optional<std::string> VoiceResumeRegistry::read(const UserId& user) const {
    std::lock_guard lock(mutex_);
    auto it = user_resume_ids_.find(user);
    if (it == user_resume_ids_.end()) return std::nullopt;
    return it->second;
}

void VoiceResumeRegistry::clear(const UserId& user) {
    {
        std::lock_guard lock(mutex_);
        user_resume_ids_.erase(user);
    }

    try {
        redis_.del(storage_key(user));
    } catch (const std::exception& ex) {
        utils::EventLogger::instance().log(utils::EventCategory::VOICE, user.value,
                                           "resume_id_clear_failed", 0,
                                           std::string("error=") + ex.what());
    }
}

void VoiceResumeRegistry::clear_all() {
    std::lock_guard lock(mutex_);
    user_resume_ids_.clear();
}

}  // namespace app::services::voice

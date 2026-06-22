#include "app/services/voice/PendingJoinIntentStore.h"

#include "utils/EventLogger.h"

#include <chrono>
#include <nlohmann/json.hpp>

namespace app::services::voice {
namespace {
using json = nlohmann::json;

uint64_t unix_now_seconds() {
    return static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::seconds>(
                                     std::chrono::system_clock::now().time_since_epoch())
                                     .count());
}
}  // namespace

PendingJoinIntentStore::PendingJoinIntentStore(infra::redis::RedisClient& redis) : redis_(redis) {}

std::string PendingJoinIntentStore::storage_key(const UserId& user) {
    return "voice:pending_join:" + user.value;
}

bool PendingJoinIntentStore::stage(const UserId& user, const PendingJoinIntent& intent,
                                   uint64_t expires_in_seconds) {
    const auto ttl = std::chrono::seconds(expires_in_seconds + 10);

    try {
        json doc;
        doc["user_id"] = user.value;
        doc["session_id"] = intent.session_id;
        doc["intent_nonce"] = intent.intent_nonce;
        doc["to_channel"] = intent.to_channel.value;
        doc["from_channel"] = intent.has_from_channel ? intent.from_channel.value : "";
        doc["has_from_channel"] = intent.has_from_channel;
        doc["muted"] = intent.muted;
        doc["deafened"] = intent.deafened;
        doc["old_leave_seen"] = intent.old_leave_seen;
        doc["new_join_seen"] = intent.new_join_seen;
        doc["expires_at_unix"] = unix_now_seconds() + static_cast<uint64_t>(ttl.count());

        redis_.setex(storage_key(user), ttl, doc.dump());
        return true;
    } catch (const std::exception& ex) {
        utils::EventLogger::instance().log(utils::EventCategory::VOICE, user.value,
                                           "pending_join_write_failed", 0,
                                           std::string("error=") + ex.what());
        return false;
    }
}

std::optional<PendingJoinIntent> PendingJoinIntentStore::read(const UserId& user) const {
    try {
        const auto raw = redis_.get(storage_key(user));
        if (!raw.has_value()) return std::nullopt;

        const auto doc = json::parse(*raw, nullptr, false);
        if (!doc.is_object()) return std::nullopt;

        PendingJoinIntent intent;

        if (doc.contains("session_id")) {
            if (doc["session_id"].is_number_unsigned()) {
                intent.session_id = static_cast<SessionId>(doc["session_id"].get<uint64_t>());
            } else if (doc["session_id"].is_string()) {
                intent.session_id =
                    static_cast<SessionId>(std::stoull(doc["session_id"].get<std::string>()));
            }
        }

        if (doc.contains("intent_nonce") && doc["intent_nonce"].is_string()) {
            intent.intent_nonce = doc["intent_nonce"].get<std::string>();
        }

        if (!doc.contains("to_channel") || !doc["to_channel"].is_string()) {
            return std::nullopt;
        }
        intent.to_channel = ChannelId(doc["to_channel"].get<std::string>());

        if (doc.contains("has_from_channel") && doc["has_from_channel"].is_boolean()) {
            intent.has_from_channel = doc["has_from_channel"].get<bool>();
        }
        if (doc.contains("from_channel") && doc["from_channel"].is_string()) {
            const auto from = doc["from_channel"].get<std::string>();
            if (!from.empty()) {
                intent.from_channel = ChannelId(from);
                intent.has_from_channel = true;
            }
        }

        if (doc.contains("muted") && doc["muted"].is_boolean()) {
            intent.muted = doc["muted"].get<bool>();
        }
        if (doc.contains("deafened") && doc["deafened"].is_boolean()) {
            intent.deafened = doc["deafened"].get<bool>();
        }
        if (doc.contains("old_leave_seen") && doc["old_leave_seen"].is_boolean()) {
            intent.old_leave_seen = doc["old_leave_seen"].get<bool>();
        }
        if (doc.contains("new_join_seen") && doc["new_join_seen"].is_boolean()) {
            intent.new_join_seen = doc["new_join_seen"].get<bool>();
        }
        if (doc.contains("expires_at_unix") && doc["expires_at_unix"].is_number_unsigned()) {
            intent.expires_at_unix = doc["expires_at_unix"].get<uint64_t>();
        }

        return intent;
    } catch (...) {
        return std::nullopt;
    }
}

bool PendingJoinIntentStore::update(const UserId& user, const PendingJoinIntent& intent) {
    try {
        const auto ttl = redis_.ttl(storage_key(user));
        if (!ttl.has_value() || ttl->count() <= 0) {
            return false;
        }

        json doc;
        doc["user_id"] = user.value;
        doc["session_id"] = intent.session_id;
        doc["intent_nonce"] = intent.intent_nonce;
        doc["to_channel"] = intent.to_channel.value;
        doc["from_channel"] = intent.has_from_channel ? intent.from_channel.value : "";
        doc["has_from_channel"] = intent.has_from_channel;
        doc["muted"] = intent.muted;
        doc["deafened"] = intent.deafened;
        doc["old_leave_seen"] = intent.old_leave_seen;
        doc["new_join_seen"] = intent.new_join_seen;
        doc["expires_at_unix"] = intent.expires_at_unix;

        redis_.setex(storage_key(user), *ttl, doc.dump());
        return true;
    } catch (...) {
        return false;
    }
}

void PendingJoinIntentStore::clear(const UserId& user) {
    try {
        redis_.del(storage_key(user));
    } catch (...) {
    }
}

}  // namespace app::services::voice

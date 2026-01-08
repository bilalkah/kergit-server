#include "infra/security/validation/ProtoMessageValidator.h"

#include <algorithm>

namespace infra::security::validation {

// ---------- ctor ----------

ProtoMessageValidator::ProtoMessageValidator() { load_profanity_filter(); }

// ---------- entry point ----------

std::expected<void, ValidationError> ProtoMessageValidator::validate_envelope(
    const sercom::protocol::Envelope& env) {
    if (env.version() != 1) {
        return std::unexpected("Unsupported protocol version");
    }

    switch (env.type()) {
        case sercom::protocol::Envelope::CHAT: {
            sercom::protocol::chat::ChatMessage msg;
            if (!msg.ParseFromArray(env.payload().data(), env.payload().size())) {
                return std::unexpected("Invalid CHAT payload");
            }
            return validate_chat(msg);
        }

        default:
            return std::unexpected("Unsupported message type");
    }
}

// ---------- CHAT validation ----------

std::expected<void, ValidationError> ProtoMessageValidator::validate_chat(
    const sercom::protocol::chat::ChatMessage& msg) {
    if (msg.channel_id().empty()) {
        return std::unexpected("channel_id is empty");
    }

    if (msg.content().empty()) {
        return std::unexpected("content is empty");
    }

    if (msg.content().size() > MAX_CHAT_LENGTH) {
        return std::unexpected("message too long");
    }

    if (contains_profanity(msg.content())) {
        return std::unexpected("profanity detected");
    }

    return {};
}

// ---------- helpers ----------

bool ProtoMessageValidator::contains_pattern(
    std::string_view text, const std::unordered_set<std::string>& patterns) const {
    for (const auto& p : patterns) {
        if (text.find(p) != std::string_view::npos) return true;
    }
    return false;
}

bool ProtoMessageValidator::contains_profanity(std::string_view text) const {
    std::string lowered(text);
    std::transform(lowered.begin(), lowered.end(), lowered.begin(), ::tolower);
    return contains_pattern(lowered, profanity_words_);
}

void ProtoMessageValidator::load_profanity_filter() {
    profanity_words_ = {"spam", "scam", "fraud", "phishing"};
}

}  // namespace infra::security::validation

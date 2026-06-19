// AuditRepository.cpp

#include "infra/persistence/repositories/AuditRepository.h"

#include "infra/persistence/repositories/RepositorySchema.h"

#include <optional>
#include <stdexcept>
#include <string>

namespace {

std::optional<std::string> id_param(const std::optional<UserId>& value) {
    if (!value.has_value()) return std::nullopt;
    return value->value;
}

std::optional<std::string> id_param(const std::optional<HubId>& value) {
    if (!value.has_value()) return std::nullopt;
    return value->value;
}

std::optional<std::string> id_param(const std::optional<ChannelId>& value) {
    if (!value.has_value()) return std::nullopt;
    return value->value;
}

std::optional<std::string> id_param(const std::optional<MessageId>& value) {
    if (!value.has_value()) return std::nullopt;
    return value->value;
}

std::optional<std::string> int_param(const std::optional<int>& value) {
    if (!value.has_value()) return std::nullopt;
    return std::to_string(*value);
}

std::string with_default(const std::string& value, const char* fallback) {
    if (value.empty()) return fallback;
    return value;
}

std::string metadata_dump(const nlohmann::json& metadata) {
    if (!metadata.is_object()) {
        return "{}";
    }

    return metadata.dump();
}

void validate_event(const AuditRepository::Event& event) {
    if (event.category.empty()) {
        throw std::invalid_argument("Audit event category is required");
    }

    if (event.event_type.empty()) {
        throw std::invalid_argument("Audit event type is required");
    }
}

}  // namespace

void AuditRepository::logEvent(const Event& event) {
    validate_event(event);

    db_.write("AuditRepository.logEvent", [&](pqxx::work& txn) {
        const std::string query = std::string{"INSERT INTO "} + dbschema::kAuditEvents +
                                  " ("
                                  "category, "
                                  "event_type, "
                                  "severity, "
                                  "actor_type, "
                                  "actor_user_id, "
                                  "target_user_id, "
                                  "hub_id, "
                                  "channel_id, "
                                  "message_id, "
                                  "attachment_id, "
                                  "invite_id, "
                                  "request_id, "
                                  "session_id, "
                                  "connection_id, "
                                  "server_node_id, "
                                  "error_code, "
                                  "status_code, "
                                  "metadata"
                                  ") "
                                  "VALUES ("
                                  "$1, "
                                  "$2, "
                                  "$3, "
                                  "$4, "
                                  "$5::uuid, "
                                  "$6::uuid, "
                                  "$7::uuid, "
                                  "$8::uuid, "
                                  "$9::uuid, "
                                  "$10::uuid, "
                                  "$11::uuid, "
                                  "$12, "
                                  "$13, "
                                  "$14, "
                                  "$15, "
                                  "$16, "
                                  "$17::integer, "
                                  "$18::jsonb"
                                  ")";

        txn.exec(query, pqxx::params{
                            event.category,
                            event.event_type,
                            with_default(event.severity, "info"),
                            with_default(event.actor_type, "user"),
                            id_param(event.actor_user_id),
                            id_param(event.target_user_id),
                            id_param(event.hub_id),
                            id_param(event.channel_id),
                            id_param(event.message_id),
                            event.attachment_id,
                            event.invite_id,
                            event.request_id,
                            event.session_id,
                            event.connection_id,
                            event.server_node_id,
                            event.error_code,
                            int_param(event.status_code),
                            metadata_dump(event.metadata),
                        });
    });
}

int AuditRepository::purgeOldEvents(int limit) {
    if (limit <= 0) {
        return 0;
    }

    return db_.write("AuditRepository.purgeOldEvents", [&](pqxx::work& txn) {
        const std::string query =
            std::string{"SELECT "} + dbschema::kPurgeOldAuditEvents + "($1::integer)";

        auto res = txn.exec(query, pqxx::params{limit});
        if (res.empty()) {
            return 0;
        }

        return res[0][0].as<int>(0);
    });
}

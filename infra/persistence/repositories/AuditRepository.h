#ifndef INFRA_PERSISTENCE_REPOSITORIES_AUDIT_REPOSITORY_H
#define INFRA_PERSISTENCE_REPOSITORIES_AUDIT_REPOSITORY_H

#include "domains/ids/Ids.h"
#include "infra/persistence/DatabaseExecutor.h"

#include <nlohmann/json.hpp>
#include <optional>
#include <string>

// Product-level audit and technical event writer.
//
// Important:
// - Do not write message content.
// - Do not write file content.
// - Do not write passwords.
// - Do not write access/refresh tokens.
// - Do not write request bodies.
// - Do not write cookie/header dumps.
// - Metadata must stay compact and non-sensitive.
//
// Durable user-visible state belongs in normal tables.
// This repository is only for operational/security/product events.
class AuditRepository {
   public:
    explicit AuditRepository(DatabaseExecutor& db) : db_(db) {}

    struct Event {
        std::string category;
        std::string event_type;
        std::string severity = "info";

        std::string actor_type = "user";
        std::optional<UserId> actor_user_id;
        std::optional<UserId> target_user_id;

        std::optional<HubId> hub_id;
        std::optional<ChannelId> channel_id;
        std::optional<MessageId> message_id;

        std::optional<std::string> attachment_id;
        std::optional<std::string> invite_id;

        std::optional<std::string> request_id;
        std::optional<std::string> session_id;
        std::optional<std::string> connection_id;

        std::optional<std::string> ip;
        std::optional<std::string> user_agent;
        std::optional<std::string> server_node_id;

        std::optional<std::string> error_code;
        std::optional<int> status_code;

        nlohmann::json metadata = nlohmann::json::object();
    };

    void logEvent(const Event& event);

    // Deletes expired audit events using the DB-side retention function.
    // Expected to be called periodically by server maintenance code.
    int purgeOldEvents(int limit = 5000);

   private:
    DatabaseExecutor& db_;
};

#endif  // INFRA_PERSISTENCE_REPOSITORIES_AUDIT_REPOSITORY_H

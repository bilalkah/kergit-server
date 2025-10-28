#include "core/database/src/chatdb.h"

#include <stdexcept>

ChatDB::ChatDB(const std::string& conninfo) : conn_(conninfo) {
    if (!conn_.is_open()) throw std::runtime_error("Failed to open database connection");
}

std::string ChatDB::createHub(const std::string& hubName, const std::string& ownerUuid) {
    pqxx::work txn(conn_);
    auto res = txn.exec_params(
        "INSERT INTO public.hubs (name, owner_id) VALUES ($1, $2::uuid) RETURNING id::text",
        hubName, ownerUuid);
    if (res.empty()) throw std::runtime_error("createHub failed");
    std::string hubId = res[0][0].as<std::string>();
    txn.commit();
    return hubId;
}

void ChatDB::addMember(const std::string& hubId, const std::string& userUuid,
                       const std::string& role) {
    pqxx::work txn(conn_);
    txn.exec_params(
        "INSERT INTO public.hub_members (hub_id, user_id, role) "
        "VALUES ($1::uuid, $2::uuid, $3) "
        "ON CONFLICT (hub_id, user_id) DO UPDATE SET role = EXCLUDED.role",
        hubId, userUuid, role);
    txn.commit();
}

void ChatDB::removeMember(const std::string& hubId, const std::string& userUuid) {
    pqxx::work txn(conn_);
    txn.exec_params("DELETE FROM public.hub_members WHERE hub_id = $1::uuid AND user_id = $2::uuid",
                    hubId, userUuid);
    txn.commit();
}

std::string ChatDB::createChannel(const std::string& hubId, const std::string& channelName,
                                  const std::string& type) {
    pqxx::work txn(conn_);
    auto res = txn.exec_params(
        "INSERT INTO public.channels (hub_id, name, type) VALUES ($1::uuid, $2, $3) RETURNING "
        "id::text",
        hubId, channelName, type);
    if (res.empty()) throw std::runtime_error("createChannel failed");
    std::string channelId = res[0][0].as<std::string>();
    txn.commit();
    return channelId;
}

void ChatDB::sendMessage(const std::string& channelId, const std::string& senderUuid,
                         const std::string& content) {
    pqxx::work txn(conn_);
    txn.exec_params(
        "INSERT INTO public.messages (channel_id, sender_id, content) "
        "VALUES ($1::uuid, $2::uuid, $3)",
        channelId, senderUuid, content);
    txn.commit();
}

std::vector<DbMessage> ChatDB::fetchMessages(const std::string& channelId, int limit) {
    pqxx::work txn(conn_);
    auto res = txn.exec_params(
        "SELECT id::text, channel_id::text, sender_id::text, content, created_at "
        "FROM public.messages WHERE channel_id = $1::uuid "
        "ORDER BY created_at DESC LIMIT $2",
        channelId, limit);

    std::vector<DbMessage> msgs;
    msgs.reserve(res.size());
    for (const auto& row : res) {
        msgs.push_back({row[0].as<std::string>(), row[1].as<std::string>(),
                        row[2].as<std::string>(), row[3].as<std::string>(),
                        row[4].as<std::string>()});
    }
    return msgs;
}

pqxx::connection& ChatDB::getConnection() { return conn_; }

std::vector<HubInfo> ChatDB::getUserHubs(const UserId& userUuid) {
    pqxx::work txn(conn_);
    auto res = txn.exec_params(
        "SELECT h.id::text, h.name "
        "FROM public.hubs h "
        "JOIN public.hub_members m ON h.id = m.hub_id "
        "WHERE m.user_id = $1::uuid ORDER BY h.created_at DESC",
        userUuid.value);

    std::vector<HubInfo> hubs;
    hubs.reserve(res.size());
    for (const auto& row : res) {
        hubs.push_back({row[0].as<std::string>(), row[1].as<std::string>()});
    }
    return hubs;
}

std::vector<ChannelInfo> ChatDB::getHubChannels(const std::string& hubId) {
    pqxx::work txn(conn_);
    auto res = txn.exec_params(
        "SELECT id::text, hub_id::text, name, type "
        "FROM public.channels WHERE hub_id = $1::uuid ORDER BY created_at ASC",
        hubId);

    std::vector<ChannelInfo> chans;
    chans.reserve(res.size());
    for (const auto& row : res) {
        chans.push_back({row[0].as<std::string>(), row[1].as<std::string>(),
                         row[2].as<std::string>(), row[3].as<std::string>()});
    }
    return chans;
}

std::string ChatDB::ensurePersonalHubWithGeneral(const std::string& ownerUuid,
                                                 const std::string& hubName) {
    pqxx::work txn(conn_);
    auto existing = txn.exec_params(
        "SELECT id::text FROM public.hubs WHERE owner_id = $1::uuid LIMIT 1", ownerUuid);

    std::string hubId;
    if (existing.empty()) {
        auto res = txn.exec_params(
            "INSERT INTO public.hubs (name, owner_id) VALUES ($1, $2::uuid) RETURNING id::text",
            hubName, ownerUuid);
        hubId = res[0][0].as<std::string>();
    } else {
        hubId = existing[0][0].as<std::string>();
        txn.exec_params(
            "INSERT INTO public.hub_members (hub_id, user_id, role) "
            "VALUES ($1::uuid, $2::uuid, 'member') ON CONFLICT DO NOTHING",
            hubId, ownerUuid);
    }

    // ensure "general" exists
    auto ch = txn.exec_params(
        "SELECT 1 FROM public.channels WHERE hub_id = $1::uuid AND name = 'general' LIMIT 1",
        hubId);
    if (ch.empty()) {
        txn.exec_params(
            "INSERT INTO public.channels (hub_id, name, type) VALUES ($1::uuid, 'general', 'text')",
            hubId);
    }

    txn.commit();
    return hubId;
}

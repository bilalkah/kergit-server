#include "core/database/src/chatdb.h"

#include <stdexcept>

ChatDB::ChatDB(const std::string& conninfo) : conn_(conninfo) {
    if (!conn_.is_open()) {
        throw std::runtime_error("Failed to open database connection");
    }
}

int ChatDB::createHub(const std::string& hubName, int ownerId) {
    pqxx::work txn(conn_);
    auto result = txn.exec(pqxx::zview(
                               "INSERT INTO hubs (name, owner_id) VALUES ($1, $2) RETURNING id"),
                           pqxx::params(hubName, ownerId));
    int hubId = result[0][0].as<int>();

    txn.exec(pqxx::zview(
                 "INSERT INTO hub_members (hub_id, user_id, role) VALUES ($1, $2, 'owner')"),
             pqxx::params(hubId, ownerId));

    txn.commit();
    return hubId;
}

void ChatDB::addMember(int hubId, int userId) {
    pqxx::work txn(conn_);
    txn.exec(
        pqxx::zview(
            "INSERT INTO hub_members (hub_id, user_id, role) VALUES ($1, $2, 'member') ON "
            "CONFLICT DO NOTHING"),
        pqxx::params(hubId, userId));
    txn.commit();
}

void ChatDB::removeMember(int hubId, int userId) {
    pqxx::work txn(conn_);
    txn.exec(pqxx::zview("DELETE FROM hub_members WHERE hub_id = $1 AND user_id = $2"),
             pqxx::params(hubId, userId));
    txn.commit();
}

int ChatDB::createChannel(int hubId, const std::string& channelName, const std::string& type) {
    if (type != "text" && type != "voice") {
        throw std::invalid_argument("Channel type must be 'text' or 'voice'");
    }

    pqxx::work txn(conn_);
    auto result = txn.exec(
        pqxx::zview("INSERT INTO channels (hub_id, name, type) VALUES ($1, $2, $3) RETURNING id"),
        pqxx::params(hubId, channelName, type));
    int channelId = result[0][0].as<int>();
    txn.commit();
    return channelId;
}

void ChatDB::sendMessage(int channelId, int senderId, const std::string& content) {
    pqxx::work txn(conn_);
    txn.exec(
        pqxx::zview("INSERT INTO messages (channel_id, sender_id, content) VALUES ($1, $2, $3)"),
        pqxx::params(channelId, senderId, content));
    txn.commit();
}

std::vector<DbMessage> ChatDB::fetchMessages(int channelId, int limit) {
    pqxx::work txn(conn_);
    auto result = txn.exec(
        pqxx::zview(
            "SELECT id, channel_id, sender_id, content, sent_at FROM messages WHERE channel_id "
            "= $1 ORDER BY sent_at DESC LIMIT $2"),
        pqxx::params(channelId, limit));

    std::vector<DbMessage> messages;
    for (auto row : result) {
        messages.push_back({row[0].as<int>(), row[1].as<int>(), row[2].as<int>(),
                            row[3].as<std::string>(), row[4].as<std::string>()});
    }
    return messages;
}

pqxx::connection& ChatDB::getConnection() {
    return conn_;
}

std::optional<int> ChatDB::findUserIdByUsername(const std::string& username) {
    pqxx::work txn(conn_);
    auto res = txn.exec(pqxx::zview("SELECT id FROM users WHERE username = $1"),
                        pqxx::params(username));
    if (res.empty()) return std::nullopt;
    return res[0][0].as<int>();
}

std::optional<std::string> ChatDB::findPasswordHashByUsername(const std::string& username) {
    pqxx::work txn(conn_);
    auto res = txn.exec(
        pqxx::zview("SELECT password_hash FROM users WHERE username = $1"), pqxx::params(username));
    if (res.empty()) return std::nullopt;
    return res[0][0].as<std::string>();
}

int ChatDB::createUser(const std::string& username, const std::string& passwordHash,
                       const std::string& email) {
    pqxx::work txn(conn_);
    auto res = txn.exec(
        pqxx::zview(
            "INSERT INTO users (username, password_hash, email) VALUES ($1, $2, $3) RETURNING id"),
        pqxx::params(username, passwordHash, email));
    int id = res[0][0].as<int>();
    txn.commit();
    return id;
}

std::vector<HubInfo> ChatDB::getUserHubs(int userId) {
    pqxx::work txn(conn_);
    auto res = txn.exec(
        pqxx::zview(
            "SELECT h.id, h.name FROM hubs h JOIN hub_members m ON h.id = m.hub_id WHERE m.user_"
            "id = $1 ORDER BY h.id"),
        pqxx::params(userId));
    std::vector<HubInfo> hubs;
    hubs.reserve(res.size());
    for (auto row : res) {
        hubs.push_back({row[0].as<int>(), row[1].as<std::string>()});
    }
    return hubs;
}

std::vector<ChannelInfo> ChatDB::getHubChannels(int hubId) {
    pqxx::work txn(conn_);
    auto res = txn.exec(
        pqxx::zview("SELECT id, hub_id, name, type FROM channels WHERE hub_id = $1 ORDER BY id"),
        pqxx::params(hubId));
    std::vector<ChannelInfo> chans;
    chans.reserve(res.size());
    for (auto row : res) {
        chans.push_back({row[0].as<int>(), row[1].as<int>(), row[2].as<std::string>(),
                         row[3].as<std::string>()});
    }
    return chans;
}

int ChatDB::ensurePersonalHubWithGeneral(int userId, const std::string& hubName) {
    pqxx::work txn(conn_);

    // Check if user already owns a hub
    auto res = txn.exec(
        pqxx::zview("SELECT id FROM hubs WHERE owner_id = $1 ORDER BY id LIMIT 1"),
        pqxx::params(userId));
    int hubId;
    if (res.empty()) {
        auto r2 = txn.exec(
            pqxx::zview("INSERT INTO hubs (name, owner_id) VALUES ($1, $2) RETURNING id"),
            pqxx::params(hubName, userId));
        hubId = r2[0][0].as<int>();
        txn.exec(
            pqxx::zview("INSERT INTO hub_members (hub_id, user_id, role) VALUES ($1, $2, 'owner')"),
            pqxx::params(hubId, userId));
    } else {
        hubId = res[0][0].as<int>();
        // Ensure membership exists at least as member
        txn.exec(
            pqxx::zview(
                "INSERT INTO hub_members (hub_id, user_id, role) VALUES ($1, $2, 'member') ON "
                "CONFLICT DO NOTHING"),
            pqxx::params(hubId, userId));
    }

    // Ensure a 'general' text channel exists
    auto ch = txn.exec(pqxx::zview("SELECT id FROM channels WHERE hub_id = $1 AND name = 'general'"),
                       pqxx::params(hubId));
    if (ch.empty()) {
        txn.exec(
            pqxx::zview("INSERT INTO channels (hub_id, name, type) VALUES ($1, 'general', 'text')"),
            pqxx::params(hubId));
    }

    txn.commit();
    return hubId;
}

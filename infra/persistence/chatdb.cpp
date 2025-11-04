#include "infra/persistence/chatdb.h"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <stdexcept>

namespace {

Role role_from_string(const std::string& role) {
    std::string lower = role;
    std::transform(lower.begin(), lower.end(), lower.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    if (lower == "owner") return Role::OWNER;
    if (lower == "admin") return Role::ADMIN;
    return Role::USER;
}

ChannelType channel_type_from_string(const std::string& type) {
    std::string lower = type;
    std::transform(lower.begin(), lower.end(), lower.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return lower == "voice" ? ChannelType::VOICE : ChannelType::CHAT;
}

std::chrono::system_clock::time_point parse_timestamp(const std::string& ts) {
    if (ts.empty()) return {};
    std::string trimmed = ts;
    auto dot = trimmed.find('.');
    if (dot != std::string::npos) {
        trimmed = trimmed.substr(0, dot);
    }
    std::tm tm{};
    std::istringstream ss(trimmed);
    ss >> std::get_time(&tm, "%Y-%m-%d %H:%M:%S");
    if (ss.fail()) return {};
    tm.tm_isdst = -1;
    std::time_t time = std::mktime(&tm);
    if (time == static_cast<std::time_t>(-1)) return {};
    return std::chrono::system_clock::from_time_t(time);
}

}  // namespace

ChatDB::ChatDB(const std::string& conninfo) : conn_(conninfo) {
    if (!conn_.is_open()) throw std::runtime_error("Failed to open database connection");
}

HubId ChatDB::createHub(const std::string& hubName, const UserId& ownerUuid) {
    pqxx::work txn(conn_);
    auto res = txn.exec_params(
        "INSERT INTO public.hubs (name, owner_id) VALUES ($1, $2::uuid) RETURNING id::text",
        hubName, ownerUuid.value);
    if (res.empty()) throw std::runtime_error("createHub failed");
    HubId hubId{res[0][0].as<std::string>()};
    txn.commit();
    return hubId;
}

void ChatDB::addMember(const HubId& hubId, const UserId& userUuid,
                       const std::string& role) {
    pqxx::work txn(conn_);
    txn.exec_params(
        "INSERT INTO public.hub_members (hub_id, user_id, role) "
        "VALUES ($1::uuid, $2::uuid, $3) "
        "ON CONFLICT (hub_id, user_id) DO UPDATE SET role = EXCLUDED.role",
        hubId.value, userUuid.value, role);
    txn.commit();
}

void ChatDB::removeMember(const HubId& hubId, const UserId& userUuid) {
    pqxx::work txn(conn_);
    txn.exec_params("DELETE FROM public.hub_members WHERE hub_id = $1::uuid AND user_id = $2::uuid",
                    hubId.value, userUuid.value);
    txn.commit();
}

ChannelId ChatDB::createChannel(const HubId& hubId, const std::string& channelName,
                                const std::string& type) {
    pqxx::work txn(conn_);
    auto res = txn.exec_params(
        "INSERT INTO public.channels (hub_id, name, type) VALUES ($1::uuid, $2, $3) RETURNING "
        "id::text",
        hubId.value, channelName, type);
    if (res.empty()) throw std::runtime_error("createChannel failed");
    ChannelId channelId{res[0][0].as<std::string>()};
    txn.commit();
    return channelId;
}

Message ChatDB::sendMessage(const ChannelId& channelId, const UserId& senderUuid,
                            const std::string& content) {
    pqxx::work txn(conn_);
    auto res = txn.exec_params(
        "INSERT INTO public.messages (channel_id, sender_id, content) "
        "VALUES ($1::uuid, $2::uuid, $3) "
        "RETURNING id::text, channel_id::text, sender_id::text, content, created_at",
        channelId.value, senderUuid.value, content);
    if (res.empty()) throw std::runtime_error("sendMessage failed");
    Message msg;
    msg.id = MessageId{res[0][0].as<std::string>()};
    msg.channel_id = ChannelId{res[0][1].as<std::string>()};
    msg.sender_id = UserId{res[0][2].as<std::string>()};
    msg.text = res[0][3].as<std::string>();
    msg.sent_at = parse_timestamp(res[0][4].as<std::string>());
    txn.commit();
    return msg;
}

std::vector<Message> ChatDB::fetchMessages(const ChannelId& channelId, int limit) {
    pqxx::work txn(conn_);
    auto res = txn.exec_params(
        "SELECT id::text, channel_id::text, sender_id::text, content, created_at "
        "FROM public.messages WHERE channel_id = $1::uuid "
        "ORDER BY created_at DESC LIMIT $2",
        channelId.value, limit);

    std::vector<Message> msgs;
    msgs.reserve(res.size());
    for (const auto& row : res) {
        Message msg;
        msg.id = MessageId{row[0].as<std::string>()};
        msg.channel_id = ChannelId{row[1].as<std::string>()};
        msg.sender_id = UserId{row[2].as<std::string>()};
        msg.text = row[3].as<std::string>();
        msg.sent_at = parse_timestamp(row[4].as<std::string>());
        msgs.push_back(std::move(msg));
    }
    return msgs;
}

pqxx::connection& ChatDB::getConnection() { return conn_; }

std::vector<Hub> ChatDB::getUserHubs(const UserId& userUuid) {
    pqxx::work txn(conn_);
    auto res = txn.exec_params(
        "SELECT h.id::text, h.name, h.owner_id::text, m.role "
        "FROM public.hubs h "
        "JOIN public.hub_members m ON h.id = m.hub_id "
        "WHERE m.user_id = $1::uuid ORDER BY h.created_at DESC",
        userUuid.value);

    std::vector<Hub> hubs;
    hubs.reserve(res.size());
    for (const auto& row : res) {
        Hub hub(row[1].as<std::string>(), HubId{row[0].as<std::string>()},
                UserId{row[2].as<std::string>()});
        hub.setMemberRole(userUuid, role_from_string(row[3].as<std::string>()));
        hubs.push_back(std::move(hub));
    }
    return hubs;
}

std::vector<Channel> ChatDB::getHubChannels(const HubId& hubId) {
    pqxx::work txn(conn_);
    auto res = txn.exec_params(
        "SELECT id::text, hub_id::text, name, type "
        "FROM public.channels WHERE hub_id = $1::uuid ORDER BY created_at ASC",
        hubId.value);

    std::vector<Channel> chans;
    chans.reserve(res.size());
    for (const auto& row : res) {
        Channel channel(row[2].as<std::string>(), ChannelId{row[0].as<std::string>()},
                        HubId{row[1].as<std::string>()},
                        channel_type_from_string(row[3].as<std::string>()));
        chans.push_back(std::move(channel));
    }
    return chans;
}

std::optional<Channel> ChatDB::getChannel(const ChannelId& channelId) {
    pqxx::work txn(conn_);
    auto res = txn.exec_params(
        "SELECT id::text, hub_id::text, name, type "
        "FROM public.channels WHERE id = $1::uuid LIMIT 1",
        channelId.value);
    if (res.empty()) return std::nullopt;
    const auto& row = res[0];
    return Channel{row[2].as<std::string>(), ChannelId{row[0].as<std::string>()},
                   HubId{row[1].as<std::string>()},
                   channel_type_from_string(row[3].as<std::string>())};
}

bool ChatDB::isHubMember(const HubId& hubId, const UserId& userUuid) {
    pqxx::work txn(conn_);
    auto res = txn.exec_params(
        "SELECT 1 FROM public.hub_members WHERE hub_id = $1::uuid AND user_id = $2::uuid LIMIT 1",
        hubId.value, userUuid.value);
    return !res.empty();
}

std::optional<Role> ChatDB::getMembershipRole(const HubId& hubId, const UserId& userUuid) {
    pqxx::work txn(conn_);
    auto res = txn.exec_params(
        "SELECT role FROM public.hub_members WHERE hub_id = $1::uuid AND user_id = $2::uuid LIMIT 1",
        hubId.value, userUuid.value);
    if (res.empty()) return std::nullopt;
    return role_from_string(res[0][0].as<std::string>());
}

HubId ChatDB::ensurePersonalHubWithGeneral(const UserId& ownerUuid,
                                           const std::string& hubName) {
    pqxx::work txn(conn_);
    auto existing = txn.exec_params(
        "SELECT id::text FROM public.hubs WHERE owner_id = $1::uuid LIMIT 1", ownerUuid.value);

    std::string hubId;
    if (existing.empty()) {
        auto res = txn.exec_params(
            "INSERT INTO public.hubs (name, owner_id) VALUES ($1, $2::uuid) RETURNING id::text",
            hubName, ownerUuid.value);
        hubId = res[0][0].as<std::string>();
    } else {
        hubId = existing[0][0].as<std::string>();
        txn.exec_params(
            "INSERT INTO public.hub_members (hub_id, user_id, role) "
            "VALUES ($1::uuid, $2::uuid, 'member') ON CONFLICT DO NOTHING",
            hubId, ownerUuid.value);
    }

    auto ch = txn.exec_params(
        "SELECT 1 FROM public.channels WHERE hub_id = $1::uuid AND name = 'general' LIMIT 1",
        hubId);
    if (ch.empty()) {
        txn.exec_params(
            "INSERT INTO public.channels (hub_id, name, type) VALUES ($1::uuid, 'general', 'text')",
            hubId);
    }

    txn.commit();
    return HubId{hubId};
}

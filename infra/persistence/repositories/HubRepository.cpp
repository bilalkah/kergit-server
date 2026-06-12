#include "infra/persistence/repositories/HubRepository.h"

#include "infra/persistence/repositories/RepositorySchema.h"
#include "infra/persistence/repositories/RepositoryUtils.h"

#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace {

Channel parse_channel_row(const pqxx::row& row) {
    return Channel{row[2].as<std::string>(), ChannelId{row[0].as<std::string>()},
                   HubId{row[1].as<std::string>()},
                   channel_type_from_string(row[3].as<std::string>())};
}

Hub parse_hub_row(const pqxx::row& row) {
    Hub hub(row[1].as<std::string>(), HubId{row[0].as<std::string>()},
            UserId{row[2].as<std::string>()});
    hub.avatar_seed = row[3].as<std::string>("");
    return hub;
}

std::string normalize_db_role(const std::string& role) {
    switch (role_from_string(role)) {
        case Role::OWNER:
            return "owner";
        case Role::ADMIN:
            return "admin";
        case Role::USER:
        default:
            return "member";
    }
}

std::string hub_select_columns() {
    return "h.id::text, COALESCE(h.name, ''), h.owner_id::text, "
           "COALESCE(h.avatar_seed, '')";
}

std::string channel_select_columns() { return "id::text, hub_id::text, name, type"; }

}  // namespace

Hub HubRepository::createHub(const std::string& hubName, const UserId& ownerUuid) {
    return db_.write("HubRepository.createHub", [&](pqxx::work& txn) {
        const std::string query =
            std::string{"INSERT INTO "} + dbschema::kHubs +
            " (name, owner_id, created_by) "
            "VALUES ($1, $2::uuid, $2::uuid) "
            "RETURNING id::text, name, owner_id::text, COALESCE(avatar_seed, '')";

        auto res = txn.exec(query, pqxx::params{hubName, ownerUuid.value});
        if (res.empty()) {
            throw std::runtime_error("createHub failed");
        }

        return parse_hub_row(res[0]);
    });
}

void HubRepository::addMember(const HubId& hubId, const UserId& userUuid, const std::string& role) {
    db_.write("HubRepository.addMember", [&](pqxx::work& txn) {
        const std::string query = std::string{"INSERT INTO "} + dbschema::kHubMembers +
                                  " (hub_id, user_id, role_id, invited_by) "
                                  "VALUES ("
                                  "$1::uuid, "
                                  "$2::uuid, " +
                                  std::string{dbschema::kGetHubRoleId} +
                                  "($3), "
                                  "NULL"
                                  ") "
                                  "ON CONFLICT (hub_id, user_id) DO UPDATE "
                                  "SET "
                                  "role_id = EXCLUDED.role_id, "
                                  "updated_at = now()";

        txn.exec(query, pqxx::params{hubId.value, userUuid.value, normalize_db_role(role)});
    });
}

void HubRepository::removeMember(const HubId& hubId, const UserId& userUuid) {
    db_.write("HubRepository.removeMember", [&](pqxx::work& txn) {
        const std::string query = std::string{"DELETE FROM "} + dbschema::kHubMembers +
                                  " WHERE hub_id = $1::uuid AND user_id = $2::uuid";

        txn.exec(query, pqxx::params{hubId.value, userUuid.value});
    });
}

std::vector<Hub> HubRepository::getUserHubs(const UserId& userUuid) {
    return db_.read("HubRepository.getUserHubs", [&](pqxx::work& txn) {
        const std::string query = std::string{"SELECT "} + hub_select_columns() +
                                  ", COALESCE(r.name, 'member') "
                                  "FROM " +
                                  dbschema::kHubs +
                                  " h "
                                  "JOIN " +
                                  dbschema::kHubMembers +
                                  " hm ON h.id = hm.hub_id "
                                  "JOIN " +
                                  dbschema::kHubRoles +
                                  " r ON r.id = hm.role_id "
                                  "WHERE hm.user_id = $1::uuid "
                                  "ORDER BY h.created_at DESC";

        auto res = txn.exec(query, pqxx::params{userUuid.value});

        std::vector<Hub> hubs;
        hubs.reserve(res.size());

        for (const auto& row : res) {
            Hub hub = parse_hub_row(row);
            hub.setMemberRole(userUuid, role_from_string(row[4].as<std::string>("member")));
            hubs.push_back(std::move(hub));
        }

        return hubs;
    });
}

std::vector<Hub> HubRepository::getHubsByIds(const std::vector<HubId>& hubIds) {
    if (hubIds.empty()) {
        return {};
    }

    return db_.read("HubRepository.getHubsByIds", [&](pqxx::work& txn) {
        pqxx::params params;

        std::ostringstream query;
        query << "SELECT " << hub_select_columns() << " FROM " << dbschema::kHubs << " h "
              << "WHERE h.id = ANY(ARRAY[";

        for (size_t i = 0; i < hubIds.size(); ++i) {
            if (i > 0) {
                query << ", ";
            }

            query << "$" << (i + 1) << "::uuid";
            params.append(hubIds[i].value);
        }

        query << "]::uuid[])";

        auto res = txn.exec(query.str(), params);

        std::vector<Hub> hubs;
        hubs.reserve(res.size());

        for (const auto& row : res) {
            hubs.push_back(parse_hub_row(row));
        }

        return hubs;
    });
}

std::optional<Hub> HubRepository::getHub(const HubId& hubId) {
    return db_.read("HubRepository.getHub", [&](pqxx::work& txn) -> std::optional<Hub> {
        const std::string query = std::string{"SELECT "} + hub_select_columns() + " FROM " +
                                  dbschema::kHubs +
                                  " h "
                                  "WHERE h.id = $1::uuid "
                                  "LIMIT 1";

        auto res = txn.exec(query, pqxx::params{hubId.value});
        if (res.empty()) {
            return std::nullopt;
        }

        return parse_hub_row(res[0]);
    });
}

std::optional<Channel> HubRepository::getChannel(const ChannelId& channelId) {
    return db_.read("HubRepository.getChannel", [&](pqxx::work& txn) -> std::optional<Channel> {
        const std::string query = std::string{"SELECT "} + channel_select_columns() + " FROM " +
                                  dbschema::kChannels +
                                  " WHERE id = $1::uuid "
                                  "LIMIT 1";

        auto res = txn.exec(query, pqxx::params{channelId.value});
        if (res.empty()) {
            return std::nullopt;
        }

        return parse_channel_row(res[0]);
    });
}

std::vector<Channel> HubRepository::getChannelsByIds(const std::vector<ChannelId>& channelIds) {
    if (channelIds.empty()) {
        return {};
    }

    return db_.read("HubRepository.getChannelsByIds", [&](pqxx::work& txn) {
        pqxx::params params;

        std::ostringstream query;
        query << "SELECT " << channel_select_columns() << " FROM " << dbschema::kChannels
              << " WHERE id = ANY(ARRAY[";

        for (size_t i = 0; i < channelIds.size(); ++i) {
            if (i > 0) {
                query << ", ";
            }

            query << "$" << (i + 1) << "::uuid";
            params.append(channelIds[i].value);
        }

        query << "]::uuid[])";

        auto res = txn.exec(query.str(), params);

        std::vector<Channel> channels;
        channels.reserve(res.size());

        for (const auto& row : res) {
            channels.push_back(parse_channel_row(row));
        }

        return channels;
    });
}

std::vector<Channel> HubRepository::getHubChannels(const HubId& hubId) {
    return db_.read("HubRepository.getHubChannels", [&](pqxx::work& txn) {
        const std::string query = std::string{"SELECT "} + channel_select_columns() + " FROM " +
                                  dbschema::kChannels +
                                  " WHERE hub_id = $1::uuid "
                                  "ORDER BY position ASC, created_at ASC";

        auto res = txn.exec(query, pqxx::params{hubId.value});

        std::vector<Channel> channels;
        channels.reserve(res.size());

        for (const auto& row : res) {
            channels.push_back(parse_channel_row(row));
        }

        return channels;
    });
}

std::unordered_map<HubId, std::vector<Channel>> HubRepository::getHubChannelsByHubIds(
    const std::vector<HubId>& hubIds) {
    if (hubIds.empty()) {
        return {};
    }

    return db_.read("HubRepository.getHubChannelsByHubIds", [&](pqxx::work& txn) {
        pqxx::params params;

        std::ostringstream query;
        query << "SELECT " << channel_select_columns() << " FROM " << dbschema::kChannels
              << " WHERE hub_id = ANY(ARRAY[";

        for (size_t i = 0; i < hubIds.size(); ++i) {
            if (i > 0) {
                query << ", ";
            }

            query << "$" << (i + 1) << "::uuid";
            params.append(hubIds[i].value);
        }

        query << "]::uuid[]) " << "ORDER BY hub_id ASC, position ASC, created_at ASC";

        auto res = txn.exec(query.str(), params);

        std::unordered_map<HubId, std::vector<Channel>> by_hub;
        by_hub.reserve(hubIds.size());

        for (const auto& row : res) {
            const auto channel = parse_channel_row(row);
            by_hub[channel.hub_id].push_back(channel);
        }

        return by_hub;
    });
}

bool HubRepository::isHubMember(const HubId& hubId, const UserId& userUuid) {
    return db_.read("HubRepository.isHubMember", [&](pqxx::work& txn) {
        const std::string query = std::string{"SELECT 1 FROM "} + dbschema::kHubMembers +
                                  " WHERE hub_id = $1::uuid "
                                  "AND user_id = $2::uuid "
                                  "LIMIT 1";

        auto res = txn.exec(query, pqxx::params{hubId.value, userUuid.value});
        return !res.empty();
    });
}

std::optional<Role> HubRepository::getMembershipRole(const HubId& hubId, const UserId& userUuid) {
    return db_.read("HubRepository.getMembershipRole", [&](pqxx::work& txn) -> std::optional<Role> {
        const std::string query = std::string{"SELECT r.name FROM "} + dbschema::kHubMembers +
                                  " hm "
                                  "JOIN " +
                                  dbschema::kHubRoles +
                                  " r ON r.id = hm.role_id "
                                  "WHERE hm.hub_id = $1::uuid "
                                  "AND hm.user_id = $2::uuid "
                                  "LIMIT 1";

        auto res = txn.exec(query, pqxx::params{hubId.value, userUuid.value});
        if (res.empty()) {
            return std::nullopt;
        }

        return role_from_string(res[0][0].as<std::string>("member"));
    });
}

std::vector<HubRepository::MemberRole> HubRepository::getHubMemberRoles(const HubId& hubId) {
    return db_.read("HubRepository.getHubMemberRoles", [&](pqxx::work& txn) {
        const std::string query = std::string{"SELECT hm.user_id::text, r.name FROM "} +
                                  dbschema::kHubMembers +
                                  " hm "
                                  "JOIN " +
                                  dbschema::kHubRoles +
                                  " r ON r.id = hm.role_id "
                                  "WHERE hm.hub_id = $1::uuid "
                                  "ORDER BY hm.joined_at ASC";

        auto res = txn.exec(query, pqxx::params{hubId.value});

        std::vector<MemberRole> members;
        members.reserve(res.size());

        for (const auto& row : res) {
            members.push_back(MemberRole{
                UserId{row[0].as<std::string>()},
                role_from_string(row[1].as<std::string>("member")),
            });
        }

        return members;
    });
}

std::unordered_map<HubId, std::vector<HubRepository::MemberRole>>
HubRepository::getHubMemberRolesByHubIds(const std::vector<HubId>& hubIds) {
    if (hubIds.empty()) {
        return {};
    }

    return db_.read("HubRepository.getHubMemberRolesByHubIds", [&](pqxx::work& txn) {
        pqxx::params params;

        std::ostringstream query;
        query << "SELECT hm.hub_id::text, hm.user_id::text, r.name " << "FROM "
              << dbschema::kHubMembers << " hm " << "JOIN " << dbschema::kHubRoles
              << " r ON r.id = hm.role_id " << "WHERE hm.hub_id = ANY(ARRAY[";

        for (size_t i = 0; i < hubIds.size(); ++i) {
            if (i > 0) {
                query << ", ";
            }

            query << "$" << (i + 1) << "::uuid";
            params.append(hubIds[i].value);
        }

        query << "]::uuid[]) " << "ORDER BY hm.hub_id ASC, hm.joined_at ASC";

        auto res = txn.exec(query.str(), params);

        std::unordered_map<HubId, std::vector<MemberRole>> by_hub;
        by_hub.reserve(hubIds.size());

        for (const auto& row : res) {
            HubId hub_id{row[0].as<std::string>()};
            by_hub[hub_id].push_back(MemberRole{
                UserId{row[1].as<std::string>()},
                role_from_string(row[2].as<std::string>("member")),
            });
        }

        return by_hub;
    });
}

std::optional<Hub> HubRepository::getHubWithMembers(const HubId& hubId) {
    return db_.read("HubRepository.getHubWithMembers", [&](pqxx::work& txn) -> std::optional<Hub> {
        const std::string query = std::string{"SELECT "} + hub_select_columns() +
                                  ", hm.user_id::text, r.name "
                                  "FROM " +
                                  dbschema::kHubs +
                                  " h "
                                  "LEFT JOIN " +
                                  dbschema::kHubMembers +
                                  " hm ON hm.hub_id = h.id "
                                  "LEFT JOIN " +
                                  dbschema::kHubRoles +
                                  " r ON r.id = hm.role_id "
                                  "WHERE h.id = $1::uuid "
                                  "ORDER BY hm.joined_at ASC";

        auto res = txn.exec(query, pqxx::params{hubId.value});
        if (res.empty()) {
            return std::nullopt;
        }

        Hub hub = parse_hub_row(res[0]);

        for (const auto& row : res) {
            if (row[4].is_null()) {
                continue;
            }

            hub.setMemberRole(UserId{row[4].as<std::string>()},
                              role_from_string(row[5].as<std::string>("member")));
        }

        return hub;
    });
}

bool HubRepository::renameHub(const HubId& hubId, const std::string& name) {
    return db_.write("HubRepository.renameHub", [&](pqxx::work& txn) {
        const std::string query = std::string{"UPDATE "} + dbschema::kHubs +
                                  " SET name = $2 "
                                  "WHERE id = $1::uuid "
                                  "RETURNING id";

        auto res = txn.exec(query, pqxx::params{hubId.value, name});
        return !res.empty();
    });
}

bool HubRepository::updateHubAvatarSeed(const HubId& hubId, const std::string& avatar_seed) {
    return db_.write("HubRepository.updateHubAvatarSeed", [&](pqxx::work& txn) {
        const std::string query = std::string{"UPDATE "} + dbschema::kHubs +
                                  " SET avatar_seed = $2 "
                                  "WHERE id = $1::uuid "
                                  "RETURNING id";

        auto res = txn.exec(query, pqxx::params{hubId.value, avatar_seed});
        return !res.empty();
    });
}

bool HubRepository::deleteHub(const HubId& hubId, const UserId& ownerUuid) {
    return db_.write("HubRepository.deleteHub", [&](pqxx::work& txn) {
        const std::string query = std::string{"DELETE FROM "} + dbschema::kHubs +
                                  " WHERE id = $1::uuid "
                                  "AND owner_id = $2::uuid "
                                  "RETURNING id";

        auto res = txn.exec(query, pqxx::params{hubId.value, ownerUuid.value});
        return !res.empty();
    });
}

ChannelId HubRepository::createChannel(const HubId& hubId, const std::string& channelName,
                                       const std::string& type, const UserId& created_by) {
    return db_.write("HubRepository.createChannel", [&](pqxx::work& txn) {
        const std::string query = std::string{"INSERT INTO "} + dbschema::kChannels +
                                  " (hub_id, name, type, created_by) "
                                  "VALUES ($1::uuid, $2, $3, $4::uuid) "
                                  "RETURNING id::text";

        auto res = txn.exec(query, pqxx::params{hubId.value, channelName, type, created_by.value});
        if (res.empty()) {
            throw std::runtime_error("createChannel failed");
        }

        return ChannelId{res[0][0].as<std::string>()};
    });
}

bool HubRepository::renameChannel(const ChannelId& channelId, const std::string& name) {
    return db_.write("HubRepository.renameChannel", [&](pqxx::work& txn) {
        const std::string query = std::string{"UPDATE "} + dbschema::kChannels +
                                  " SET name = $2 "
                                  "WHERE id = $1::uuid "
                                  "RETURNING id";

        auto res = txn.exec(query, pqxx::params{channelId.value, name});
        return !res.empty();
    });
}

bool HubRepository::deleteChannel(const ChannelId& channelId, const HubId& hubId) {
    return db_.write("HubRepository.deleteChannel", [&](pqxx::work& txn) {
        const std::string query = std::string{"DELETE FROM "} + dbschema::kChannels +
                                  " WHERE id = $1::uuid "
                                  "AND hub_id = $2::uuid "
                                  "RETURNING id";

        auto res = txn.exec(query, pqxx::params{channelId.value, hubId.value});
        return !res.empty();
    });
}

std::vector<ChannelId> HubRepository::getHubChannelIds(const HubId& hubId) {
    return db_.read("HubRepository.getHubChannelIds", [&](pqxx::work& txn) {
        const std::string query = std::string{"SELECT id::text FROM "} + dbschema::kChannels +
                                  " WHERE hub_id = $1::uuid "
                                  "ORDER BY position ASC, created_at ASC";

        auto res = txn.exec(query, pqxx::params{hubId.value});

        std::vector<ChannelId> channel_ids;
        channel_ids.reserve(res.size());

        for (const auto& row : res) {
            channel_ids.emplace_back(ChannelId{row[0].as<std::string>()});
        }

        return channel_ids;
    });
}

std::unordered_map<HubId, std::vector<ChannelId>> HubRepository::getHubChannelIdsByHubIds(
    const std::vector<HubId>& hubIds) {
    if (hubIds.empty()) {
        return {};
    }

    return db_.read("HubRepository.getHubChannelIdsByHubIds", [&](pqxx::work& txn) {
        pqxx::params params;

        std::ostringstream query;
        query << "SELECT hub_id::text, id::text " << "FROM " << dbschema::kChannels
              << " WHERE hub_id = ANY(ARRAY[";

        for (size_t i = 0; i < hubIds.size(); ++i) {
            if (i > 0) {
                query << ", ";
            }

            query << "$" << (i + 1) << "::uuid";
            params.append(hubIds[i].value);
        }

        query << "]::uuid[]) " << "ORDER BY hub_id ASC, position ASC, created_at ASC";

        auto res = txn.exec(query.str(), params);

        std::unordered_map<HubId, std::vector<ChannelId>> by_hub;
        by_hub.reserve(hubIds.size());

        for (const auto& row : res) {
            HubId hub_id{row[0].as<std::string>()};
            by_hub[hub_id].push_back(ChannelId{row[1].as<std::string>()});
        }

        return by_hub;
    });
}

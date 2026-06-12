#include "infra/persistence/repositories/HubRepository.h"

#include "infra/persistence/repositories/RepositoryUtils.h"

#include <sstream>
#include <stdexcept>

namespace {

Channel parse_channel_row(const pqxx::row& row) {
    return Channel{row[2].as<std::string>(), ChannelId{row[0].as<std::string>()},
                   HubId{row[1].as<std::string>()},
                   channel_type_from_string(row[3].as<std::string>())};
}

}  // namespace

Hub HubRepository::createHub(const std::string& hubName, const UserId& ownerUuid) {
    return db_.write("HubRepository.createHub", [&](pqxx::work& txn) {
        auto res = txn.exec(
            "INSERT INTO public.hubs (name, owner_id) VALUES ($1, $2::uuid) "
            "RETURNING id::text, name, owner_id::text, COALESCE(avatar_seed, '')",
            pqxx::params{hubName, ownerUuid.value});
        if (res.empty()) throw std::runtime_error("createHub failed");
        Hub hub(res[0][1].as<std::string>(), HubId{res[0][0].as<std::string>()},
                UserId{res[0][2].as<std::string>()});
        hub.avatar_seed = res[0][3].as<std::string>("");
        return hub;
    });
}

void HubRepository::addMember(const HubId& hubId, const UserId& userUuid, const std::string& role) {
    db_.write("HubRepository.addMember", [&](pqxx::work& txn) {
        txn.exec(
            "INSERT INTO public.hub_members (hub_id, user_id, role) "
            "VALUES ($1::uuid, $2::uuid, $3) "
            "ON CONFLICT (hub_id, user_id) DO UPDATE SET role = EXCLUDED.role",
            pqxx::params{hubId.value, userUuid.value, role});
    });
}

void HubRepository::removeMember(const HubId& hubId, const UserId& userUuid) {
    db_.write("HubRepository.removeMember", [&](pqxx::work& txn) {
        txn.exec("DELETE FROM public.hub_members WHERE hub_id = $1::uuid AND user_id = $2::uuid",
                 pqxx::params{hubId.value, userUuid.value});
    });
}

std::vector<Hub> HubRepository::getUserHubs(const UserId& userUuid) {
    return db_.read("HubRepository.getUserHubs", [&](pqxx::work& txn) {
        // Filter out orphan hubs (owner_id IS NULL) to avoid crashes
        auto res = txn.exec(
            "SELECT h.id::text, COALESCE(h.name, ''), h.owner_id::text, "
            "COALESCE(h.avatar_seed, ''), COALESCE(m.role, 'member') "
            "FROM public.hubs h "
            "JOIN public.hub_members m ON h.id = m.hub_id "
            "WHERE m.user_id = $1::uuid AND h.owner_id IS NOT NULL "
            "ORDER BY h.created_at DESC",
            pqxx::params{userUuid.value});

        std::vector<Hub> hubs;
        hubs.reserve(res.size());
        for (const auto& row : res) {
            Hub hub(row[1].as<std::string>(), HubId{row[0].as<std::string>()},
                    UserId{row[2].as<std::string>()});
            hub.avatar_seed = row[3].as<std::string>();
            hub.setMemberRole(userUuid, role_from_string(row[4].as<std::string>()));
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
        query << "SELECT h.id::text, COALESCE(h.name, ''), h.owner_id::text, "
                 "COALESCE(h.avatar_seed, '') "
                 "FROM public.hubs h WHERE h.id = ANY(ARRAY[";
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
            Hub hub(row[1].as<std::string>(), HubId{row[0].as<std::string>()},
                    UserId{row[2].as<std::string>()});
            hub.avatar_seed = row[3].as<std::string>("");
            hubs.push_back(std::move(hub));
        }
        return hubs;
    });
}

std::optional<Hub> HubRepository::getHub(const HubId& hubId) {
    return db_.read("HubRepository.getHub", [&](pqxx::work& txn) -> std::optional<Hub> {
        auto res = txn.exec(
            "SELECT h.id::text, h.name, h.owner_id::text, h.avatar_seed "
            "FROM public.hubs h WHERE h.id = $1::uuid LIMIT 1",
            pqxx::params{hubId.value});
        if (res.empty()) return std::nullopt;

        Hub hub(res[0][1].as<std::string>(), HubId{res[0][0].as<std::string>()},
                UserId{res[0][2].as<std::string>()});
        hub.avatar_seed = res[0][3].as<std::string>("");
        return hub;
    });
}

std::optional<Channel> HubRepository::getChannel(const ChannelId& channelId) {
    return db_.read("HubRepository.getChannel", [&](pqxx::work& txn) -> std::optional<Channel> {
        auto res = txn.exec(
            "SELECT id::text, hub_id::text, name, type "
            "FROM public.channels WHERE id = $1::uuid LIMIT 1",
            pqxx::params{channelId.value});
        if (res.empty()) return std::nullopt;
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
        query << "SELECT id::text, hub_id::text, name, type "
                 "FROM public.channels "
                 "WHERE id = ANY(ARRAY[";
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
        auto res = txn.exec(
            "SELECT id::text, hub_id::text, name, type "
            "FROM public.channels WHERE hub_id = $1::uuid ORDER BY created_at ASC",
            pqxx::params{hubId.value});
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
        query << "SELECT id::text, hub_id::text, name, type "
                 "FROM public.channels "
                 "WHERE hub_id = ANY(ARRAY[";
        for (size_t i = 0; i < hubIds.size(); ++i) {
            if (i > 0) {
                query << ", ";
            }
            query << "$" << (i + 1) << "::uuid";
            params.append(hubIds[i].value);
        }
        query << "]::uuid[]) "
                 "ORDER BY hub_id ASC, created_at ASC";

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
        auto res = txn.exec(
            "SELECT 1 FROM public.hub_members WHERE hub_id = $1::uuid AND user_id = $2::uuid "
            "LIMIT 1",
            pqxx::params{hubId.value, userUuid.value});
        return !res.empty();
    });
}

std::optional<Role> HubRepository::getMembershipRole(const HubId& hubId, const UserId& userUuid) {
    return db_.read("HubRepository.getMembershipRole", [&](pqxx::work& txn) -> std::optional<Role> {
        auto res = txn.exec(
            "SELECT role FROM public.hub_members WHERE hub_id = $1::uuid AND user_id = $2::uuid "
            "LIMIT 1",
            pqxx::params{hubId.value, userUuid.value});
        if (res.empty()) return std::nullopt;
        return role_from_string(res[0][0].as<std::string>());
    });
}

std::vector<HubRepository::MemberRole> HubRepository::getHubMemberRoles(const HubId& hubId) {
    return db_.read("HubRepository.getHubMemberRoles", [&](pqxx::work& txn) {
        auto res = txn.exec(
            "SELECT user_id::text, role FROM public.hub_members "
            "WHERE hub_id = $1::uuid ORDER BY joined_at ASC",
            pqxx::params{hubId.value});
        std::vector<MemberRole> members;
        members.reserve(res.size());
        for (const auto& row : res) {
            members.push_back(MemberRole{UserId{row[0].as<std::string>()},
                                         role_from_string(row[1].as<std::string>(""))});
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
        query << "SELECT hub_id::text, user_id::text, role "
                 "FROM public.hub_members "
                 "WHERE hub_id = ANY(ARRAY[";
        for (size_t i = 0; i < hubIds.size(); ++i) {
            if (i > 0) {
                query << ", ";
            }
            query << "$" << (i + 1) << "::uuid";
            params.append(hubIds[i].value);
        }
        query << "]::uuid[]) "
                 "ORDER BY hub_id ASC, joined_at ASC";

        auto res = txn.exec(query.str(), params);
        std::unordered_map<HubId, std::vector<MemberRole>> by_hub;
        by_hub.reserve(hubIds.size());
        for (const auto& row : res) {
            HubId hub_id{row[0].as<std::string>()};
            by_hub[hub_id].push_back(MemberRole{
                UserId{row[1].as<std::string>()},
                role_from_string(row[2].as<std::string>("")),
            });
        }
        return by_hub;
    });
}

std::optional<Hub> HubRepository::getHubWithMembers(const HubId& hubId) {
    return db_.read("HubRepository.getHubWithMembers", [&](pqxx::work& txn) -> std::optional<Hub> {
        auto res = txn.exec(
            "SELECT h.id::text, h.name, h.owner_id::text, h.avatar_seed, hm.user_id::text, "
            "hm.role "
            "FROM public.hubs h "
            "LEFT JOIN public.hub_members hm ON hm.hub_id = h.id "
            "WHERE h.id = $1::uuid ORDER BY hm.joined_at ASC",
            pqxx::params{hubId.value});
        if (res.empty()) return std::nullopt;

        const auto& first = res[0];
        Hub hub(first[1].as<std::string>(), HubId{first[0].as<std::string>()},
                UserId{first[2].as<std::string>()});
        hub.avatar_seed = first[3].as<std::string>("");

        for (const auto& row : res) {
            if (row[4].is_null()) continue;
            hub.setMemberRole(UserId{row[4].as<std::string>()},
                              role_from_string(row[5].as<std::string>("")));
        }
        return hub;
    });
}

bool HubRepository::renameHub(const HubId& hubId, const std::string& name) {
    return db_.write("HubRepository.renameHub", [&](pqxx::work& txn) {
        auto res = txn.exec("UPDATE public.hubs SET name = $2 WHERE id = $1::uuid RETURNING id",
                            pqxx::params{hubId.value, name});
        return !res.empty();
    });
}

bool HubRepository::updateHubAvatarSeed(const HubId& hubId, const std::string& avatar_seed) {
    return db_.write("HubRepository.updateHubAvatarSeed", [&](pqxx::work& txn) {
        auto res =
            txn.exec("UPDATE public.hubs SET avatar_seed = $2 WHERE id = $1::uuid RETURNING id",
                     pqxx::params{hubId.value, avatar_seed});
        return !res.empty();
    });
}

bool HubRepository::deleteHub(const HubId& hubId, const UserId& ownerUuid) {
    return db_.write("HubRepository.deleteHub", [&](pqxx::work& txn) {
        auto res = txn.exec(
            "DELETE FROM public.hubs WHERE id = $1::uuid AND owner_id = $2::uuid RETURNING id",
            pqxx::params{hubId.value, ownerUuid.value});
        return !res.empty();
    });
}

ChannelId HubRepository::createChannel(const HubId& hubId, const std::string& channelName,
                                       const std::string& type) {
    return db_.write("HubRepository.createChannel", [&](pqxx::work& txn) {
        auto res = txn.exec(
            "INSERT INTO public.channels (hub_id, name, type) VALUES ($1::uuid, $2, $3) "
            "RETURNING id::text",
            pqxx::params{hubId.value, channelName, type});
        if (res.empty()) throw std::runtime_error("createChannel failed");
        return ChannelId{res[0][0].as<std::string>()};
    });
}

bool HubRepository::renameChannel(const ChannelId& channelId, const std::string& name) {
    return db_.write("HubRepository.renameChannel", [&](pqxx::work& txn) {
        auto res = txn.exec("UPDATE public.channels SET name = $2 WHERE id = $1::uuid RETURNING id",
                            pqxx::params{channelId.value, name});
        return !res.empty();
    });
}

bool HubRepository::deleteChannel(const ChannelId& channelId, const HubId& hubId) {
    return db_.write("HubRepository.deleteChannel", [&](pqxx::work& txn) {
        auto res = txn.exec(
            "DELETE FROM public.channels WHERE id = $1::uuid AND hub_id = $2::uuid RETURNING id",
            pqxx::params{channelId.value, hubId.value});
        return !res.empty();
    });
}

HubId HubRepository::ensurePersonalHubWithGeneral(const UserId& ownerUuid,
                                                  const std::string& hubName) {
    return db_.write("HubRepository.ensurePersonalHubWithGeneral", [&](pqxx::work& txn) {
        auto existing =
            txn.exec("SELECT id::text FROM public.hubs WHERE owner_id = $1::uuid LIMIT 1",
                     pqxx::params{ownerUuid.value});

        std::string hubId;
        if (existing.empty()) {
            auto res = txn.exec(
                "INSERT INTO public.hubs (name, owner_id) VALUES ($1, $2::uuid) RETURNING id::text",
                pqxx::params{hubName, ownerUuid.value});
            hubId = res[0][0].as<std::string>();
        } else {
            hubId = existing[0][0].as<std::string>();
            txn.exec(
                "INSERT INTO public.hub_members (hub_id, user_id, role) "
                "VALUES ($1::uuid, $2::uuid, 'member') ON CONFLICT DO NOTHING",
                pqxx::params{hubId, ownerUuid.value});
        }

        auto ch = txn.exec(
            "SELECT 1 FROM public.channels WHERE hub_id = $1::uuid AND name = 'general' LIMIT 1",
            pqxx::params{hubId});
        if (ch.empty()) {
            txn.exec(
                "INSERT INTO public.channels (hub_id, name, type) VALUES ($1::uuid, 'general', "
                "'text')",
                pqxx::params{hubId});
        }

        return HubId{hubId};
    });
}

std::vector<ChannelId> HubRepository::getHubChannelIds(const HubId& hubId) {
    return db_.read("HubRepository.getHubChannelIds", [&](pqxx::work& txn) {
        auto res = txn.exec(
            "SELECT id::text FROM public.channels WHERE hub_id = $1::uuid ORDER BY created_at ASC",
            pqxx::params{hubId.value});
        std::vector<ChannelId> channelIds;
        channelIds.reserve(res.size());
        for (const auto& row : res) {
            channelIds.emplace_back(ChannelId{row[0].as<std::string>()});
        }
        return channelIds;
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
        query << "SELECT hub_id::text, id::text FROM public.channels "
                 "WHERE hub_id = ANY(ARRAY[";
        for (size_t i = 0; i < hubIds.size(); ++i) {
            if (i > 0) {
                query << ", ";
            }
            query << "$" << (i + 1) << "::uuid";
            params.append(hubIds[i].value);
        }
        query << "]::uuid[]) "
                 "ORDER BY hub_id ASC, created_at ASC";

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

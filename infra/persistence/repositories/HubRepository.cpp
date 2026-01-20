#include "infra/persistence/repositories/HubRepository.h"

#include "infra/persistence/repositories/RepositoryUtils.h"

#include <stdexcept>

HubId HubRepository::createHub(const std::string& hubName, const UserId& ownerUuid) {
    return db_.write("HubRepository.createHub", [&](pqxx::work& txn) {
        auto res = txn.exec(
            "INSERT INTO public.hubs (name, owner_id) VALUES ($1, $2::uuid) RETURNING id::text",
            pqxx::params{hubName, ownerUuid.value});
        if (res.empty()) throw std::runtime_error("createHub failed");
        return HubId{res[0][0].as<std::string>()};
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
        auto res = txn.exec(
            "SELECT h.id::text, h.name, h.owner_id::text, m.role "
            "FROM public.hubs h "
            "JOIN public.hub_members m ON h.id = m.hub_id "
            "WHERE m.user_id = $1::uuid ORDER BY h.created_at DESC",
            pqxx::params{userUuid.value});

        std::vector<Hub> hubs;
        hubs.reserve(res.size());
        for (const auto& row : res) {
            Hub hub(row[1].as<std::string>(), HubId{row[0].as<std::string>()},
                    UserId{row[2].as<std::string>()});
            hub.setMemberRole(userUuid, role_from_string(row[3].as<std::string>()));
            hubs.push_back(std::move(hub));
        }
        return hubs;
    });
}

std::optional<Hub> HubRepository::getHub(const HubId& hubId) {
    return db_.read("HubRepository.getHub", [&](pqxx::work& txn) -> std::optional<Hub> {
        auto res = txn.exec(
            "SELECT h.id::text, h.name, h.owner_id::text "
            "FROM public.hubs h WHERE h.id = $1::uuid LIMIT 1",
            pqxx::params{hubId.value});
        if (res.empty()) return std::nullopt;

        Hub hub(res[0][1].as<std::string>(), HubId{res[0][0].as<std::string>()},
                UserId{res[0][2].as<std::string>()});

        auto members =
            txn.exec("SELECT user_id::text, role FROM public.hub_members WHERE hub_id = $1::uuid",
                     pqxx::params{hubId.value});
        for (const auto& member_row : members) {
            hub.setMemberRole(UserId{member_row[0].as<std::string>()},
                              role_from_string(member_row[1].as<std::string>()));
        }
        return hub;
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
    return db_.read("HubRepository.getMembershipRole",
                    [&](pqxx::work& txn) -> std::optional<Role> {
        auto res = txn.exec(
            "SELECT role FROM public.hub_members WHERE hub_id = $1::uuid AND user_id = $2::uuid "
            "LIMIT 1",
            pqxx::params{hubId.value, userUuid.value});
        if (res.empty()) return std::nullopt;
        return role_from_string(res[0][0].as<std::string>());
    });
}

std::vector<std::pair<UserId, std::string>> HubRepository::getHubMembers(const HubId& hubId) {
    return db_.read("HubRepository.getHubMembers", [&](pqxx::work& txn) {
        auto res = txn.exec(
            "SELECT hm.user_id::text, COALESCE(u.raw_user_meta_data->>'username',"
            " u.raw_user_meta_data->>'preferred_username', u.raw_user_meta_data->>'full_name', '') "
            "AS display "
            "FROM public.hub_members hm "
            "LEFT JOIN auth.users u ON u.id = hm.user_id "
            "WHERE hm.hub_id = $1::uuid ORDER BY hm.joined_at ASC",
            pqxx::params{hubId.value});
        std::vector<std::pair<UserId, std::string>> members;
        members.reserve(res.size());
        for (const auto& row : res) {
            members.emplace_back(UserId{row[0].as<std::string>()}, row[1].as<std::string>());
        }
        return members;
    });
}

std::vector<HubRepository::MemberWithRole> HubRepository::getHubMembersWithRoles(
    const HubId& hubId) {
    return db_.read("HubRepository.getHubMembersWithRoles", [&](pqxx::work& txn) {
        auto res = txn.exec(
            "SELECT hm.user_id::text, COALESCE(u.raw_user_meta_data->>'username',"
            " u.raw_user_meta_data->>'preferred_username', u.raw_user_meta_data->>'full_name', '') "
            "AS display, hm.role "
            "FROM public.hub_members hm "
            "LEFT JOIN auth.users u ON u.id = hm.user_id "
            "WHERE hm.hub_id = $1::uuid ORDER BY hm.joined_at ASC",
            pqxx::params{hubId.value});
        std::vector<MemberWithRole> members;
        members.reserve(res.size());
        for (const auto& row : res) {
            const auto role_str = row[2].as<std::string>("");
            members.push_back(MemberWithRole{UserId{row[0].as<std::string>()},
                                             row[1].as<std::string>(),
                                             role_from_string(role_str)});
        }
        return members;
    });
}

bool HubRepository::renameHub(const HubId& hubId, const std::string& name) {
    return db_.write("HubRepository.renameHub", [&](pqxx::work& txn) {
        auto res = txn.exec("UPDATE public.hubs SET name = $2 WHERE id = $1::uuid RETURNING id",
                            pqxx::params{hubId.value, name});
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

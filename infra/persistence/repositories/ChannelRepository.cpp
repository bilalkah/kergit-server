#include "infra/persistence/repositories/ChannelRepository.h"

#include "infra/persistence/repositories/RepositoryUtils.h"

#include <stdexcept>

ChannelId ChannelRepository::createChannel(const HubId& hubId, const std::string& channelName,
                                           const std::string& type) {
    return mux_.run(Repository::Channel, [&](pqxx::work& txn) {
        auto res = txn.exec(
            "INSERT INTO public.channels (hub_id, name, type) VALUES ($1::uuid, $2, $3) "
            "RETURNING id::text",
            pqxx::params{hubId.value, channelName, type});
        if (res.empty()) throw std::runtime_error("createChannel failed");
        return ChannelId{res[0][0].as<std::string>()};
    });
}

bool ChannelRepository::deleteChannel(const ChannelId& channelId, const HubId& hubId) {
    return mux_.run(Repository::Channel, [&](pqxx::work& txn) {
        auto res = txn.exec(
            "DELETE FROM public.channels WHERE id = $1::uuid AND hub_id = $2::uuid RETURNING id",
            pqxx::params{channelId.value, hubId.value});
        return !res.empty();
    });
}

Message ChannelRepository::sendMessage(const ChannelId& channelId, const UserId& senderUuid,
                                       const std::string& content) {
    return mux_.run(Repository::Message, [&](pqxx::work& txn) {
        auto res = txn.exec(
            "INSERT INTO public.messages (channel_id, sender_id, content) "
            "VALUES ($1::uuid, $2::uuid, $3) "
            "RETURNING id::text, channel_id::text, sender_id::text, content, created_at",
            pqxx::params{channelId.value, senderUuid.value, content});
        if (res.empty()) throw std::runtime_error("sendMessage failed");
        Message msg;
        msg.id = MessageId{res[0][0].as<std::string>()};
        msg.ch_id = ChannelId{res[0][1].as<std::string>()};
        msg.sender_id = UserId{res[0][2].as<std::string>()};
        msg.text = res[0][3].as<std::string>();
        msg.sent_at = parse_timestamp(res[0][4].as<std::string>());
        return msg;
    });
}

std::vector<Message> ChannelRepository::fetchMessages(const ChannelId& channelId, int limit) {
    return mux_.run(Repository::Message, [&](pqxx::work& txn) {
        auto res = txn.exec(
            "SELECT id::text, channel_id::text, sender_id::text, content, created_at "
            "FROM public.messages WHERE channel_id = $1::uuid "
            "ORDER BY created_at DESC LIMIT $2",
            pqxx::params{channelId.value, limit});

        std::vector<Message> msgs;
        msgs.reserve(res.size());
        for (const auto& row : res) {
            Message msg;
            msg.id = MessageId{row[0].as<std::string>()};
            msg.ch_id = ChannelId{row[1].as<std::string>()};
            msg.sender_id = UserId{row[2].as<std::string>()};
            msg.text = row[3].as<std::string>();
            msg.sent_at = parse_timestamp(row[4].as<std::string>());
            msgs.push_back(std::move(msg));
        }
        return msgs;
    });
}

std::vector<Channel> ChannelRepository::getHubChannels(const HubId& hubId) {
    return mux_.run(Repository::Channel, [&](pqxx::work& txn) {
        auto res = txn.exec(
            "SELECT id::text, hub_id::text, name, type "
            "FROM public.channels WHERE hub_id = $1::uuid ORDER BY created_at ASC",
            pqxx::params{hubId.value});

        std::vector<Channel> chans;
        chans.reserve(res.size());
        for (const auto& row : res) {
            Channel channel(row[2].as<std::string>(), ChannelId{row[0].as<std::string>()},
                            HubId{row[1].as<std::string>()},
                            channel_type_from_string(row[3].as<std::string>()));
            chans.push_back(std::move(channel));
        }
        return chans;
    });
}

std::optional<Channel> ChannelRepository::getChannel(const ChannelId& channelId) {
    return mux_.run(Repository::Channel, [&](pqxx::work& txn) -> std::optional<Channel> {
        auto res = txn.exec(
            "SELECT id::text, hub_id::text, name, type "
            "FROM public.channels WHERE id = $1::uuid LIMIT 1",
            pqxx::params{channelId.value});
        if (res.empty()) return std::nullopt;
        const auto& row = res[0];
        return Channel{row[2].as<std::string>(), ChannelId{row[0].as<std::string>()},
                       HubId{row[1].as<std::string>()},
                       channel_type_from_string(row[3].as<std::string>())};
    });
}

bool ChannelRepository::renameChannel(const ChannelId& channelId, const std::string& name) {
    return mux_.run(Repository::Channel, [&](pqxx::work& txn) {
        auto res = txn.exec("UPDATE public.channels SET name = $2 WHERE id = $1::uuid RETURNING id",
                            pqxx::params{channelId.value, name});
        return !res.empty();
    });
}

#include "infra/persistence/repositories/MessageRepository.h"

#include <algorithm>
#include <stdexcept>

namespace {

Message row_to_message(const pqxx::row& row) {
    Message msg;
    msg.id = MessageId{row[0].as<std::string>()};
    msg.ch_id = ChannelId{row[1].as<std::string>()};
    msg.sender_id = UserId{row[2].as<std::string>()};
    msg.text = row[3].as<std::string>();
    msg.created_at_unix_us = static_cast<uint64_t>(row[4].as<long long>());
    return msg;
}

}  // namespace

Message MessageRepository::sendMessage(const ChannelId& channelId, const UserId& senderUuid,
                                       const std::string& content) {
    return db_.write("MessageRepository.sendMessage", [&](pqxx::work& txn) {
        auto res = txn.exec(
            "INSERT INTO public.messages (channel_id, sender_id, content) "
            "VALUES ($1::uuid, $2::uuid, $3) "
            "RETURNING id::text, channel_id::text, sender_id::text, content, "
            "(EXTRACT(EPOCH FROM created_at) * 1000000)::bigint AS created_at_unix_us",
            pqxx::params{channelId.value, senderUuid.value, content});
        if (res.empty()) throw std::runtime_error("sendMessage failed");
        return row_to_message(res[0]);
    });
}

bool MessageRepository::insertMessage(const Message& msg) {
    return db_.write("MessageRepository.insertMessage", [&](pqxx::work& txn) {
        auto res = txn.exec(
            "INSERT INTO public.messages (id, channel_id, sender_id, content) "
            "VALUES ($1::uuid, $2::uuid, $3::uuid, $4)",
            pqxx::params{msg.id.value, msg.ch_id.value, msg.sender_id.value, msg.text});
        return res.affected_rows() > 0;
    });
}

std::vector<Message> MessageRepository::fetchMessages(const ChannelId& channelId, int limit) {
    return db_.read("MessageRepository.fetchMessages", [&](pqxx::work& txn) {
        auto res = txn.exec(
            "SELECT id::text, channel_id::text, sender_id::text, content, "
            "(EXTRACT(EPOCH FROM created_at) * 1000000)::bigint AS created_at_unix_us "
            "FROM public.messages WHERE channel_id = $1::uuid "
            "ORDER BY created_at DESC, id DESC LIMIT $2",
            pqxx::params{channelId.value, limit});

        std::vector<Message> msgs;
        msgs.reserve(res.size());
        for (const auto& row : res) {
            msgs.push_back(row_to_message(row));
        }
        return msgs;
    });
}

std::vector<Message> MessageRepository::fetchMessagesAfter(const ChannelId& channelId,
                                                           const MessageCursor& after, int limit) {
    return db_.read("MessageRepository.fetchMessagesAfter", [&](pqxx::work& txn) {
        auto res = txn.exec(
            "SELECT id::text, channel_id::text, sender_id::text, content, "
            "(EXTRACT(EPOCH FROM created_at) * 1000000)::bigint AS created_at_unix_us "
            "FROM public.messages WHERE channel_id = $1::uuid "
            "AND (created_at, id) > ((TIMESTAMPTZ 'epoch' + $2::bigint * "
            "INTERVAL '1 microsecond'), $3::uuid) "
            "ORDER BY created_at ASC, id ASC LIMIT $4",
            pqxx::params{channelId.value, after.created_at_unix_us, after.message_id.value, limit});

        std::vector<Message> msgs;
        msgs.reserve(res.size());
        for (const auto& row : res) {
            msgs.push_back(row_to_message(row));
        }
        return msgs;
    });
}

std::vector<Message> MessageRepository::fetchMessagesBefore(const ChannelId& channelId,
                                                            const MessageCursor& before, int limit) {
    return db_.read("MessageRepository.fetchMessagesBefore", [&](pqxx::work& txn) {
        auto res = txn.exec(
            "SELECT id::text, channel_id::text, sender_id::text, content, "
            "(EXTRACT(EPOCH FROM created_at) * 1000000)::bigint AS created_at_unix_us "
            "FROM public.messages WHERE channel_id = $1::uuid "
            "AND (created_at, id) < ((TIMESTAMPTZ 'epoch' + $2::bigint * "
            "INTERVAL '1 microsecond'), $3::uuid) "
            "ORDER BY created_at DESC, id DESC LIMIT $4",
            pqxx::params{channelId.value, before.created_at_unix_us, before.message_id.value, limit});

        std::vector<Message> msgs;
        msgs.reserve(res.size());
        for (const auto& row : res) {
            msgs.push_back(row_to_message(row));
        }
        return msgs;
    });
}

std::vector<Message> MessageRepository::fetchMessagesPage(const ChannelId& channelId,
                                                          std::optional<MessageCursor> after,
                                                          std::optional<MessageCursor> before,
                                                          int limit) {
    return db_.read("MessageRepository.fetchMessagesPage", [&](pqxx::work& txn) {
        pqxx::result res;
        if (after.has_value()) {
            if (before.has_value()) {
                res = txn.exec(
                    "SELECT id::text, channel_id::text, sender_id::text, content, "
                    "(EXTRACT(EPOCH FROM created_at) * 1000000)::bigint AS created_at_unix_us "
                    "FROM public.messages WHERE channel_id = $1::uuid "
                    "AND (created_at, id) > ((TIMESTAMPTZ 'epoch' + $2::bigint * "
                    "INTERVAL '1 microsecond'), $3::uuid) "
                    "AND (created_at, id) < ((TIMESTAMPTZ 'epoch' + $4::bigint * "
                    "INTERVAL '1 microsecond'), $5::uuid) "
                    "ORDER BY created_at ASC, id ASC LIMIT $6",
                    pqxx::params{channelId.value, after->created_at_unix_us, after->message_id.value,
                                 before->created_at_unix_us, before->message_id.value, limit});
            } else {
                res = txn.exec(
                    "SELECT id::text, channel_id::text, sender_id::text, content, "
                    "(EXTRACT(EPOCH FROM created_at) * 1000000)::bigint AS created_at_unix_us "
                    "FROM public.messages WHERE channel_id = $1::uuid "
                    "AND (created_at, id) > ((TIMESTAMPTZ 'epoch' + $2::bigint * "
                    "INTERVAL '1 microsecond'), $3::uuid) "
                    "ORDER BY created_at ASC, id ASC LIMIT $4",
                    pqxx::params{channelId.value, after->created_at_unix_us,
                                 after->message_id.value, limit});
            }
        } else if (before.has_value()) {
            res = txn.exec(
                "SELECT id::text, channel_id::text, sender_id::text, content, "
                "(EXTRACT(EPOCH FROM created_at) * 1000000)::bigint AS created_at_unix_us "
                "FROM public.messages WHERE channel_id = $1::uuid "
                "AND (created_at, id) < ((TIMESTAMPTZ 'epoch' + $2::bigint * "
                "INTERVAL '1 microsecond'), $3::uuid) "
                "ORDER BY created_at DESC, id DESC LIMIT $4",
                pqxx::params{channelId.value, before->created_at_unix_us, before->message_id.value,
                             limit});
        } else {
            res = txn.exec(
                "SELECT id::text, channel_id::text, sender_id::text, content, "
                "(EXTRACT(EPOCH FROM created_at) * 1000000)::bigint AS created_at_unix_us "
                "FROM public.messages WHERE channel_id = $1::uuid "
                "ORDER BY created_at DESC, id DESC LIMIT $2",
                pqxx::params{channelId.value, limit});
        }

        std::vector<Message> msgs;
        msgs.reserve(res.size());
        for (const auto& row : res) {
            msgs.push_back(row_to_message(row));
        }
        return msgs;
    });
}

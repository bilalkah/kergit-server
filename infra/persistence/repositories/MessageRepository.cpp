#include "infra/persistence/repositories/MessageRepository.h"

#include <algorithm>
#include <nlohmann/json.hpp>
#include <stdexcept>

namespace {

using json = nlohmann::json;

constexpr const char* kEmptyAttachmentsJson = "[]";

int attachment_kind_to_int(const MessageAttachmentKind kind) {
    switch (kind) {
        case MessageAttachmentKind::IMAGE:
            return 1;
        case MessageAttachmentKind::FILE:
        default:
            return 2;
    }
}

MessageAttachmentKind attachment_kind_from_json(const json& value) {
    if (value.is_number_integer()) {
        const auto numeric = value.get<int>();
        if (numeric == 1) return MessageAttachmentKind::IMAGE;
        return MessageAttachmentKind::FILE;
    }
    if (value.is_string() && value.get<std::string>() == "image") return MessageAttachmentKind::IMAGE;
    return MessageAttachmentKind::FILE;
}

json to_json_attachments(const std::vector<MessageAttachment>& attachments) {
    json out = json::array();
    for (const auto& attachment : attachments) {
        out.push_back(json::object({
            {"id", attachment.id},
            {"kind", attachment_kind_to_int(attachment.kind)},
            {"storage_key", attachment.storage_key},
            {"mime_type", attachment.mime_type},
            {"display_name", attachment.display_name},
            {"size_bytes", attachment.size_bytes},
        }));
    }
    return out;
}

std::vector<MessageAttachment> from_json_attachments(const std::string& raw_json) {
    std::vector<MessageAttachment> out;
    const auto parsed = json::parse(raw_json, nullptr, false);
    if (!parsed.is_array()) return out;
    out.reserve(parsed.size());
    for (const auto& item : parsed) {
        if (!item.is_object()) continue;
        MessageAttachment next;
        next.id = item.value("id", "");
        next.kind = attachment_kind_from_json(item.contains("kind") ? item.at("kind") : json{});
        next.storage_key = item.value("storage_key", "");
        next.mime_type = item.value("mime_type", "");
        next.display_name = item.value("display_name", "");
        next.size_bytes = item.value("size_bytes", uint64_t{0});
        if (next.id.empty() || next.storage_key.empty()) continue;
        out.push_back(std::move(next));
    }
    return out;
}

json to_json_link_preview(const MessageLinkPreview& link_preview) {
    return json::object({
        {"url", link_preview.url},
        {"title", link_preview.title},
        {"description", link_preview.description},
        {"site_name", link_preview.site_name},
        {"image_url", link_preview.image_url},
    });
}

std::optional<MessageLinkPreview> from_json_link_preview(const std::string& raw_json) {
    const auto parsed = json::parse(raw_json, nullptr, false);
    if (!parsed.is_object()) return std::nullopt;
    MessageLinkPreview out;
    out.url = parsed.value("url", "");
    out.title = parsed.value("title", "");
    out.description = parsed.value("description", "");
    out.site_name = parsed.value("site_name", "");
    out.image_url = parsed.value("image_url", "");
    if (out.url.empty()) return std::nullopt;
    return out;
}

Message row_to_message(const pqxx::row& row) {
    Message msg;
    msg.id = MessageId{row[0].as<std::string>()};
    msg.ch_id = ChannelId{row[1].as<std::string>()};
    msg.sender_id = UserId{row[2].as<std::string>()};
    msg.text = row[3].as<std::string>();
    msg.attachments = from_json_attachments(row[4].as<std::string>(kEmptyAttachmentsJson));
    if (row[5].is_null()) {
        msg.link_preview = std::nullopt;
    } else {
        msg.link_preview = from_json_link_preview(row[5].as<std::string>());
    }
    msg.created_at_unix_us = static_cast<uint64_t>(row[6].as<long long>());
    return msg;
}

}  // namespace

Message MessageRepository::sendMessage(const ChannelId& channelId, const UserId& senderUuid,
                                       const std::string& content,
                                       std::vector<MessageAttachment> attachments,
                                       std::optional<MessageLinkPreview> link_preview) {
    return db_.write("MessageRepository.sendMessage", [&](pqxx::work& txn) {
        const auto attachments_json = to_json_attachments(attachments).dump();
        const auto link_preview_json =
            link_preview.has_value() ? std::optional<std::string>(to_json_link_preview(*link_preview).dump())
                                     : std::nullopt;
        auto res = txn.exec(
            "INSERT INTO public.messages (channel_id, sender_id, content, attachments_json, "
            "link_preview_json) "
            "VALUES ($1::uuid, $2::uuid, $3, $4::jsonb, $5::jsonb) "
            "RETURNING id::text, channel_id::text, sender_id::text, content, attachments_json::text,"
            "link_preview_json::text, "
            "(EXTRACT(EPOCH FROM created_at) * 1000000)::bigint AS created_at_unix_us",
            pqxx::params{channelId.value, senderUuid.value, content, attachments_json,
                         link_preview_json});
        if (res.empty()) throw std::runtime_error("sendMessage failed");
        return row_to_message(res[0]);
    });
}

bool MessageRepository::insertMessage(const Message& msg) {
    return db_.write("MessageRepository.insertMessage", [&](pqxx::work& txn) {
        const auto attachments_json = to_json_attachments(msg.attachments).dump();
        const auto link_preview_json =
            msg.link_preview.has_value()
                ? std::optional<std::string>(to_json_link_preview(*msg.link_preview).dump())
                : std::nullopt;
        auto res = txn.exec(
            "INSERT INTO public.messages (id, channel_id, sender_id, content, attachments_json, "
            "link_preview_json) "
            "VALUES ($1::uuid, $2::uuid, $3::uuid, $4, $5::jsonb, $6::jsonb)",
            pqxx::params{msg.id.value, msg.ch_id.value, msg.sender_id.value, msg.text,
                         attachments_json, link_preview_json});
        return res.affected_rows() > 0;
    });
}

std::vector<Message> MessageRepository::fetchMessages(const ChannelId& channelId, int limit) {
    return db_.read("MessageRepository.fetchMessages", [&](pqxx::work& txn) {
        auto res = txn.exec(
            "SELECT id::text, channel_id::text, sender_id::text, content, attachments_json::text, "
            "link_preview_json::text, "
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
            "SELECT id::text, channel_id::text, sender_id::text, content, attachments_json::text, "
            "link_preview_json::text, "
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
            "SELECT id::text, channel_id::text, sender_id::text, content, attachments_json::text, "
            "link_preview_json::text, "
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
                    "attachments_json::text, link_preview_json::text, "
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
                    "attachments_json::text, link_preview_json::text, "
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
                "attachments_json::text, link_preview_json::text, "
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
                "attachments_json::text, link_preview_json::text, "
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

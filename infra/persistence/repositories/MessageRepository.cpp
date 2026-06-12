// New message persistence model:
// messages -> kergit_app.messages
// attachments_json -> kergit_app.message_attachments
// file bytes -> private object storage
// message_seq is DB-assigned but not yet carried by the C++ Message domain model.

#include "infra/persistence/repositories/MessageRepository.h"

#include "infra/persistence/repositories/RepositorySchema.h"

#include <cstdint>
#include <nlohmann/json.hpp>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace {

using json = nlohmann::json;

constexpr const char* kEmptyAttachmentsJson = "[]";

MessageAttachmentKind attachment_kind_from_json(const json& value) {
    if (value.is_number_integer()) {
        const auto numeric = value.get<int>();
        if (numeric == 1) return MessageAttachmentKind::IMAGE;
        return MessageAttachmentKind::FILE;
    }

    if (value.is_string() && value.get<std::string>() == "image") {
        return MessageAttachmentKind::IMAGE;
    }

    return MessageAttachmentKind::FILE;
}

MessageAttachmentKind attachment_kind_from_mime_type(const std::string& mime_type) {
    if (mime_type.rfind("image/", 0) == 0) {
        return MessageAttachmentKind::IMAGE;
    }

    return MessageAttachmentKind::FILE;
}

std::vector<MessageAttachment> from_json_attachments(const std::string& raw_json) {
    std::vector<MessageAttachment> out;

    const auto parsed = json::parse(raw_json, nullptr, false);
    if (!parsed.is_array()) {
        return out;
    }

    out.reserve(parsed.size());

    for (const auto& item : parsed) {
        if (!item.is_object()) {
            continue;
        }

        MessageAttachment next;
        next.id = item.value("id", "");
        next.kind = attachment_kind_from_json(item.contains("kind") ? item.at("kind") : json{});
        next.storage_bucket = item.value("storage_bucket", "");
        next.storage_key = item.value("storage_key", "");
        next.mime_type = item.value("mime_type", "");
        next.display_name = item.value("display_name", "");
        next.size_bytes = item.value("size_bytes", uint64_t{0});

        if (next.id.empty() || next.storage_key.empty()) {
            continue;
        }

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
    if (!parsed.is_object()) {
        return std::nullopt;
    }

    MessageLinkPreview out;
    out.url = parsed.value("url", "");
    out.title = parsed.value("title", "");
    out.description = parsed.value("description", "");
    out.site_name = parsed.value("site_name", "");
    out.image_url = parsed.value("image_url", "");

    if (out.url.empty()) {
        return std::nullopt;
    }

    return out;
}

Message row_to_message(const pqxx::row& row) {
    Message msg;
    msg.id = MessageId{row[0].as<std::string>()};
    msg.ch_id = ChannelId{row[1].as<std::string>()};
    msg.sender_id = UserId{row[2].as<std::string>("")};
    msg.text = row[3].as<std::string>();
    msg.attachments = from_json_attachments(row[4].as<std::string>(kEmptyAttachmentsJson));

    if (row[5].is_null()) {
        msg.link_preview = std::nullopt;
    } else {
        msg.link_preview = from_json_link_preview(row[5].as<std::string>());
    }

    msg.message_seq = static_cast<uint64_t>(row[6].as<long long>());
    msg.created_at_unix_us = static_cast<uint64_t>(row[7].as<long long>());
    return msg;
}

std::string message_select_list() {
    return std::string{
        "m.id::text, "
        "m.channel_id::text, "
        "COALESCE(m.sender_id::text, '') AS sender_id, "
        "m.content, "
        "COALESCE(a.attachments_json, '[]'::jsonb)::text AS attachments_json, "
        "m.link_preview_json::text, "
        "m.message_seq, "
        "(EXTRACT(EPOCH FROM m.created_at) * 1000000)::bigint AS created_at_unix_us "};
}

std::string message_from_clause() {
    return std::string{"FROM "} + dbschema::kMessages +
           " m "
           "LEFT JOIN LATERAL ("
           "  SELECT jsonb_agg("
           "    jsonb_build_object("
           "      'id', ma.id::text, "
           "      'kind', CASE "
           "        WHEN COALESCE(ma.mime_type, '') LIKE 'image/%' THEN 'image' "
           "        ELSE 'file' "
           "      END, "
           "      'storage_bucket', ma.storage_bucket, "
           "      'storage_key', ma.storage_key, "
           "      'mime_type', COALESCE(ma.mime_type, ''), "
           "      'display_name', ma.file_name, "
           "      'size_bytes', ma.size_bytes "
           "    ) "
           "    ORDER BY ma.created_at ASC, ma.id ASC"
           "  ) AS attachments_json "
           "  FROM " +
           dbschema::kMessageAttachments +
           " ma "
           "  WHERE ma.message_id = m.id "
           ") a ON true ";
}

std::vector<MessageAttachment> insert_attachments(
    pqxx::work& txn, const MessageId& message_id, const UserId& uploaded_by,
    const std::vector<MessageAttachment>& attachments) {
    std::vector<MessageAttachment> inserted;
    inserted.reserve(attachments.size());

    const std::string query =
        std::string{"INSERT INTO "} + dbschema::kMessageAttachments +
        " (message_id, uploaded_by, storage_bucket, storage_key, file_name, mime_type, size_bytes) "
        "VALUES ($1::uuid, $2::uuid, $3, $4, $5, $6, $7) "
        "RETURNING id::text, storage_bucket, storage_key, COALESCE(mime_type, ''), file_name, "
        "size_bytes";

    for (const auto& attachment : attachments) {
        if (attachment.storage_bucket.empty()) {
            throw std::runtime_error("Attachment storage_bucket is required");
        }
        if (attachment.storage_key.empty()) {
            throw std::runtime_error("Attachment storage_key is required");
        }

        const std::string file_name =
            attachment.display_name.empty() ? attachment.storage_key : attachment.display_name;

        const std::optional<std::string> mime_type =
            attachment.mime_type.empty() ? std::nullopt
                                         : std::optional<std::string>{attachment.mime_type};

        auto res = txn.exec(query, pqxx::params{
                                       message_id.value,
                                       uploaded_by.value,
                                       attachment.storage_bucket,
                                       attachment.storage_key,
                                       file_name,
                                       mime_type,
                                       static_cast<long long>(attachment.size_bytes),
                                   });

        if (res.empty()) {
            throw std::runtime_error("insert message attachment failed");
        }

        MessageAttachment next;
        next.id = res[0][0].as<std::string>();
        next.storage_bucket = res[0][1].as<std::string>();
        next.storage_key = res[0][2].as<std::string>();
        next.mime_type = res[0][3].as<std::string>("");
        next.display_name = res[0][4].as<std::string>();
        next.size_bytes = static_cast<uint64_t>(res[0][5].as<long long>());
        next.kind = attachment_kind_from_mime_type(next.mime_type);

        inserted.push_back(std::move(next));
    }

    return inserted;
}

long long cursor_seq(const MessageCursor& cursor) {
    return static_cast<long long>(cursor.message_seq);
}

}  // namespace

Message MessageRepository::sendMessage(const ChannelId& channelId, const UserId& senderUuid,
                                       const std::string& content,
                                       std::vector<MessageAttachment> attachments,
                                       std::optional<MessageLinkPreview> link_preview) {
    return db_.write("MessageRepository.sendMessage", [&](pqxx::work& txn) {
        const auto link_preview_json =
            link_preview.has_value()
                ? std::optional<std::string>(to_json_link_preview(*link_preview).dump())
                : std::nullopt;

        const std::string query =
            std::string{"INSERT INTO "} + dbschema::kMessages +
            " (channel_id, sender_id, content, link_preview_json) "
            "VALUES ($1::uuid, $2::uuid, $3, $4::jsonb) "
            "RETURNING "
            "id::text, "
            "channel_id::text, "
            "COALESCE(sender_id::text, '') AS sender_id, "
            "content, "
            "'[]'::jsonb::text AS attachments_json, "
            "link_preview_json::text, "
            "message_seq, "
            "(EXTRACT(EPOCH FROM created_at) * 1000000)::bigint AS created_at_unix_us";

        auto res = txn.exec(
            query, pqxx::params{channelId.value, senderUuid.value, content, link_preview_json});

        if (res.empty()) {
            throw std::runtime_error("sendMessage failed");
        }

        Message msg = row_to_message(res[0]);
        msg.attachments = insert_attachments(txn, msg.id, senderUuid, attachments);

        return msg;
    });
}

bool MessageRepository::insertMessage(const Message& msg) {
    return db_.write("MessageRepository.insertMessage", [&](pqxx::work& txn) {
        const auto link_preview_json =
            msg.link_preview.has_value()
                ? std::optional<std::string>(to_json_link_preview(*msg.link_preview).dump())
                : std::nullopt;

        const std::string query = std::string{"INSERT INTO "} + dbschema::kMessages +
                                  " (id, channel_id, sender_id, content, link_preview_json) "
                                  "VALUES ($1::uuid, $2::uuid, $3::uuid, $4, $5::jsonb) "
                                  "RETURNING id::text, message_seq";

        auto res = txn.exec(query, pqxx::params{
                                       msg.id.value,
                                       msg.ch_id.value,
                                       msg.sender_id.value,
                                       msg.text,
                                       link_preview_json,
                                   });

        if (res.empty()) {
            return false;
        }

        insert_attachments(txn, msg.id, msg.sender_id, msg.attachments);
        return true;
    });
}

std::vector<Message> MessageRepository::fetchMessages(const ChannelId& channelId, int limit) {
    return db_.read("MessageRepository.fetchMessages", [&](pqxx::work& txn) {
        const std::string query = "SELECT " + message_select_list() + message_from_clause() +
                                  "WHERE m.channel_id = $1::uuid "
                                  "ORDER BY m.message_seq DESC "
                                  "LIMIT $2";

        auto res = txn.exec(query, pqxx::params{channelId.value, limit});

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
        const std::string query = "SELECT " + message_select_list() + message_from_clause() +
                                  "WHERE m.channel_id = $1::uuid "
                                  "AND m.message_seq > $2 "
                                  "ORDER BY m.message_seq ASC "
                                  "LIMIT $3";

        auto res = txn.exec(query, pqxx::params{
                                       channelId.value,
                                       cursor_seq(after),
                                       limit,
                                   });

        std::vector<Message> msgs;
        msgs.reserve(res.size());

        for (const auto& row : res) {
            msgs.push_back(row_to_message(row));
        }

        return msgs;
    });
}

std::vector<Message> MessageRepository::fetchMessagesBefore(const ChannelId& channelId,
                                                            const MessageCursor& before,
                                                            int limit) {
    return db_.read("MessageRepository.fetchMessagesBefore", [&](pqxx::work& txn) {
        const std::string query = "SELECT " + message_select_list() + message_from_clause() +
                                  "WHERE m.channel_id = $1::uuid "
                                  "AND m.message_seq < $2 "
                                  "ORDER BY m.message_seq DESC "
                                  "LIMIT $3";

        auto res = txn.exec(query, pqxx::params{
                                       channelId.value,
                                       cursor_seq(before),
                                       limit,
                                   });

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

        if (after.has_value() && before.has_value()) {
            const std::string query = "SELECT " + message_select_list() + message_from_clause() +
                                      "WHERE m.channel_id = $1::uuid "
                                      "AND m.message_seq > $2 "
                                      "AND m.message_seq < $3 "
                                      "ORDER BY m.message_seq ASC "
                                      "LIMIT $4";

            res = txn.exec(query, pqxx::params{
                                      channelId.value,
                                      cursor_seq(*after),
                                      cursor_seq(*before),
                                      limit,
                                  });
        } else if (after.has_value()) {
            const std::string query = "SELECT " + message_select_list() + message_from_clause() +
                                      "WHERE m.channel_id = $1::uuid "
                                      "AND m.message_seq > $2 "
                                      "ORDER BY m.message_seq ASC "
                                      "LIMIT $3";

            res = txn.exec(query, pqxx::params{
                                      channelId.value,
                                      cursor_seq(*after),
                                      limit,
                                  });
        } else if (before.has_value()) {
            const std::string query = "SELECT " + message_select_list() + message_from_clause() +
                                      "WHERE m.channel_id = $1::uuid "
                                      "AND m.message_seq < $2 "
                                      "ORDER BY m.message_seq DESC "
                                      "LIMIT $3";

            res = txn.exec(query, pqxx::params{
                                      channelId.value,
                                      cursor_seq(*before),
                                      limit,
                                  });
        } else {
            const std::string query = "SELECT " + message_select_list() + message_from_clause() +
                                      "WHERE m.channel_id = $1::uuid "
                                      "ORDER BY m.message_seq DESC "
                                      "LIMIT $2";

            res = txn.exec(query, pqxx::params{channelId.value, limit});
        }

        std::vector<Message> msgs;
        msgs.reserve(res.size());

        for (const auto& row : res) {
            msgs.push_back(row_to_message(row));
        }

        return msgs;
    });
}

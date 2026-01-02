#include "app/commands/JoinHubByInviteCommand.h"

#include "net/PerSocketData.h"

#include <nlohmann/json.hpp>
#include <string>
#include <unordered_set>

using nlohmann::json;

namespace app {

JoinHubByInviteCommand::JoinHubByInviteCommand(ServiceObjects& svc_objs)
    : services_(svc_objs) {}

json JoinHubByInviteCommand::build_members_payload(const HubId& hub_id) {
    json members = json::array();
    auto db_members = services_.db_.hubs().getHubMembers(hub_id);
    std::unordered_set<UserId> online_members;

    // Collect online members from db and dont use connection manager's snapshot
    const auto hub_members = services_.db_.hubs().getHubMembers(hub_id);
    for (const auto&[ user_id, _] : hub_members) {
        online_members.insert(user_id);
    }
    

    for (const auto& [user_id, display_hint] : db_members) {
        std::string display = services_.ids_.display_for(user_id);
        if (display.empty() && !display_hint.empty()) {
            display = display_hint;
            services_.ids_.remember_display(user_id, display);
        }
        if (display.empty()) {
            if (auto db_name = services_.db_.users().getUserDisplayName(user_id)) {
                if (!db_name->empty()) {
                    display = *db_name;
                    services_.ids_.remember_display(user_id, display);
                }
            }
        }
        if (display.empty()) display = "Member";
        const auto public_user = services_.ids_.to_public(user_id);
        members.push_back({{"handle", display},
                           {"display_name", display},
                           {"online", online_members.count(user_id) > 0},
                           {"user_id", public_user.value}});
    }
    return members;
}

json JoinHubByInviteCommand::build_channels_payload(const HubId& hub_id) {
    json channels_json = json::array();
    const auto channels = services_.db_.channels().getHubChannels(hub_id);
    for (const auto& channel : channels) {
        const auto public_channel_id = services_.ids_.to_public(channel.id);
        const auto public_hub_id = services_.ids_.to_public(channel.hub_id);
        std::string type = channel.type == ChannelType::VOICE ? "voice" : "text";
        channels_json.push_back({{"id", public_channel_id.value},
                                 {"hub_id", public_hub_id.value},
                                 {"name", channel.name},
                                 {"type", type}});
    }
    return channels_json;
}

void JoinHubByInviteCommand::execute(CommandContext& ctx) {
    const auto& input = ctx.input;
    auto& output = ctx.output;

    if (!ctx.snapshot.authenticated) {
        output.success = false;
        output.error_code = "not_authenticated";
        output.error_message = "Authentication required.";
        json err = {{"type", "error"},
                    {"code", "not_authenticated"},
                    {"message", "Authentication required"}};
        DirectMessage msg;
        msg.conn_id = ctx.conn_id;
        msg.payload = err.dump();
        ctx.output.messages.push_back(std::move(msg));
        output.sent_at = std::chrono::system_clock::now();
        return;
    }

    const std::string invite_code = input.data.value("invite_code", "");
    if (invite_code.empty()) {
        output.success = false;
        output.error_code = "invalid_invite";
        output.error_message = "Invite code is required.";
        json err = {
            {"type", "error"}, {"code", "invalid_invite"}, {"message", "Invite code is required"}};
        DirectMessage msg;
        msg.conn_id = ctx.conn_id;
        msg.payload = err.dump();
        ctx.output.messages.push_back(std::move(msg));
        output.sent_at = std::chrono::system_clock::now();
        return;
    }

    auto internal_hub = services_.ids_.to_internal(PublicHubId{invite_code});
    if (!internal_hub.has_value()) {
        output.success = false;
        output.error_code = "invite_not_found";
        output.error_message = "Invite code is invalid.";
        json err = {
            {"type", "error"}, {"code", "invite_not_found"}, {"message", "Invite code is invalid"}};
        DirectMessage msg;
        msg.conn_id = ctx.conn_id;
        msg.payload = err.dump();
        ctx.output.messages.push_back(std::move(msg));
        output.sent_at = std::chrono::system_clock::now();
        return;
    }

    auto hub_opt = services_.db_.hubs().getHub(*internal_hub);
    if (!hub_opt.has_value()) {
        output.success = false;
        output.error_code = "hub_not_found";
        output.error_message = "Hub not found.";
        json err = {{"type", "error"}, {"code", "hub_not_found"}, {"message", "Hub not found"}};
        DirectMessage msg;
        msg.conn_id = ctx.conn_id;
        msg.payload = err.dump();
        ctx.output.messages.push_back(std::move(msg));
        output.sent_at = std::chrono::system_clock::now();
        return;
    }

    if (ctx.snapshot.hubs.count(*internal_hub) ||
        services_.db_.hubs().isHubMember(*internal_hub, ctx.snapshot.user_id)) {
        output.success = true;
        output.error_code.clear();
        output.error_message.clear();
        const auto public_hub_id = services_.ids_.to_public(*internal_hub);
        json hub_json = {{"id", public_hub_id.value}, {"name", hub_opt->name}, {"role", "member"}};
        json data = {{"type", "hub_joined"},
                     {"hub", std::move(hub_json)},
                     {"channels", build_channels_payload(*internal_hub)},
                     {"members", build_members_payload(*internal_hub)},
                     {"already_member", true}};
        DirectMessage msg;
        msg.conn_id = ctx.conn_id;
        msg.payload = data.dump();
        ctx.output.messages.push_back(std::move(msg));
        output.sent_at = std::chrono::system_clock::now();
        return;
    }

    try {
        services_.db_.hubs().addMember(*internal_hub, ctx.snapshot.user_id, "member");

        services_.gateway_.subscribe(ctx.conn_id, app::services::HubPublisher::topic_for(*internal_hub));

        const auto public_hub_id = services_.ids_.to_public(*internal_hub);
        json hub_json = {
            {"id", public_hub_id.value},
            {"name", hub_opt->name},
            {"role", "member"},
        };

        services_.hub_publisher_.publish_hub(*internal_hub);

        output.success = true;
        output.error_code.clear();
        output.error_message.clear();
        json data = {{"type", "hub_joined"},
                     {"hub", std::move(hub_json)},
                     {"channels", build_channels_payload(*internal_hub)},
                     {"members", build_members_payload(*internal_hub)},
                     {"already_member", false}};
        DirectMessage msg;
        msg.conn_id = ctx.conn_id;
        msg.payload = data.dump();
        msg.apply_psd = [internal_hub](net::PerSocketData* psd) {
            if (psd) {
                auto snapshot = *psd->snapshot;
                snapshot.hubs.insert(internal_hub.value());
                snapshot.roles[internal_hub.value()] = Role::USER;
                psd->snapshot = std::make_shared<net::Snapshot>(snapshot);
            }
        };
        ctx.output.messages.push_back(std::move(msg));
        output.sent_at = std::chrono::system_clock::now();
    } catch (const std::exception& ex) {
        output.success = false;
        output.error_code = "join_hub_failed";
        output.error_message = ex.what();
        json err = {{"type", "error"}, {"code", "join_hub_failed"}, {"message", ex.what()}};
        DirectMessage msg;
        msg.conn_id = ctx.conn_id;
        msg.payload = err.dump();
        ctx.output.messages.push_back(std::move(msg));
        output.sent_at = std::chrono::system_clock::now();
    }
}

}  // namespace app

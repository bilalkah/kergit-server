#include "app/commands/ChannelUsersCommand.h"

#include "net/ConnectionManager.h"
#include "net/PerSocketData.h"

#include <nlohmann/json.hpp>

using nlohmann::json;

namespace app {

namespace {
std::string safe_display(const net::PerSocketData& psd) {
    if (!psd.username.empty()) return psd.username;
    return "Member";
}

json make_member_payload(const net::PerSocketData& psd) {
    const auto name = safe_display(psd);
    return json{{"handle", name}, {"display_name", name}, {"online", true}};
}
}  // namespace

ChannelUsersCommand::ChannelUsersCommand(net::ConnectionManager& connections)
    : connections_(connections) {}

void ChannelUsersCommand::execute(CommandContext& ctx) {
    auto& psd = ctx.psd;
    auto& output = ctx.output;
    const auto& data = ctx.input.data;

    if (!psd.authenticated) {
        output.success = false;
        output.error_code = "not_authenticated";
        output.error_message = "Authentication required.";
        output.data = {{"type", "error"},
                       {"code", "not_authenticated"},
                       {"message", "Authentication required"}};
        return;
    }

    const std::string scope = data.value("scope", "");
    if (scope != "channel") {
        output.success = false;
        output.error_code = "unsupported_scope";
        output.error_message = "Only channel scope is supported.";
        output.data = {{"type", "error"},
                       {"code", "unsupported_scope"},
                       {"message", "Only channel scope is supported"}};
        return;
    }

    const std::string channel_id_str = data.value("channel_id", "");
    if (channel_id_str.empty()) {
        output.success = false;
        output.error_code = "missing_channel_id";
        output.error_message = "channel_id is required.";
        output.data = {{"type", "error"},
                       {"code", "missing_channel_id"},
                       {"message", "channel_id is required"}};
        return;
    }

    ChannelId channel_id{channel_id_str};
    auto presence = collect_channel_presence(channel_id);

    output.success = true;
    output.error_code.clear();
    output.error_message.clear();
    output.data = {
        {"type", "presence_snapshot"},
        {"channel_id", channel_id.value},
        {"members", std::move(presence)},
    };
    output.sent_at = std::chrono::system_clock::now();
}

json ChannelUsersCommand::collect_channel_presence(const ChannelId& channel_id) const {
    json members = json::array();
    connections_.for_each([&](UwsSocket* ws) {
        if (!ws) return;
        auto* other = ws->getUserData();
        if (!other || !other->authenticated) return;
        if (other->current_channel_id.value != channel_id.value) return;
        members.push_back(make_member_payload(*other));
    });
    return members;
}

}  // namespace app

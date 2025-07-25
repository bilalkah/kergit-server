#pragma once
#include "ICommand.h"

class JoinCommand : public ICommand {
public:
    void execute(json& j, User& user, ChatServer& server, uWS::WebSocket<false, true, struct PerSocketData>* ws) override {
        std::string channel = j["channel"];
        std::string username = j.value("username", user.id);
        user.username = username;
        if (!channel.empty()) {
            if (server.channels.find(channel) == server.channels.end()) {
                server.createChannel(channel, user.id);
            }
            server.joinChannel(user.id, channel);
            json resp = { {"type", "joined"}, {"channel", channel}, {"username", username} };
            ws->send(resp.dump());
            auto history = server.getChannelHistory(channel);
            for (const auto& msg : history) {
                json hist_msg = { {"type", "chat"}, {"sender", msg.sender}, {"text", msg.text}, {"timestamp", msg.timestamp} };
                ws->send(hist_msg.dump());
            }
        } else {
            server.leaveChannel(user.id);
        }
    }
}; 
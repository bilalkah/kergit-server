#pragma once
#include "ICommand.h"

class JoinCommand : public ICommand {
   public:
    using WsToUserMap =
        std::unordered_map<uWS::WebSocket<false, true, struct PerSocketData>*, std::string>;
    JoinCommand(const WsToUserMap& ws_to_user) : ws_to_user(ws_to_user) {}

    void execute(json& j, User& user, ChatServerState& server,
                 uWS::WebSocket<false, true, struct PerSocketData>* ws) override {
        std::string channel = j["channel"];
        std::string username = j.value("username", user.id);
        user.username = username;

        if (!channel.empty()) {
            if (server.channels.find(channel) == server.channels.end()) {
                server.createChannel(channel, user.id);
            }

            // Get current users in channel before joining
            auto& ch = server.channels[channel];
            std::vector<std::string> current_users;
            for (const auto& uid : ch.user_ids) {
                auto user_it = server.users.find(uid);
                if (user_it != server.users.end()) {
                    current_users.push_back(user_it->second.username);
                }
            }

            // Join the channel
            server.joinChannel(user.id, channel);

            // Notify the joining user
            json resp;
            resp["type"] = "joined";
            resp["channel"] = channel;
            resp["username"] = username;
            ws->send(resp.dump(), uWS::OpCode::TEXT);

            // Send current users list to the joining user
            json users_resp;
            users_resp["type"] = "users";
            users_resp["channel"] = channel;
            users_resp["users"] = current_users;
            ws->send(users_resp.dump(), uWS::OpCode::TEXT);

            // Send channel history
            auto history = server.getChannelHistory(channel);
            for (const auto& msg : history) {
                json hist_msg;
                hist_msg["type"] = "chat";
                hist_msg["sender"] = msg.sender;
                hist_msg["text"] = msg.text;
                hist_msg["timestamp"] = msg.timestamp;
                ws->send(hist_msg.dump(), uWS::OpCode::TEXT);
            }

            // Notify other users in the channel that someone joined
            json join_notification;
            join_notification["type"] = "user_joined";
            join_notification["username"] = username;
            join_notification["channel"] = channel;
            std::string join_msg = join_notification.dump();
            for (const auto& uid : ch.user_ids) {
                if (uid != user.id) {  // Don't send to the joining user
                    for (const auto& [ws_ptr, ws_uid] : ws_to_user) {
                        if (ws_uid == uid) {
                            ws_ptr->send(join_msg, uWS::OpCode::TEXT);
                        }
                    }
                }
            }
        } else {
            // Leaving channel
            std::string old_channel = user.current_channel;
            if (!old_channel.empty()) {
                // Notify other users that someone left
                auto& ch = server.channels[old_channel];
                json leave_notification;
                leave_notification["type"] = "user_left";
                leave_notification["username"] = username;
                leave_notification["channel"] = old_channel;
                std::string leave_msg = leave_notification.dump();
                for (const auto& uid : ch.user_ids) {
                    if (uid != user.id) {  // Don't send to the leaving user
                        for (const auto& [ws_ptr, ws_uid] : ws_to_user) {
                            if (ws_uid == uid) {
                                ws_ptr->send(leave_msg, uWS::OpCode::TEXT);
                            }
                        }
                    }
                }
            }
            server.leaveChannel(user.id);

            // If the channel is now empty and it's a temp channel, destroy it
            if (!old_channel.empty() && old_channel == "temp") {
                auto ch_it = server.channels.find(old_channel);
                if (ch_it != server.channels.end() && ch_it->second.user_ids.empty()) {
                    server.channels.erase(ch_it);
                }
            }
        }
    }

   private:
    const WsToUserMap& ws_to_user;
};
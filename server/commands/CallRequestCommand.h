#pragma once
#include "ICommand.h"
#include "common/Call.h"

class CallRequestCommand : public ICommand {
   public:
    using WsToUserMap =
        std::unordered_map<uWS::WebSocket<false, true, struct PerSocketData>*, std::string>;
    CallRequestCommand(const WsToUserMap& ws_to_user) : ws_to_user(ws_to_user) {}

    void execute(json& j, User& user, ChatServerState& server,
                 uWS::WebSocket<false, true, struct PerSocketData>* ws) override {
        std::string target_user = j["target_user"];
        std::string media_type = j["media_type"];  // "voice", "video"

        std::cout << "[CALL] User '" << user.username << "' (ID: " << user.id << ") requesting "
                  << media_type << " call to '" << target_user << "'" << std::endl;

        // Find target user by username
        std::string target_user_id;
        for (const auto& [user_id, user] : server.users) {
            if (user.username == target_user) {
                target_user_id = user_id;
                break;
            }
        }

        if (target_user_id.empty()) {
            std::cout << "[CALL] Target user '" << target_user << "' not found" << std::endl;
            json resp;
            resp["type"] = "call_error";
            resp["message"] = "Target user not found";
            ws->send(resp.dump(), uWS::OpCode::TEXT);
            return;
        }

        std::cout << "[CALL] Found target user '" << target_user << "' with ID: " << target_user_id
                  << std::endl;

        // Check if target user is already in a call
        if (server.isUserInCall(target_user_id)) {
            json resp;
            resp["type"] = "call_error";
            resp["message"] = "Target user is busy";
            ws->send(resp.dump(), uWS::OpCode::TEXT);
            return;
        }

        // Determine call type
        CallType call_type = (media_type == "voice") ? CallType::VOICE : CallType::VIDEO;

        // Create the call
        std::string call_id = server.createCall(user.id, target_user_id, call_type);

        // Send call request to target user
        json call_request;
        call_request["type"] = "call_incoming";
        call_request["call_id"] = call_id;
        call_request["from_user"] = user.username;
        call_request["media_type"] = media_type;

        // Find target user's WebSocket connection
        bool target_found = false;
        for (const auto& [ws_ptr, ws_uid] : ws_to_user) {
            if (ws_uid == target_user_id) {
                std::cout << "[CALL] Sending call request to target user '" << target_user << "'"
                          << std::endl;
                ws_ptr->send(call_request.dump(), uWS::OpCode::TEXT);
                target_found = true;
                break;
            }
        }

        if (!target_found) {
            std::cout << "[CALL] Target user '" << target_user
                      << "' not connected (no WebSocket found)" << std::endl;
        }

        // Send confirmation to initiator
        json resp;
        resp["type"] = "call_requested";
        resp["call_id"] = call_id;
        resp["target_user"] = target_user;
        ws->send(resp.dump(), uWS::OpCode::TEXT);
    }

   private:
    const WsToUserMap& ws_to_user;
};
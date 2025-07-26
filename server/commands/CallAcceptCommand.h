#pragma once
#include "ICommand.h"
#include "common/Call.h"

class CallAcceptCommand : public ICommand {
   public:
    using WsToUserMap =
        std::unordered_map<uWS::WebSocket<false, true, struct PerSocketData>*, std::string>;
    CallAcceptCommand(const WsToUserMap& ws_to_user) : ws_to_user(ws_to_user) {}

    void execute(json& j, User& user, ChatServerState& server,
                 uWS::WebSocket<false, true, struct PerSocketData>* ws) override {
        // TODO: Implement call acceptance handling
        std::string call_id = j["call_id"];

        // Get the call
        Call* call = server.getCall(call_id);
        if (!call) {
            json resp;
            resp["type"] = "call_error";
            resp["message"] = "Call not found";
            ws->send(resp.dump(), uWS::OpCode::TEXT);
            return;
        }

        // Check if user is part of this call
        if (!call->is_participant(user.id)) {
            json resp;
            resp["type"] = "call_error";
            resp["message"] = "Not part of this call";
            ws->send(resp.dump(), uWS::OpCode::TEXT);
            return;
        }

        // Accept the call
        if (server.acceptCall(call_id, user.id)) {
            // Notify all participants
            json call_accepted;
            call_accepted["type"] = "call_accepted";
            call_accepted["call_id"] = call_id;
            call_accepted["accepted_by"] = user.username;

            for (const auto& participant_id : call->participant_ids) {
                for (const auto& [ws_ptr, ws_uid] : ws_to_user) {
                    if (ws_uid == participant_id) {
                        ws_ptr->send(call_accepted.dump(), uWS::OpCode::TEXT);
                    }
                }
            }
        } else {
            json resp;
            resp["type"] = "call_error";
            resp["message"] = "Failed to accept call";
            ws->send(resp.dump(), uWS::OpCode::TEXT);
        }
    }

   private:
    const WsToUserMap& ws_to_user;
};
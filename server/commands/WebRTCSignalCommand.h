#pragma once
#include "ICommand.h"
#include "common/Call.h"

class WebRTCSignalCommand : public ICommand {
   public:
    using WsToUserMap =
        std::unordered_map<uWS::WebSocket<false, true, struct PerSocketData>*, std::string>;
    WebRTCSignalCommand(const WsToUserMap& ws_to_user) : ws_to_user(ws_to_user) {}

    void execute(json& j, User& user, ChatServerState& server,
                 uWS::WebSocket<false, true, struct PerSocketData>* ws) override {
        // TODO: Implement WebRTC signaling handling
        std::string call_id = j["call_id"];
        std::string signal_type = j["signal_type"];  // "offer", "answer", "ice_candidate"

        // Get the call
        Call* call = server.getCall(call_id);
        if (!call) {
            json resp;
            resp["type"] = "webrtc_error";
            resp["message"] = "Call not found";
            ws->send(resp.dump(), uWS::OpCode::TEXT);
            return;
        }

        // Check if user is part of this call
        if (!call->is_participant(user.id)) {
            json resp;
            resp["type"] = "webrtc_error";
            resp["message"] = "Not part of this call";
            ws->send(resp.dump(), uWS::OpCode::TEXT);
            return;
        }

        // Handle different signal types
        if (signal_type == "offer" || signal_type == "answer") {
            std::string sdp_data = j["sdp"];
            MessageType msg_type =
                (signal_type == "offer") ? MessageType::WEBRTC_OFFER : MessageType::WEBRTC_ANSWER;

            if (server.addWebRTCSignal(call_id, user.id, sdp_data, msg_type)) {
                // Forward to other participants
                json webrtc_signal;
                webrtc_signal["type"] = "webrtc_signal";
                webrtc_signal["call_id"] = call_id;
                webrtc_signal["signal_type"] = signal_type;
                webrtc_signal["sdp"] = sdp_data;
                webrtc_signal["from_user"] = user.username;

                for (const auto& participant_id : call->participant_ids) {
                    if (participant_id != user.id) {  // Don't send back to sender
                        for (const auto& [ws_ptr, ws_uid] : ws_to_user) {
                            if (ws_uid == participant_id) {
                                ws_ptr->send(webrtc_signal.dump(), uWS::OpCode::TEXT);
                            }
                        }
                    }
                }
            }
        } else if (signal_type == "ice_candidate") {
            std::string candidate = j["candidate"];

            if (server.addIceCandidate(call_id, user.id, candidate)) {
                // Forward to other participants
                json ice_signal;
                ice_signal["type"] = "ice_candidate";
                ice_signal["call_id"] = call_id;
                ice_signal["candidate"] = candidate;
                ice_signal["from_user"] = user.username;

                for (const auto& participant_id : call->participant_ids) {
                    if (participant_id != user.id) {  // Don't send back to sender
                        for (const auto& [ws_ptr, ws_uid] : ws_to_user) {
                            if (ws_uid == participant_id) {
                                ws_ptr->send(ice_signal.dump(), uWS::OpCode::TEXT);
                            }
                        }
                    }
                }
            }
        } else {
            json resp;
            resp["type"] = "webrtc_error";
            resp["message"] = "Unknown signal type";
            ws->send(resp.dump(), uWS::OpCode::TEXT);
        }
    }

   private:
    const WsToUserMap& ws_to_user;
};
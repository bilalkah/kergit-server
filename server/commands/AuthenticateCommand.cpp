#include "AuthenticateCommand.h"

#include "server/MessageFilters.h"

#include <functional>
#include <iostream>
#include <sstream>

void AuthenticateCommand::execute(json& message, User& user, ChatServerState& server_state,
                                  WS* ws) {
    std::string auth_type = message.value("auth_type", "");
    std::cerr << "[AUTH] Execute auth type=" << auth_type << std::endl;

    // Check if JWT token is provided (new Supabase auth flow)
    if (message.contains("token") && !message["token"].get<std::string>().empty()) {
        handle_jwt_auth(message, user, server_state, ws);
    } else {
        std::cerr << "[AUTH] Invalid authentication type: " << auth_type << std::endl;
        send_auth_response(ws, false, UserId(ws->getUserData()->user_id),
                           "Invalid authentication type");
    }
}

// Assumptions:
// - jwt_verifier_->verify_token(token) returns a struct with at least:
//     .sub (Supabase user UUID), .email (optional), .username (optional)
//   If your verifier returns only the decoded JWT, read "sub" from claims.
// - send_auth_response(ws, ok, user_uuid, err?) takes the Supabase UUID.
// - send_init_state(ws, user_uuid) expects a UUID (string). If your current
//   signature takes an int, make an overload that takes std::string uuid.
bool AuthenticateCommand::handle_jwt_auth(const json& message, User& user,
                                          ChatServerState& server_state, WS* ws) {
    const std::string token = message.value("token", "");
    const std::string username_ = message.value("username", "");
    const std::string auth_type = message.value("auth_type", "");

    std::cerr << "[AUTH] JWT auth attempt (supabase) auth_type=" << auth_type << std::endl;

    if (token.empty()) {
        send_auth_response(ws, false, UserId(ws->getUserData()->user_id), "JWT token required");
        return false;
    }
    if (!jwt_verifier_) {
        send_auth_response(ws, false, UserId(ws->getUserData()->user_id),
                           "JWT authentication not configured");
        return false;
    }

    try {
        // 1️⃣ Verify token and get SupabaseUser
        auto supa = jwt_verifier_->verify_token(token);
        if (!supa.has_value()) {
            send_auth_response(ws, false, UserId(ws->getUserData()->user_id), "Invalid JWT token");
            return false;
        }

        if (jwt_verifier_->is_token_expired(token)) {
            send_auth_response(ws, false, UserId(ws->getUserData()->user_id), "JWT token expired");
            return false;
        }

        user = get_or_create_user(supa.value(), server_state);

        // 4️⃣ Reply and send initial state
        send_auth_response(ws, true, user.id);
        ws->getUserData()->user_id = user.id.value;

        std::cout << "[AUTH] User authenticated: " << user.id.value << " (" << user.username << ")\n";

        if (db) {
            send_init_state(ws, user.id);
        } else {
            std::cerr << "[AUTH] Database not available; skipping init state\n";
        }

        return true;

    } catch (const std::exception& e) {
        std::cerr << "[AUTH] JWT auth error: " << e.what() << std::endl;
        send_auth_response(ws, false, UserId(ws->getUserData()->user_id), std::string("JWT authentication error: ") + e.what());
        return false;
    }
}

void AuthenticateCommand::send_auth_response(WS* ws, bool success, const UserId& user_id,
                                             const std::string& error) {
    json response;
    response["type"] = "auth_response";
    response["success"] = success;

    if (success) {
        response["user_id"] = user_id.value;
        response["message"] = "Authentication successful";
    } else {
        response["error"] = error;
    }

    send_json(ws, response, uWS::OpCode::TEXT);
}

void AuthenticateCommand::send_init_state(WS* ws, UserId userId) {
    if (!db) return;
    // Hubs
    auto hubs = db->getUserHubs(userId);
    json hubsMsg;
    hubsMsg["type"] = "hubs";
    json hubsArr = json::array();
    for (const auto& h : hubs) {
        json hobj;
        hobj["id"] = h.id;
        hobj["name"] = h.name;
        hubsArr.push_back(hobj);
    }
    hubsMsg["hubs"] = hubsArr;
    send_json(ws, hubsMsg, uWS::OpCode::TEXT);

    // For each hub, send channels
    for (const auto& h : hubs) {
        auto chans = db->getHubChannels(h.id);
        json chansMsg;
        chansMsg["type"] = "channels_for_hub";
        chansMsg["hub_id"] = h.id;
        json carr = json::array();
        for (const auto& c : chans) {
            json cobj;
            cobj["id"] = c.id;
            cobj["name"] = c.name;
            cobj["type"] = c.type;
            carr.push_back(cobj);
        }
        chansMsg["channels"] = carr;
        send_json(ws, chansMsg, uWS::OpCode::TEXT);
    }
}

User& AuthenticateCommand::get_or_create_user(const SupabaseUser& supa,
                                              ChatServerState& server_state) {
    auto it = server_state.users.find(supa.id);
    if (it != server_state.users.end()) {
        return it->second;
    } else {
        User new_user;
        new_user.id = supa.id;
        new_user.username = !supa.username.empty() ? supa.username : supa.email;
        new_user.full_name = supa.full_name;
        new_user.email = supa.email;
        server_state.users[new_user.id] = new_user;
        return server_state.users[new_user.id];
    }
}

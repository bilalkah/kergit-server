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
        send_auth_response(ws, false, "", "Invalid authentication type");
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
        send_auth_response(ws, false, "", "JWT token required");
        return false;
    }
    if (!jwt_verifier_) {
        send_auth_response(ws, false, "", "JWT authentication not configured");
        return false;
    }

    try {
        // 1️⃣ Verify token and get SupabaseUser
        auto supa = jwt_verifier_->verify_token(token);
        if (!supa.has_value()) {
            send_auth_response(ws, false, "", "Invalid JWT token");
            return false;
        }

        if (jwt_verifier_->is_token_expired(token)) {
            send_auth_response(ws, false, "", "JWT token expired");
            return false;
        }

        const std::string user_uuid = supa->id;  // ✅ Supabase user UUID
        const std::string email = supa->email;
        const std::string handle =
            !username_.empty()
                ? username_
                : (!supa->username.empty() ? supa->username
                                           : (!email.empty() ? email : "unknown_user"));

        std::cerr << "[AUTH] JWT verified: uid=" << user_uuid
                  << (email.empty() ? "" : (", email=" + email)) << std::endl;

        // 3️⃣ Add user to in-memory server state
        user.id = user_uuid;
        user.username = handle;
        server_state.users[user.id] = user;

        std::cerr << "[AUTH] JWT auth success: uid=" << user.id
                  << (user.username.empty() ? "" : (", name=" + user.username)) << std::endl;

        // 4️⃣ Reply and send initial state
        send_auth_response(ws, true, user.id);

        if (db) {
            send_init_state(ws, user.id);
        } else {
            std::cerr << "[AUTH] Database not available; skipping init state\n";
        }

        return true;

    } catch (const std::exception& e) {
        std::cerr << "[AUTH] JWT auth error: " << e.what() << std::endl;
        send_auth_response(ws, false, "", std::string("JWT authentication error: ") + e.what());
        return false;
    }
}

void AuthenticateCommand::send_auth_response(WS* ws, bool success, const std::string& user_id,
                                             const std::string& error) {
    json response;
    response["type"] = "auth_response";
    response["success"] = success;

    if (success) {
        response["user_id"] = user_id;
        response["message"] = "Authentication successful";
    } else {
        response["error"] = error;
    }

    send_json(ws, response, uWS::OpCode::TEXT);
}

void AuthenticateCommand::send_init_state(WS* ws, std::string userId) {
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

#include "AuthenticateCommand.h"
#include "server/MessageFilters.h"

#include <functional>
#include <sstream>
#include <iostream>

void AuthenticateCommand::execute(json& message, User& user, ChatServerState& server_state,
                                  uWS::WebSocket<false, true, struct PerSocketData>* ws) {
    std::string auth_type = message.value("auth_type", "");
    std::cerr << "[AUTH] Execute auth type=" << auth_type << std::endl;

    if (auth_type == "login") {
        handle_login(message, user, server_state, ws);
    } else if (auth_type == "register") {
        handle_register(message, user, server_state, ws);
    } else {
        std::cerr << "[AUTH] Invalid authentication type: " << auth_type << std::endl;
        send_auth_response(ws, false, "", "Invalid authentication type");
    }
}

bool AuthenticateCommand::handle_login(const json& message, User& user,
                                       ChatServerState& server_state,
                                       uWS::WebSocket<false, true, struct PerSocketData>* ws) {
    std::string username = message.value("username", "");
    std::cerr << "[AUTH] Login attempt: username=" << username << std::endl;
    std::string password = message.value("password", "");

    if (username.empty() || password.empty()) {
        send_auth_response(ws, false, "", "Username and password required");
        return false;
    }

    if (!db) {
        send_auth_response(ws, false, "", "Server database unavailable");
        return false;
    }

    try {
        auto stored_hash = db->findPasswordHashByUsername(username);
        if (!stored_hash.has_value()) {
            std::cerr << "[AUTH] User not found: " << username << std::endl;
            send_auth_response(ws, false, "", "User not found");
            return false;
        }

        if (!verify_password(password, stored_hash.value())) {
            std::cerr << "[AUTH] Invalid password for: " << username << std::endl;
            send_auth_response(ws, false, "", "Invalid password");
            return false;
        }

        // Login successful - add user to server state
        std::string connection_id = user.id;  // connection ID
        User authenticated_user;
        authenticated_user.id = connection_id;
        authenticated_user.username = username;
        server_state.users[connection_id] = authenticated_user;
        std::cerr << "[AUTH] Login success: username=" << username << ", conn_id=" << connection_id << std::endl;

        send_auth_response(ws, true, connection_id);

        // Send initialization data
        auto userId = db->findUserIdByUsername(username);
        if (userId.has_value()) {
            send_init_state(ws, userId.value());
        }
    } catch (const std::exception& e) {
        std::cerr << "[AUTH] DB error during login: " << e.what() << std::endl;
        send_auth_response(ws, false, "", std::string("Database error during login: ") + e.what());
        return false;
    }

    return true;
}

bool AuthenticateCommand::handle_register(const json& message, User& user,
                                          ChatServerState& server_state,
                                          uWS::WebSocket<false, true, struct PerSocketData>* ws) {
    std::string username = message.value("username", "");
    std::string password = message.value("password", "");
    std::string email = message.value("email", "");
    std::cerr << "[AUTH] Register attempt: username=" << username << ", email=" << email << std::endl;

    if (username.empty() || password.empty()) {
        send_auth_response(ws, false, "", "Username and password required");
        return false;
    }

    if (password.length() < 6) {
        send_auth_response(ws, false, "", "Password must be at least 6 characters");
        return false;
    }

    if (!db) {
        send_auth_response(ws, false, "", "Server database unavailable");
        return false;
    }

    try {
        // Check if user exists
        auto existingId = db->findUserIdByUsername(username);
        if (existingId.has_value()) {
            std::cerr << "[AUTH] Registration failed, username exists: " << username << std::endl;
            send_auth_response(ws, false, "", "Username already exists");
            return false;
        }

        // Create user
        int newUserId = db->createUser(username, hash_password(password), email);
        std::cerr << "[AUTH] Registration success: username=" << username << ", user_id=" << newUserId << std::endl;

        // Ensure personal hub with general channel
        db->ensurePersonalHubWithGeneral(newUserId);

        // Auto-login
        std::string connection_id = user.id;
        User authenticated_user;
        authenticated_user.id = connection_id;
        authenticated_user.username = username;
        server_state.users[connection_id] = authenticated_user;

        send_auth_response(ws, true, connection_id);
        send_init_state(ws, newUserId);
    } catch (const std::exception& e) {
        std::cerr << "[AUTH] DB error during registration: " << e.what() << std::endl;
        send_auth_response(ws, false, "", std::string("Database error during registration: ") + e.what());
        return false;
    }

    return true;
}

void AuthenticateCommand::send_auth_response(uWS::WebSocket<false, true, struct PerSocketData>* ws,
                                             bool success, const std::string& user_id,
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

void AuthenticateCommand::send_init_state(uWS::WebSocket<false, true, struct PerSocketData>* ws, int userId) {
    if (!db) return;
    // Hubs
    auto hubs = db->getUserHubs(userId);
    json hubsMsg;
    hubsMsg["type"] = "hubs";
    json hubsArr = json::array();
    for (const auto &h : hubs) {
        json hobj;
        hobj["id"] = h.id;
        hobj["name"] = h.name;
        hubsArr.push_back(hobj);
    }
    hubsMsg["hubs"] = hubsArr;
    send_json(ws, hubsMsg, uWS::OpCode::TEXT);

    // For each hub, send channels
    for (const auto &h : hubs) {
        auto chans = db->getHubChannels(h.id);
        json chansMsg;
        chansMsg["type"] = "channels_for_hub";
        chansMsg["hub_id"] = h.id;
        json carr = json::array();
        for (const auto &c : chans) {
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

std::string AuthenticateCommand::hash_password(const std::string& password) {
    // Simple hash for demo - use bcrypt in production!
    std::hash<std::string> hasher;
    size_t hash = hasher(password + "simple_salt");

    std::stringstream ss;
    ss << std::hex << hash;
    return ss.str();
}

bool AuthenticateCommand::verify_password(const std::string& password, const std::string& hash) {
    return hash_password(password) == hash;
}

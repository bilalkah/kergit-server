#include "AuthenticateCommand.h"

#include <functional>
#include <iomanip>
#include <random>
#include <sstream>

// Static member initialization
std::unordered_map<std::string, std::string> AuthenticateCommand::users_;
std::unordered_map<std::string, std::string> AuthenticateCommand::user_emails_;

AuthenticateCommand::AuthenticateCommand() {
    // Initialize with a default admin user for testing
    if (users_.empty()) {
        users_["admin"] = hash_password("admin123");
        user_emails_["admin"] = "admin@example.com";
    }
}

void AuthenticateCommand::execute(json& message, User& user, ChatServerState& server_state,
                                  uWS::WebSocket<false, true, struct PerSocketData>* ws) {
    std::string auth_type = message.value("auth_type", "");

    if (auth_type == "login") {
        handle_login(message, user, server_state, ws);
    } else if (auth_type == "register") {
        handle_register(message, user, server_state, ws);
    } else {
        send_auth_response(ws, false, "", "Invalid authentication type");
    }
}

bool AuthenticateCommand::handle_login(const json& message, User& user,
                                       ChatServerState& server_state,
                                       uWS::WebSocket<false, true, struct PerSocketData>* ws) {
    std::string username = message.value("username", "");
    std::string password = message.value("password", "");

    if (username.empty() || password.empty()) {
        send_auth_response(ws, false, "", "Username and password required");
        return false;
    }

    auto user_it = users_.find(username);
    if (user_it == users_.end()) {
        send_auth_response(ws, false, "", "User not found");
        return false;
    }

    if (!verify_password(password, user_it->second)) {
        send_auth_response(ws, false, "", "Invalid password");
        return false;
    }

    // Login successful - add user to server state
    std::string connection_id = user.id;  // This is the connection ID
    User authenticated_user;
    authenticated_user.id = connection_id;
    authenticated_user.username = username;
    server_state.users[connection_id] = authenticated_user;

    send_auth_response(ws, true, connection_id);
    return true;
}

bool AuthenticateCommand::handle_register(const json& message, User& user,
                                          ChatServerState& server_state,
                                          uWS::WebSocket<false, true, struct PerSocketData>* ws) {
    std::string username = message.value("username", "");
    std::string password = message.value("password", "");
    std::string email = message.value("email", "");

    if (username.empty() || password.empty()) {
        send_auth_response(ws, false, "", "Username and password required");
        return false;
    }

    // Check if user already exists
    if (users_.find(username) != users_.end()) {
        send_auth_response(ws, false, "", "Username already exists");
        return false;
    }

    // Validate password strength (basic check)
    if (password.length() < 6) {
        send_auth_response(ws, false, "", "Password must be at least 6 characters");
        return false;
    }

    // Register the user
    users_[username] = hash_password(password);
    if (!email.empty()) {
        user_emails_[username] = email;
    }

    // Auto-login after registration - add user to server state
    std::string connection_id = user.id;  // This is the connection ID
    User authenticated_user;
    authenticated_user.id = connection_id;
    authenticated_user.username = username;
    server_state.users[connection_id] = authenticated_user;

    send_auth_response(ws, true, connection_id);
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

    ws->send(response.dump(), uWS::OpCode::TEXT);
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

std::string AuthenticateCommand::generate_user_id(const std::string& username) {
    std::hash<std::string> hasher;
    size_t hash = hasher(username + std::to_string(std::time(nullptr)));

    std::stringstream ss;
    ss << "user_" << std::hex << hash;
    return ss.str();
}

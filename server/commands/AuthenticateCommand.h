#pragma once
#include "ICommand.h"

#include <string>
#include <unordered_map>

class AuthenticateCommand : public ICommand {
   public:
    AuthenticateCommand();

    void execute(json& message, User& user, ChatServerState& server_state,
                 uWS::WebSocket<false, true, struct PerSocketData>* ws) override;

   private:
    // In-memory user store for demo (would be database in production)
    static std::unordered_map<std::string, std::string> users_;        // username -> password_hash
    static std::unordered_map<std::string, std::string> user_emails_;  // username -> email

    bool handle_login(const json& message, User& user, ChatServerState& server_state,
                      uWS::WebSocket<false, true, struct PerSocketData>* ws);
    bool handle_register(const json& message, User& user, ChatServerState& server_state,
                         uWS::WebSocket<false, true, struct PerSocketData>* ws);
    void send_auth_response(uWS::WebSocket<false, true, struct PerSocketData>* ws, bool success,
                            const std::string& user_id = "", const std::string& error = "");

    // Simple password hashing for demo
    std::string hash_password(const std::string& password);
    bool verify_password(const std::string& password, const std::string& hash);
    std::string generate_user_id(const std::string& username);
};

#pragma once
#include "core/database/src/chatdb.h"
#include "server/commands/ICommand.h"
#include "core/security/supabase_jwt_verifier/SupabaseJWTVerifier.h"

#include <string>
#include <unordered_map>
#include <memory>

class AuthenticateCommand : public ICommand {
   public:
    explicit AuthenticateCommand(ChatDB* db_ptr, std::unique_ptr<SupabaseJWTVerifier> jwt_verifier = nullptr) 
        : db(db_ptr), jwt_verifier_(std::move(jwt_verifier)) {}

    void execute(json& message, User& user, ChatServerState& server_state, WS* ws) override;

   private:
    ChatDB* db;  // not owned
    std::unique_ptr<SupabaseJWTVerifier> jwt_verifier_;

    bool handle_jwt_auth(const json& message, User& user, ChatServerState& server_state, WS* ws);
    bool handle_login(const json& message, User& user, ChatServerState& server_state, WS* ws);
    bool handle_register(const json& message, User& user, ChatServerState& server_state, WS* ws);
    void send_auth_response(WS* ws, bool success, const std::string& user_id = "",
                            const std::string& error = "");
    void send_init_state(WS* ws, int userId);

    // Simple password hashing for demo
    std::string hash_password(const std::string& password);
    bool verify_password(const std::string& password, const std::string& hash);
};

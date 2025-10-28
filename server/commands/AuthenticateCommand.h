#pragma once
#include "core/database/src/chatdb.h"
#include "core/security/supabase_jwt_verifier/SupabaseJWTVerifier.h"
#include "server/commands/ICommand.h"

#include <memory>
#include <string>
#include <unordered_map>

class AuthenticateCommand : public ICommand {
   public:
    explicit AuthenticateCommand(ChatDB* db_ptr,
                                 std::unique_ptr<SupabaseJWTVerifier> jwt_verifier = nullptr)
        : db(db_ptr), jwt_verifier_(std::move(jwt_verifier)) {}

    void execute(json& message, User& user, ChatServerState& server_state, WS* ws) override;

   private:
    ChatDB* db;  // not owned
    std::unique_ptr<SupabaseJWTVerifier> jwt_verifier_;

    bool handle_jwt_auth(const json& message, User& user, ChatServerState& server_state, WS* ws);
    void send_auth_response(WS* ws, bool success, const UserId& user_id,
                            const std::string& error = "");
    void send_init_state(WS* ws, UserId userId);
    User& get_or_create_user(const SupabaseUser& supa, ChatServerState& server_state);
};

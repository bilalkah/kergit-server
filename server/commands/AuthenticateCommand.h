#pragma once
#include "core/database/src/chatdb.h"
#include "server/commands/ICommand.h"

#include <string>
#include <unordered_map>

class AuthenticateCommand : public ICommand {
   public:
    explicit AuthenticateCommand(ChatDB* db_ptr) : db(db_ptr) {}

    void execute(json& message, User& user, ChatServerState& server_state, WS* ws) override;

   private:
    ChatDB* db;  // not owned

    bool handle_login(const json& message, User& user, ChatServerState& server_state, WS* ws);
    bool handle_register(const json& message, User& user, ChatServerState& server_state, WS* ws);
    void send_auth_response(WS* ws, bool success, const std::string& user_id = "",
                            const std::string& error = "");
    void send_init_state(WS* ws, int userId);

    // Simple password hashing for demo
    std::string hash_password(const std::string& password);
    bool verify_password(const std::string& password, const std::string& hash);
};

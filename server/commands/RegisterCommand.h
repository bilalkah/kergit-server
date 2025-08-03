#pragma once
#include "ICommand.h"

class RegisterCommand : public ICommand {
   public:
    RegisterCommand() {}

    void execute(json& j, User& user, ChatServerState& server,
                 uWS::WebSocket<false, true, struct PerSocketData>* ws) override {
        std::string username = j.value("username", "");
        std::string password = j.value("password", "");
        std::string email = j.value("email", "");

        if (username.empty() || password.empty()) {
            json error_resp;
            error_resp["type"] = "error";
            error_resp["message"] = "Username and password required";
            ws->send(error_resp.dump(), uWS::OpCode::TEXT);
            return;
        }

        // For now, just accept registration without actual persistence
        // In production, this would validate and store in database
        json success_resp;
        success_resp["type"] = "register_success";
        success_resp["username"] = username;
        success_resp["message"] = "Registration successful";
        ws->send(success_resp.dump(), uWS::OpCode::TEXT);

        // Update user information
        user.username = username;
    }
};

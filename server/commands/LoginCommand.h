#pragma once
#include "ICommand.h"
#include "core/security/authentication/Authentication.h"

class LoginCommand : public ICommand {
   public:
    LoginCommand(Authentication& auth) : auth(auth) {}

    void execute(json& j, User& user, ChatServerState& server,
                 uWS::WebSocket<false, true, struct PerSocketData>* ws) override {
        std::string username = j.value("username", "");
        std::string password = j.value("password", "");

        if (username.empty() || password.empty()) {
            json error_resp;
            error_resp["type"] = "error";
            error_resp["message"] = "Username and password required";
            ws->send(error_resp.dump(), uWS::OpCode::TEXT);
            return;
        }

        // Authenticate the user
        if (auth.authenticate_user(username, password)) {
            // Login successful
            auth.update_last_login(user.id);

            json success_resp;
            success_resp["type"] = "login_success";
            success_resp["username"] = username;
            success_resp["message"] = "Login successful";
            ws->send(success_resp.dump(), uWS::OpCode::TEXT);

            // Update user information
            user.username = username;
        } else {
            // Login failed
            json error_resp;
            error_resp["type"] = "error";
            error_resp["message"] = "Invalid username or password";
            ws->send(error_resp.dump(), uWS::OpCode::TEXT);
        }
    }

   private:
    Authentication& auth;
};

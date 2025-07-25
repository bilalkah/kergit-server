#pragma once
#include "ICommand.h"

class ListCommand : public ICommand {
public:
    void execute(json& j, User& user, ChatServer& server, uWS::WebSocket<false, true, struct PerSocketData>* ws) override {
        auto names = server.listChannels();
        json resp = { {"type", "channels"}, {"channels", names} };
        ws->send(resp.dump());
    }
}; 
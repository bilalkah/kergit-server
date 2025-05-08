#include <uwebsockets/App.h>
#include <iostream>
#include <nlohmann/json.hpp>
#include <thread>

using json = nlohmann::json;
using namespace std;

int main() {
    std::string username = "User" + std::to_string(rand() % 1000);

    uWS::App().connect("ws://localhost:8765", nullptr, {}, [&](auto *ws, auto *req) {
        cout << "[CONNECTED]\n";

        // Join room
        json join_msg = {
            {"type", "join"},
            {"room", "lobby"}
        };
        ws->send(join_msg.dump(), uWS::OpCode::TEXT);

        // Input thread to send messages
        std::thread input_thread([ws, &username]() {
            std::string line;
            while (getline(cin, line)) {
                json chat_msg = {
                    {"type", "chat"},
                    {"sender", username},
                    {"text", line}
                };
                ws->send(chat_msg.dump(), uWS::OpCode::TEXT);
            }
        });

        input_thread.detach();
    },
    [&](auto *ws, std::string_view msg, uWS::OpCode) {
        try {
            auto j = json::parse(msg);
            if (j["type"] == "chat") {
                cout << j["sender"] << ": " << j["text"] << endl;
            } else if (j["type"] == "joined") {
                cout << "Joined room: " << j["room"] << "\n";
            }
        } catch (...) {
            cerr << "[ERROR] Invalid JSON\n";
        }
    },
    [&](auto *ws, int code, std::string_view msg) {
        cout << "[DISCONNECTED]\n";
    });

    return 0;
}
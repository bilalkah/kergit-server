#include "ChatServerApp.h"
#include <iostream>

using json = nlohmann::json;

ChatServerApp::ChatServerApp(int port) : port(port) {
    setup_commands();
}

ChatServerApp::~ChatServerApp() {
    stop();
}

bool ChatServerApp::start() {
    if (running) return true;
    
    running = true;
    server_thread = std::thread(&ChatServerApp::run_server, this);
    
    // Wait a bit for server to start
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    return true;
}

void ChatServerApp::stop() {
    if (!running) return;
    
    running = false;
    
    try {
        // Stop the uWS server if it exists
        if (app) {
            app->close();
            app.reset();
        }
        
        // Wait for server thread to finish with a short timeout
        if (server_thread.joinable()) {
            auto start = std::chrono::steady_clock::now();
            while (server_thread.joinable() && 
                   std::chrono::steady_clock::now() - start < std::chrono::milliseconds(500)) {
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
            
            if (server_thread.joinable()) {
                // If thread is still running, detach it to avoid hanging
                server_thread.detach();
            }
        }
        
        // Wait a bit more to ensure the server is fully stopped
        std::this_thread::sleep_for(std::chrono::milliseconds(25));
    } catch (const std::exception& e) {
        // Ignore exceptions during stop
    }
}

void ChatServerApp::setup_commands() {
    command_map["join"] = std::make_unique<JoinCommand>(ws_to_user);
    command_map["chat"] = std::make_unique<ChatCommand>(ws_to_user);
    command_map["list"] = std::make_unique<ListCommand>();
    command_map["ping"] = std::make_unique<PingCommand>();
    command_map["users"] = std::make_unique<UsersCommand>();
}

void ChatServerApp::run_server() {
    app = std::make_unique<uWS::App>();
    app->ws<PerSocketData>(
        "/*", {                   .open =
                       [this](auto *ws) {
                           if (!running) return; // Don't process if server is shutting down
                           
                           std::string user_id = std::to_string(reinterpret_cast<uintptr_t>(ws));
                           ws->getUserData()->user_id = user_id;
                           User user;
                           user.id = user_id;
                           user.username = user_id;
                           chatServer.users[user_id] = user;
                           ws_to_user[ws] = user_id;
                           
                           if (connection_handler) {
                               connection_handler(user_id);
                           }
                       },
                   .message =
                       [this](auto *ws, std::string_view message, uWS::OpCode opCode) {
                           if (!running) return; // Don't process if server is shutting down
                           
                           try {
                               auto j = json::parse(message);
                               std::string user_id = ws->getUserData()->user_id;
                               auto user_it = chatServer.users.find(user_id);
                               if (user_it == chatServer.users.end()) {
                                   return; // User not found, ignore message
                               }
                               auto &user = user_it->second;
                               std::string type = j["type"];
                               auto it = command_map.find(type);
                               if (it != command_map.end()) {
                                   it->second->execute(j, user, chatServer, ws);
                               } else {
                                   std::cerr << "[ERROR] Unknown command type: " << type
                                             << std::endl;
                               }
                           } catch (const std::exception &e) {
                               std::cerr << "[ERROR] Invalid message: " << e.what() << std::endl;
                           }
                       },
                   .close =
                       [this](auto *ws, int /*code*/, std::string_view /*message*/) {
                           if (!running) return; // Don't process if server is shutting down
                           
                           std::string user_id = ws->getUserData()->user_id;
                           auto user_it = chatServer.users.find(user_id);
                           if (user_it != chatServer.users.end()) {
                               std::string username = user_it->second.username;
                               std::string channel = user_it->second.current_channel;
                               
                               // Notify other users in the channel that someone disconnected
                               if (!channel.empty()) {
                                   auto ch_it = chatServer.channels.find(channel);
                                   if (ch_it != chatServer.channels.end()) {
                                       json disconnect_notification;
                                       disconnect_notification["type"] = "user_disconnected";
                                       disconnect_notification["username"] = username;
                                       disconnect_notification["channel"] = channel;
                                       std::string disconnect_msg = disconnect_notification.dump();
                                       
                                       // Create a copy of the user IDs to avoid iterator invalidation
                                       std::vector<std::string> user_ids_copy;
                                       for (const auto& uid : ch_it->second.user_ids) {
                                           if (uid != user_id) {
                                               user_ids_copy.push_back(uid);
                                           }
                                       }
                                       
                                       // Send notifications to remaining users
                                       for (const auto& uid : user_ids_copy) {
                                           auto ws_it = ws_to_user.begin();
                                           while (ws_it != ws_to_user.end()) {
                                               if (ws_it->second == uid) {
                                                   try {
                                                       ws_it->first->send(disconnect_msg);
                                                   } catch (const std::exception& e) {
                                                       // Ignore send errors during disconnect
                                                   }
                                                   break;
                                               }
                                               ++ws_it;
                                           }
                                       }
                                   }
                               }
                               
                               // Remove user from channel and server state
                               chatServer.leaveChannel(user_id);
                               chatServer.users.erase(user_id);
                               ws_to_user.erase(ws);
                               
                               if (disconnection_handler) {
                                   disconnection_handler(user_id);
                               }
                           }
                       }})
        .listen(port,
                [this](auto *token) {
                    if (!token) {
                        running = false;
                    }
                })
        .run();
}

void ChatServerApp::set_connection_handler(std::function<void(const std::string&)> handler) {
    connection_handler = handler;
}

void ChatServerApp::set_disconnection_handler(std::function<void(const std::string&)> handler) {
    disconnection_handler = handler;
} 
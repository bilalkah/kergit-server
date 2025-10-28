#include "server/ChatServerApp.h"

#include "server/MessageFilters.h"
#include "server/OriginAllowlist.h"
#include "server/TlsConfig.h"

#include <iostream>

// Security modules
#include "core/security/hmac_validator/HMACValidator.h"
#include "core/security/message_validator/MessageValidator.h"
#include "core/security/rate_limiter/RateLimiter.h"
#include "core/security/supabase_jwt_verifier/SupabaseJWTVerifier.h"
#include "utils/EnvLoader.h"

using json = nlohmann::json;

ChatServerApp::ChatServerApp(int port) : port(port) { setup_commands(); }

ChatServerApp::~ChatServerApp() { stop(); }

bool ChatServerApp::start() {
    if (running) return true;

    running = true;
    server_thread = std::thread(&ChatServerApp::run_server, this);

    // Wait a bit for server to start
    while (!started) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
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
        auto current = std::chrono::steady_clock::now();
        while (!stopped) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            if (std::chrono::steady_clock::now() - current > std::chrono::milliseconds(500)) {
                std::cerr << "[WARN] Server stop timeout reached\n";
                break;
            }
        }
        if (stopped) {
            std::cerr << "[SERVER] Server stopped successfully\n";
        }
    } catch (const std::exception &e) {
        // Ignore exceptions during stop
    }
}

void ChatServerApp::setup_commands() {
    // Load environment variables from .env file
    EnvLoader::load_env_file();

    // Initialize DB connection
    try {
        std::string conninfo =
            EnvLoader::get_env("DATABASE_URL", "postgresql://chat_user:12345678@localhost/chat_db");
        db = std::make_unique<ChatDB>(conninfo);
        std::cerr << "[SERVER] Connected to DB" << std::endl;
    } catch (const std::exception &e) {
        std::cerr << "[WARN] Failed to connect to DB: " << e.what() << "\n";
    }

    // Initialize Supabase JWT verifier with single key from environment
    std::string supabase_jwt_key = EnvLoader::get_env("SUPABASE_JWT_KEY", "");

    std::unique_ptr<SupabaseJWTVerifier> jwt_verifier = nullptr;
    if (!supabase_jwt_key.empty()) {
        jwt_verifier = std::make_unique<SupabaseJWTVerifier>(
            supabase_jwt_key, supabase_jwt_key);  // Use same key for both
        std::cerr << "[SERVER] Supabase JWT verifier initialized with single key" << std::endl;
    } else {
        std::cerr << "[WARN] Supabase JWT key not found in environment variables" << std::endl;
        std::cerr << "[WARN] Make sure you have created .env file with SUPABASE_JWT_KEY"
                  << std::endl;
    }

    command_map["auth"] = std::make_unique<AuthenticateCommand>(db.get(), std::move(jwt_verifier));
    command_map["join"] = std::make_unique<JoinCommand>(ws_to_user);
    command_map["chat"] = std::make_unique<ChatCommand>(ws_to_user);
    command_map["list"] = std::make_unique<ListCommand>();
    command_map["ping"] = std::make_unique<PingCommand>();
    command_map["users"] = std::make_unique<UsersCommand>();

    // // Security filters: message validation, optional HMAC, and rate limiting
    // static security::MessageValidator validator;
    // static RateLimiter rateLimiter;
    // static HMACValidator hmacValidator("dev_hmac_secret");
    // rateLimiter.set_message_rate_limit(120);  // 120 messages/min per user/connection

    // set_incoming_filter([&](json &msg) {
    //     // Allow auth to pass basic checks (no signature expected)
    //     std::string type = msg.value("type", "");
    //     std::string cid = msg.value("__cid", "");

    //     // Rate limit per connection id
    //     if (!cid.empty() && !rateLimiter.is_message_allowed(cid)) {
    //         msg["__invalid"] = true;
    //         msg["__error_message"] = "Rate limit exceeded";
    //         msg["type"] = "error";
    //         msg["message"] = "Rate limit exceeded";
    //         return;
    //     }

    //     // Validate message format/content (skip for auth)
    //     if (type != "auth") {
    //         auto res = validator.validate_message(msg);
    //         if (!res.is_valid) {
    //             msg["__invalid"] = true;
    //             msg["__error_message"] = res.error_message;
    //             msg["type"] = "error";
    //             msg["message"] = res.error_message;
    //             return;
    //         }

    //         // Optional HMAC verification if client provided signature and id
    //         if (msg.contains("signature") && msg["signature"].is_string() && msg.contains("id")
    //         &&
    //             msg["id"].is_string()) {
    //             std::string signature = msg["signature"].get<std::string>();
    //             std::string message_id = msg["id"].get<std::string>();
    //             if (!hmacValidator.verify_message_signature(message_id, signature)) {
    //                 msg["__invalid"] = true;
    //                 msg["__error_message"] = "Invalid signature";
    //                 msg["type"] = "error";
    //                 msg["message"] = "Invalid signature";
    //                 return;
    //             }
    //         }
    //     }
    // });
}

void ChatServerApp::run_server() {
#ifdef USE_SSL
    // Load TLS config and validate paths
    TlsConfig tls{};
    if (!tls.validate_and_log()) {
        std::cerr << "[SSL] Startup aborted due to missing files.\n";
        return;
    }

    if (tls.enabled) {
        uWS::SocketContextOptions opts{
            .key_file_name = tls.key_path.c_str(),
            .cert_file_name = tls.cert_path.c_str(),
        };
        app = std::make_unique<uWS::SSLApp>(opts);
    } else {
        // fallback: non-TLS build path (if you ever want it)
        exit(1);
    }
    if (!app) {
        std::cerr << "[ERROR] Failed to create uWS app instance\n";
        return;
    }

    app->ws<PerSocketData>(
           "/*",
           {.upgrade =
                [this](auto *res, auto *req, auto *ctx) {
                    std::string origin = std::string(req->getHeader("origin"));
                    if (!origin_allowed(origin)) {
                        res->writeStatus("403 Forbidden")->end("Origin not allowed");
                        return;
                    }
                    // proceed with default upgrade; no custom PerSocketData yet
                    res->template upgrade<PerSocketData>({}, req->getHeader("sec-websocket-key"),
                                                         req->getHeader("sec-websocket-protocol"),
                                                         req->getHeader("sec-websocket-extensions"),
                                                         ctx);
                },
            .open =
                [this](auto *ws) {
                    if (!running) return;  // Don't process if server is shutting down

                    std::string connection_id = std::to_string(reinterpret_cast<uintptr_t>(ws));
                    ws->getUserData()->user_id = connection_id;
                    ws_to_user[ws] = connection_id;
                    std::cerr << "[SERVER] Connection opened: " << connection_id << std::endl;

                    // Ensure a placeholder User exists for this connection to avoid dereferencing
                    // an invalid iterator on first message
                    if (chatServer.users.find(connection_id) == chatServer.users.end()) {
                        User new_user;
                        new_user.id = connection_id;
                        chatServer.users[connection_id] = new_user;
                    }

                    if (connection_handler) {
                        connection_handler(connection_id);
                    }
                },
            .message =
                [this](auto *ws, std::string_view message, uWS::OpCode opCode) {
                    if (!running) return;  // Don't process if server is shutting down

                    try {
                        std::string connection_id = ws->getUserData()->user_id;
                        auto j = json::parse(message);
                        // Provide connection id to filter
                        j["__cid"] = connection_id;
                        apply_incoming_filter(j);
                        std::string type = j.value("type", "");
                        std::cerr << "[SERVER] Message from " << connection_id << ", type=" << type
                                  << ": " << message << std::endl;

                        // If filter marked invalid, send structured error and drop
                        if (j.contains("__invalid") && j["__invalid"].is_boolean() &&
                            j["__invalid"].get<bool>()) {
                            json err;
                            err["type"] = "error";
                            err["message"] =
                                j.value("__error_message", std::string("Invalid message"));
                            send_json(ws, err, uWS::OpCode::TEXT);
                            return;
                        }

                        auto user_it = chatServer.users.find(connection_id);
                        auto &user = user_it->second;
                        auto it = command_map.find(type);
                        if (it != command_map.end()) {
                            it->second->execute(j, user, chatServer, ws);
                        } else {
                            std::cerr << "[ERROR] Unknown command type: " << type << std::endl;
                        }
                    } catch (const std::exception &e) {
                        std::cerr << "[ERROR] Invalid message: " << e.what() << std::endl;
                    }
                },
            .close =
                [this](auto *ws, int /*code*/, std::string_view /*message*/) {
                    if (!running) return;  // Don't process if server is shutting down

                    std::string user_id = ws->getUserData()->user_id;
                    std::cerr << "[SERVER] Connection closed: " << user_id << std::endl;
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
                                // Create a copy of the user IDs to avoid iterator invalidation
                                std::vector<std::string> user_ids_copy;
                                for (const auto &uid : ch_it->second.user_ids) {
                                    if (uid != user_id) {
                                        user_ids_copy.push_back(uid);
                                    }
                                }

                                // Send notifications to remaining users
                                for (const auto &uid : user_ids_copy) {
                                    auto ws_it = ws_to_user.begin();
                                    while (ws_it != ws_to_user.end()) {
                                        if (ws_it->second == uid) {
                                            try {
                                                json msg = disconnect_notification;
                                                send_json(ws_it->first, msg, uWS::OpCode::TEXT);
                                            } catch (const std::exception &e) {
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
                    } else {
                        std::cerr << "[SERVER] Listening on port " << port << std::endl;
                        started = true;
                    }
                })
        .run();
#else
    app = std::make_unique<uWS::App>();
    if (!app) {
        std::cerr << "[ERROR] Failed to create uWS app instance\n";
        return;
    }

    app->ws<PerSocketData>(
           "/*",
           {.open =
                [this](auto *ws) {
                    if (!running) return;  // Don't process if server is shutting down

                    std::string connection_id = std::to_string(reinterpret_cast<uintptr_t>(ws));
                    ws->getUserData()->user_id = connection_id;
                    ws_to_user[ws] = connection_id;
                    std::cerr << "[SERVER] Connection opened: " << connection_id << std::endl;

                    // Ensure a placeholder User exists for this connection to avoid dereferencing
                    // an invalid iterator on first message
                    if (chatServer.users.find(connection_id) == chatServer.users.end()) {
                        User new_user;
                        new_user.id = connection_id;
                        chatServer.users[connection_id] = new_user;
                    }

                    if (connection_handler) {
                        connection_handler(connection_id);
                    }
                },
            .message =
                [this](auto *ws, std::string_view message, uWS::OpCode opCode) {
                    if (!running) return;  // Don't process if server is shutting down

                    try {
                        std::string connection_id = ws->getUserData()->user_id;
                        auto j = json::parse(message);

                        // // Provide connection id to filter
                        // j["__cid"] = connection_id;
                        // apply_incoming_filter(j);
                        // std::string type = j.value("type", "");
                        // std::cerr << "[SERVER] Message from " << connection_id << ", type=" <<
                        // type
                        //           << ": " << message << std::endl;

                        // // If filter marked invalid, send structured error and drop
                        // if (j.contains("__invalid") && j["__invalid"].is_boolean() &&
                        //     j["__invalid"].get<bool>()) {
                        //     json err;
                        //     err["type"] = "error";
                        //     err["message"] =
                        //         j.value("__error_message", std::string("Invalid message"));
                        //     send_json(ws, err, uWS::OpCode::TEXT);
                        //     return;
                        // }

                        std::string type = j.value("type", "");
                        std::cerr << "[SERVER] Message from " << connection_id << ", type=" << type
                                  << ": " << message << std::endl;
                        auto user_it = chatServer.users.find(connection_id);
                        auto &user = user_it->second;
                        auto it = command_map.find(type);
                        if (it != command_map.end()) {
                            it->second->execute(j, user, chatServer, ws);
                        } else {
                            std::cerr << "[ERROR] Unknown command type: " << type << std::endl;
                        }
                        if (type == "auth") {
                            // After auth, update ws_to_user mapping
                            ws_to_user[ws] = ws->getUserData()->user_id;
                        }
                    } catch (const std::exception &e) {
                        std::cerr << "[ERROR] Invalid message: " << e.what() << std::endl;
                    }
                },
            .close =
                [this](auto *ws, int /*code*/, std::string_view /*message*/) {
                    if (!running) return;  // Don't process if server is shutting down

                    std::string user_id = ws->getUserData()->user_id;
                    std::cerr << "[SERVER] Connection closed: " << user_id << std::endl;
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
                                // Create a copy of the user IDs to avoid iterator invalidation
                                std::vector<std::string> user_ids_copy;
                                for (const auto &uid : ch_it->second.user_ids) {
                                    if (uid != user_id) {
                                        user_ids_copy.push_back(uid);
                                    }
                                }

                                // Send notifications to remaining users
                                for (const auto &uid : user_ids_copy) {
                                    auto ws_it = ws_to_user.begin();
                                    while (ws_it != ws_to_user.end()) {
                                        if (ws_it->second == uid) {
                                            try {
                                                json msg = disconnect_notification;
                                                send_json(ws_it->first, msg, uWS::OpCode::TEXT);
                                            } catch (const std::exception &e) {
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
        .listen("0.0.0.0", port,
                [this](auto *token) {
                    if (!token) {
                        running = false;
                    } else {
                        std::cerr << "[SERVER] Listening on port " << port << std::endl;
                        started = true;
                    }
                })
        .run();
#endif
    stopped = true;
}

void ChatServerApp::set_connection_handler(std::function<void(const std::string &)> handler) {
    connection_handler = handler;
}

void ChatServerApp::set_disconnection_handler(std::function<void(const std::string &)> handler) {
    disconnection_handler = handler;
}
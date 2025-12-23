// app/main_plain.cc
#include "core/ChatServerApp.h"
#include "core/ServerConfig.h"
#include "utils/EnvLoader.h"
#include "utils/Logger.h"

#include <csignal>
#include <iostream>
// #include <sw/redis++/redis++.h>
#include <thread>

using namespace core;

// void test_redis_connection(const std::string& host, int port) {
//     try {
//         std::string uri = "tcp://" + host + ":" + std::to_string(port);

//         sw::redis::Redis redis(uri);

//         auto reply = redis.ping();
//         std::cout << "[Redis] PING reply: " << reply << std::endl;
//     } catch (const sw::redis::Error& e) {
//         std::cerr << "[Redis] Connection error: " << e.what() << std::endl;
//     }
// }

std::atomic<bool> g_shutdown_requested{false};

void handle_signal(int) {
    log_line(utils::LogLevel::WARN, "[signal] Ctrl+C received, shutting down...");
    g_shutdown_requested.store(true, std::memory_order_relaxed);
}

int main() {
    std::signal(SIGINT, handle_signal);
    std::signal(SIGTERM, handle_signal);

    try {
        std::unique_ptr<ChatServerApp> g_server;
        utils::EnvLoader::load_env_file();

        // log_line(utils::LogLevel::INFO,
        //          "Redis host: " + utils::EnvLoader::get_env("REDIS_HOST", "not_set") +
        //              ", port: " + utils::EnvLoader::get_env("REDIS_PORT", "not_set"));

        // std::string redis_host = utils::EnvLoader::get_env("REDIS_HOST", "not_set");
        // int redis_port = std::stoi(utils::EnvLoader::get_env("REDIS_PORT", "not_set"));

        // test_redis_connection(redis_host, redis_port);

        ServerConfig cfg;
        ServerConfigFiller::fill_from_env(cfg);

        g_server = std::make_unique<ChatServerApp>(cfg);

        log_line(utils::LogLevel::INFO, "Starting the App...");

        g_server->start();  // runs until stop() closes listener

        while (!g_shutdown_requested.load(std::memory_order_relaxed)) {
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
        }

        log_line(utils::LogLevel::INFO, "Stopping the App...");
        g_server->stop();
        log_line(utils::LogLevel::INFO, "App stopped gracefully.");
    } catch (const std::exception& ex) {
        log_line(utils::LogLevel::ERROR, std::string("Fatal error: ") + ex.what());
        return 1;
    }

    return 0;
}

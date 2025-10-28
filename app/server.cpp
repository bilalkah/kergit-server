#include "server/ChatServerApp.h"
#include "utils/EnvLoader.h"

#include <atomic>
#include <string>
#include <csignal>
#include <iostream>

std::atomic<bool> running{true};

void handle_signal(int) { running = false; }

int main() {
    std::signal(SIGINT, handle_signal);
    std::signal(SIGTERM, handle_signal);

    EnvLoader::load_env_file();

    const std::string url = EnvLoader::get_env("SERVER_HOST", "localhost");
    const int port = std::stoi(EnvLoader::get_env("SERVER_PORT", "9001"));

    ChatServerApp server(url, port);

    if (!server.start()) {
        std::cerr << "[APP] Failed to start server." << std::endl;
        return 1;
    }

    std::cout << "[APP] Chat server started. Press Ctrl+C to stop." << std::endl;

    while (running.load()) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    std::cerr << "\n[APP] Server is shutting down." << std::endl;

    return 0;
}
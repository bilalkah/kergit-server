#pragma once

#include "clients/cli/ChatClient.h"
#include "server/ChatServer.h"

#include <atomic>
#include <chrono>
#include <gtest/gtest.h>
#include <memory>
#include <thread>
#include <vector>

using namespace std::chrono;

class ChatTestFixture : public ::testing::Test {
   protected:
    void SetUp() override {
        // Start server on a unique port for each test
        // Use a combination of pid, time, and thread id for uniqueness
        auto now = std::chrono::steady_clock::now();
        auto time_ms =
            std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
        server_port = 9000 + (getpid() % 1000) + (time_ms % 1000);
        server = std::make_unique<ChatServer>(server_port);

        ASSERT_TRUE(server->start());

        // Wait for server to be ready
        std::this_thread::sleep_for(milliseconds(100));

        // Clear any previous clients
        clients.clear();
    }

    void TearDown() override {
        // Clean up clients first
        for (auto& client : clients) {
            if (client) {
                if (client->is_connected()) {
                    client->disconnect();
                }
                // Don't call reset() here as it can cause issues with WebSocket state
                // The client will be destroyed properly when the shared_ptr is cleared
            }
        }
        clients.clear();

        // Wait a bit for client cleanup
        std::this_thread::sleep_for(milliseconds(50));

        // Stop server
        if (server) {
            server->stop();
        }

        // Wait a bit for server cleanup
        std::this_thread::sleep_for(milliseconds(100));
    }

    // Helper method to create a client
    std::shared_ptr<ChatClient> create_client(const std::string& name = "") {
        std::string uri = "ws://localhost:" + std::to_string(server_port);
        auto client = std::make_shared<ChatClient>(uri);
        clients.push_back(client);
        return client;
    }

    // Helper method to connect a client
    bool connect_client(std::shared_ptr<ChatClient> client, const std::string& username = "") {
        if (!client->connect()) {
            return false;
        }

        if (!username.empty()) {
            // Set username by joining a temporary channel and then leaving
            client->join_channel("temp", username);
            client->leave_channel();
        }

        return true;
    }

    // Helper method to wait for a message (reduced default timeout from 5000ms to 1000ms)
    bool wait_for_message(std::shared_ptr<ChatClient> client, const std::string& type,
                          int timeout_ms = 1000) {
        return client->wait_for_message(type, timeout_ms);
    }

    // Helper method to wait for connection (reduced default timeout from 5000ms to 1000ms)
    bool wait_for_connection(std::shared_ptr<ChatClient> client, int timeout_ms = 1000) {
        return client->wait_for_connection(timeout_ms);
    }

    int server_port;
    std::unique_ptr<ChatServer> server;
    std::vector<std::shared_ptr<ChatClient>> clients;
};
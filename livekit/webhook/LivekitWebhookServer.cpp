#include "livekit/webhook/LivekitWebhookServer.h"

#include <chrono>
#include <iostream>
#include <memory>

#include <nlohmann/json.hpp>
#include "utils/EventLogger.h"

namespace livekit::webhook {

LivekitWebhookServer::LivekitWebhookServer()
    : config_(), callback_(nullptr) {}

LivekitWebhookServer::LivekitWebhookServer(Config config)
    : config_(std::move(config)), callback_(nullptr) {}

LivekitWebhookServer::~LivekitWebhookServer() {
    stop();
}

void LivekitWebhookServer::start() {
    if (running_)
        return;

    running_ = true;
    LivekitWebhookServer_thread_ = std::thread(&LivekitWebhookServer::run, this);
}

void LivekitWebhookServer::stop() {
    if (stop_requested_.exchange(true))
        return;

    running_ = false;

    if (auto* lp = loop_.load(std::memory_order_acquire)) {
        lp->defer([this]() {
            if (auto* sock = listen_socket_.exchange(nullptr, std::memory_order_acq_rel)) {
                us_listen_socket_close(0, sock);
            }
        });
    }

    if (LivekitWebhookServer_thread_.joinable())
        LivekitWebhookServer_thread_.join();
}

void LivekitWebhookServer::run() {
    uWS::App app;

    app.post(config_.path, [this](auto* res, auto* req) {
        auto body = std::make_shared<std::string>();
        auto aborted = std::make_shared<bool>(false);

        res->onAborted([aborted]() {
            *aborted = true;
        });

        res->onData([this, res, body, aborted](std::string_view chunk, bool last) {
            body->append(chunk.data(), chunk.size());

            if (!last)
                return;

            if (*aborted)
                return;

            try {
                handle_body(*body);
            } catch (const std::exception& e) {
                log(utils::LogLevel::ERROR, "LiveKit webhook parse error: ", e.what());
            }

            res->writeStatus("200 OK")->end();
        });
    });

    app.listen(config_.host, config_.port, [this](auto* token) {
        if (token) {
            listen_socket_.store(token, std::memory_order_release);
            loop_.store(uWS::Loop::get(), std::memory_order_release);
            log(utils::LogLevel::INFO, "LiveKit webhook LivekitWebhookServer listening on ", config_.host, ":",
                config_.port, config_.path);
        } else {
            log(utils::LogLevel::ERROR, "Failed to start LiveKit webhook LivekitWebhookServer on ",
                config_.host, ":", config_.port);
        }
    });

    app.run();
}

void LivekitWebhookServer::handle_body(std::string_view body) {
    auto json = nlohmann::json::parse(body);

    LiveKitEvent event;

    if (json.contains("event"))
        event.type = parseLiveKitEvent(json["event"].get<std::string>());

    if (json.contains("room") && json["room"].contains("name"))
        event.channel_id = ChannelId(json["room"]["name"].get<std::string>());

    if (json.contains("participant") && json["participant"].contains("identity"))
    {
        event.user_id = UserId(json["participant"]["identity"].get<std::string>());
        if (json["participant"].contains("metadata")) {
            event.node_id = json["participant"]["metadata"].get<std::string>();
        }
    }

    if (json.contains("createdAt")) {
        // LiveKit sends createdAt as Unix seconds (integer or stringified)
        if (json["createdAt"].is_number())
            event.timestamp_ms = json["createdAt"].get<uint64_t>() * 1000;
        else if (json["createdAt"].is_string())
            event.timestamp_ms = std::stoull(json["createdAt"].get<std::string>()) * 1000;
    }

    utils::EventLogger::instance().log(utils::EventCategory::VOICE, event.user_id.value, "LIVEKIT_EVENT", 
        event.timestamp_ms, "event_type:" + std::to_string(static_cast<int>(event.type)) + 
            " node_id:" + event.node_id + " channel_id:" + event.channel_id.value);

    if (callback_) {
        callback_(event);
    }
}

}  // namespace livekit::webhook

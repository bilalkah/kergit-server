#include "livekit/webhook/LivekitWebhookServer.h"

#include "utils/EventLogger.h"

#include <chrono>
#include <cstdint>
#include <iostream>
#include <jwt-cpp/jwt.h>
#include <memory>
#include <nlohmann/json.hpp>
#include <openssl/evp.h>
#include <stdexcept>
#include <string>
#include <thread>

namespace livekit::webhook {
namespace {

bool sha256_base64(std::string_view body, std::string& out) {
    out.clear();

    thread_local std::unique_ptr<EVP_MD_CTX, decltype(&EVP_MD_CTX_free)> ctx(EVP_MD_CTX_new(),
                                                                             &EVP_MD_CTX_free);
    if (!ctx) {
        utils::log_line(utils::LogLevel::ERROR, "LiveKit webhook: EVP_MD_CTX_new failed");
        return false;
    }
    if (EVP_MD_CTX_reset(ctx.get()) != 1) {
        utils::log_line(utils::LogLevel::ERROR, "LiveKit webhook: EVP_MD_CTX_reset failed");
        return false;
    }

    unsigned char digest[EVP_MAX_MD_SIZE];
    unsigned int digest_len = 0;

    if (EVP_DigestInit_ex(ctx.get(), EVP_sha256(), nullptr) != 1 ||
        EVP_DigestUpdate(ctx.get(), body.data(), body.size()) != 1 ||
        EVP_DigestFinal_ex(ctx.get(), digest, &digest_len) != 1) {
        utils::log_line(utils::LogLevel::ERROR, "LiveKit webhook: EVP sha256 failed");
        return false;
    }

    out.resize(4 * ((digest_len + 2) / 3));
    const int encoded_len = EVP_EncodeBlock(reinterpret_cast<unsigned char*>(&out[0]), digest,
                                            static_cast<int>(digest_len));
    if (encoded_len <= 0) {
        utils::log_line(utils::LogLevel::ERROR, "LiveKit webhook: EVP base64 encode failed");
        return false;
    }
    out.resize(static_cast<size_t>(encoded_len));
    return true;
}

}  // namespace

LivekitWebhookServer::LivekitWebhookServer() : config_(), callback_(nullptr) {}

LivekitWebhookServer::LivekitWebhookServer(Config config)
    : config_(std::move(config)), callback_(nullptr) {}

LivekitWebhookServer::~LivekitWebhookServer() { stop(); }

void LivekitWebhookServer::set_signing_credentials(std::string api_key, std::string api_secret) {
    config_.api_key = std::move(api_key);
    config_.api_secret = std::move(api_secret);
}

void LivekitWebhookServer::start() {
    if (running_) return;

    running_ = true;
    LivekitWebhookServer_thread_ = std::thread(&LivekitWebhookServer::run, this);
}

void LivekitWebhookServer::stop() {
    if (stop_requested_.exchange(true)) return;

    running_ = false;

    if (auto* lp = loop_.load(std::memory_order_acquire)) {
        lp->defer([this]() {
            if (auto* sock = listen_socket_.exchange(nullptr, std::memory_order_acq_rel)) {
                us_listen_socket_close(0, sock);
            }
        });
    }

    if (LivekitWebhookServer_thread_.joinable()) LivekitWebhookServer_thread_.join();
}

void LivekitWebhookServer::run() {
    uWS::App app;

    app.post(config_.path, [this](auto* res, auto* req) {
        const std::string auth_header = std::string(req->getHeader("authorization"));
        // const std::string content_type = std::string(req->getHeader("content-type"));
        auto body = std::make_shared<std::string>();
        auto aborted = std::make_shared<bool>(false);

        res->onAborted([aborted]() { *aborted = true; });

        res->onData([this, res, body, aborted, auth_header /*, content_type */](
                        std::string_view chunk, bool last) {
            body->append(chunk.data(), chunk.size());

            if (!last) return;

            if (*aborted) return;

            // log(utils::LogLevel::WARN, "LiveKit webhook raw request: content-type=",
            // content_type,
            //     " authorization=", auth_header, " body=", *body);

            bool ok = true;
            try {
                handle_body(*body, auth_header);
            } catch (const std::exception& e) {
                ok = false;
                log(utils::LogLevel::ERROR, "LiveKit webhook parse error: ", e.what());
            }

            res->writeStatus(ok ? "200 OK" : "401 Unauthorized")->end();
        });
    });

    app.listen(config_.host, config_.port, [this](auto* token) {
        if (token) {
            listen_socket_.store(token, std::memory_order_release);
            loop_.store(uWS::Loop::get(), std::memory_order_release);
            log(utils::LogLevel::INFO, "LiveKit webhook LivekitWebhookServer listening on ",
                config_.host, ":", config_.port, config_.path);
        } else {
            log(utils::LogLevel::ERROR, "Failed to start LiveKit webhook LivekitWebhookServer on ",
                config_.host, ":", config_.port);
        }
    });

    app.run();
}

bool LivekitWebhookServer::verify_webhook_signature(std::string_view body,
                                                    std::string_view auth_header) const {
    if (config_.api_key.empty() || config_.api_secret.empty()) return false;

    static constexpr std::string_view kBearer = "Bearer ";
    std::string_view token_view = auth_header;
    if (auth_header.size() > kBearer.size() && auth_header.substr(0, kBearer.size()) == kBearer) {
        token_view = auth_header.substr(kBearer.size());
    }
    if (token_view.empty()) return false;

    try {
        const std::string token(token_view);
        const auto decoded = jwt::decode(token);
        jwt::verify()
            .allow_algorithm(jwt::algorithm::hs256{config_.api_secret})
            .with_issuer(config_.api_key)
            .verify(decoded);

        if (!decoded.has_payload_claim("sha256")) return false;
        const std::string payload_hash = decoded.get_payload_claim("sha256").as_string();

        thread_local std::string computed_hash;
        if (!sha256_base64(body, computed_hash)) {
            return false;
        }
        return payload_hash == computed_hash;
    } catch (const std::exception&) {
        return false;
    }
}

void LivekitWebhookServer::handle_body(std::string_view body, std::string_view auth_header) {
    if (!verify_webhook_signature(body, auth_header)) {
        throw std::runtime_error("LiveKit webhook signature validation failed");
    }

    auto json = nlohmann::json::parse(body);

    LiveKitEvent event;

    if (json.contains("id") && json["id"].is_string()) {
        event.event_id = json["id"].get<std::string>();
    }

    if (json.contains("event")) event.type = parseLiveKitEvent(json["event"].get<std::string>());

    if (json.contains("room") && json["room"].contains("name"))
        event.channel_id = ChannelId(json["room"]["name"].get<std::string>());

    if (json.contains("participant")) {
        const auto& participant = json["participant"];

        if (participant.contains("identity") && participant["identity"].is_string()) {
            event.user_id = UserId(participant["identity"].get<std::string>());
        }
        if (participant.contains("sid") && participant["sid"].is_string()) {
            event.participant_sid = participant["sid"].get<std::string>();
        }
        if (participant.contains("metadata") && participant["metadata"].is_string()) {
            event.participant_metadata = participant["metadata"].get<std::string>();
            auto metadata = nlohmann::json::parse(event.participant_metadata, nullptr, false);
            if (metadata.is_object()) {
                if (metadata.contains("node_id") && metadata["node_id"].is_string()) {
                    event.node_id = metadata["node_id"].get<std::string>();
                }
                if (metadata.contains("app_session_id")) {
                    if (metadata["app_session_id"].is_number_unsigned()) {
                        event.app_session_id =
                            static_cast<uint64_t>(metadata["app_session_id"].get<uint64_t>());
                    } else if (metadata["app_session_id"].is_string()) {
                        try {
                            event.app_session_id = static_cast<uint64_t>(
                                std::stoull(metadata["app_session_id"].get<std::string>()));
                        } catch (...) {
                            event.app_session_id = 0;
                        }
                    }
                }
                if (metadata.contains("intent_nonce") && metadata["intent_nonce"].is_string()) {
                    event.intent_nonce = metadata["intent_nonce"].get<std::string>();
                }
            } else {
                // Backward compatibility: old tokens carried only node id as plain metadata string.
                event.node_id = event.participant_metadata;
            }
        }
    }

    if (json.contains("createdAt")) {
        // LiveKit sends createdAt as Unix seconds (integer or stringified)
        if (json["createdAt"].is_number())
            event.timestamp_ms = json["createdAt"].get<uint64_t>() * 1000;
        else if (json["createdAt"].is_string())
            event.timestamp_ms = std::stoull(json["createdAt"].get<std::string>()) * 1000;
    }

    utils::EventLogger::instance().log(
        utils::EventCategory::VOICE, event.user_id.value, "LIVEKIT_EVENT", event.timestamp_ms,
        "event_id:" + event.event_id +
            " event_type:" + std::to_string(static_cast<int>(event.type)) +
            " node_id:" + event.node_id + " channel_id:" + event.channel_id.value +
            " session_id:" + std::to_string(event.app_session_id));

    if (callback_) {
        callback_(event);
    }
}

}  // namespace livekit::webhook

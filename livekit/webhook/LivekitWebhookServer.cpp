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

            HandleResult result = HandleResult::OK;
            try {
                result = handle_body(*body, auth_header);
            } catch (const std::exception& e) {
                result = HandleResult::BAD_REQUEST;
                log(utils::LogLevel::ERROR, "LiveKit webhook parse error: ", e.what());
            }

            if (result != HandleResult::OK) {
                const std::string_view reason =
                    result == HandleResult::UNAUTHORIZED ? "invalid_signature" : "bad_request";
                const std::string_view status =
                    result == HandleResult::UNAUTHORIZED ? "401" : "400";
                utils::EventLogger::instance().log(
                    utils::EventCategory::VOICE, "", "LIVEKIT_WEBHOOK_REJECTED", 0,
                    "status:" + std::string(status) + " reason:" + std::string(reason) +
                        " body_bytes:" + std::to_string(body->size()) + " path:" + config_.path);
            }

            switch (result) {
                case HandleResult::OK:
                    res->writeStatus("200 OK")->end();
                    break;
                case HandleResult::BAD_REQUEST:
                    res->writeStatus("400 Bad Request")->end();
                    break;
                case HandleResult::UNAUTHORIZED:
                    res->writeStatus("401 Unauthorized")->end();
                    break;
            }
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

LivekitWebhookServer::HandleResult LivekitWebhookServer::handle_body(std::string_view body,
                                                                     std::string_view auth_header) {
    if (!verify_webhook_signature(body, auth_header)) {
        return HandleResult::UNAUTHORIZED;
    }

    auto json = nlohmann::json::parse(std::string(body), nullptr, false);
    if (!json.is_object()) {
        return HandleResult::BAD_REQUEST;
    }

    LiveKitEvent event;

    if (json.contains("id") && json["id"].is_string()) {
        event.event_id = json["id"].get<std::string>();
    }

    if (!json.contains("event") || !json["event"].is_string()) {
        return HandleResult::BAD_REQUEST;
    }
    event.raw_event_name = json["event"].get<std::string>();
    event.type = parseLiveKitEvent(event.raw_event_name);

    if (json.contains("room") && json["room"].is_object() && json["room"].contains("name"))
        event.channel_id = ChannelId(json["room"]["name"].get<std::string>());

    if (json.contains("participant") && json["participant"].is_object()) {
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

    if (json.contains("track") && json["track"].is_object()) {
        const auto& track = json["track"];
        if (track.contains("sid") && track["sid"].is_string()) {
            event.track_sid = track["sid"].get<std::string>();
        }
        if (track.contains("type") && track["type"].is_string()) {
            event.track_type = track["type"].get<std::string>();
        }
        if (track.contains("source") && track["source"].is_string()) {
            event.track_source = track["source"].get<std::string>();
        }
    }

    if (json.contains("egressInfo") && json["egressInfo"].is_object()) {
        const auto& egress = json["egressInfo"];
        if (egress.contains("egressId") && egress["egressId"].is_string()) {
            event.egress_id = egress["egressId"].get<std::string>();
        } else if (egress.contains("egress_id") && egress["egress_id"].is_string()) {
            event.egress_id = egress["egress_id"].get<std::string>();
        }
    }

    if (json.contains("ingressInfo") && json["ingressInfo"].is_object()) {
        const auto& ingress = json["ingressInfo"];
        if (ingress.contains("ingressId") && ingress["ingressId"].is_string()) {
            event.ingress_id = ingress["ingressId"].get<std::string>();
        } else if (ingress.contains("ingress_id") && ingress["ingress_id"].is_string()) {
            event.ingress_id = ingress["ingress_id"].get<std::string>();
        }
    }

    if (json.contains("createdAt")) {
        // LiveKit sends createdAt as Unix seconds (integer or stringified)
        if (json["createdAt"].is_number())
            event.timestamp_ms = json["createdAt"].get<uint64_t>() * 1000;
        else if (json["createdAt"].is_string()) {
            try {
                event.timestamp_ms = std::stoull(json["createdAt"].get<std::string>()) * 1000;
            } catch (...) {
                event.timestamp_ms = 0;
            }
        }
    }

    utils::EventLogger::instance().log(
        utils::EventCategory::VOICE, event.user_id.value, "LIVEKIT_EVENT", event.timestamp_ms,
        "event_id:" + event.event_id + " raw_event_name:" + event.raw_event_name +
            " event_type:" + std::to_string(static_cast<int>(event.type)) +
            " node_id:" + event.node_id + " channel_id:" + event.channel_id.value +
            " session_id:" + std::to_string(event.app_session_id));

    if (callback_) {
        callback_(event);
    }

    return HandleResult::OK;
}

}  // namespace livekit::webhook

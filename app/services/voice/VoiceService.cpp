#include "app/services/voice/VoiceService.h"

#include "app/commands/utils.h"
#include "app/managers/subscription/Topic.h"
#include "livekit/cli/LivekitClient.h"
#include "proto/envelope.pb.h"
#include "proto/event/activity.pb.h"
#include "utils/EnvLoader.h"
#include "utils/EventLogger.h"
#include "utils/Metrics.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <memory>
#include <nlohmann/json.hpp>
#include <openssl/evp.h>
#include <openssl/rand.h>
#include <openssl/sha.h>
#include <optional>
#include <random>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace app::services::voice {
namespace {
using json = nlohmann::json;

uint64_t unix_now_seconds() {
    return static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::seconds>(
                                     std::chrono::system_clock::now().time_since_epoch())
                                     .count());
}

template <typename TPayload>
std::string serialize_as_envelope(const sercom::protocol::Envelope::Type type,
                                  const TPayload& payload) {
    sercom::protocol::Envelope env;
    env.set_version(1);
    env.set_type(type);
    payload.SerializeToString(env.mutable_payload());
    return env.SerializeAsString();
}

std::string make_voice_self_status_connected(bool is_owner, const HubId& hub_id,
                                             const ChannelId& channel_id,
                                             const std::optional<std::string>& resume_id) {
    sercom::protocol::event::VoiceSelfStatus status;
    status.set_connected(true);
    status.set_is_owner(is_owner);
    *status.mutable_channel() = ::app::to_proto_channel_ref(hub_id, channel_id);
    if (is_owner && resume_id.has_value() && !resume_id->empty()) {
        status.set_resume_id(*resume_id);
    }
    return serialize_as_envelope(sercom::protocol::Envelope::VOICE_SELF_STATUS, status);
}

std::string make_voice_self_status_disconnected() {
    sercom::protocol::event::VoiceSelfStatus status;
    status.set_connected(false);
    status.set_is_owner(false);
    return serialize_as_envelope(sercom::protocol::Envelope::VOICE_SELF_STATUS, status);
}

bool contains_participant(const std::vector<livekit::cli::ParticipantInfo>& participants,
                          const UserId& user) {
    return std::any_of(participants.begin(), participants.end(),
                       [&](const auto& p) { return p.identity == user; });
}

std::string generate_nonce_hex() {
    thread_local std::mt19937_64 rng(std::random_device{}());
    constexpr char kHex[] = "0123456789abcdef";

    std::string out;
    out.resize(32);
    for (size_t i = 0; i < 16; ++i) {
        const uint8_t byte = static_cast<uint8_t>(rng() & 0xFFu);
        out[i * 2] = kHex[(byte >> 4) & 0x0Fu];
        out[i * 2 + 1] = kHex[byte & 0x0Fu];
    }
    return out;
}

std::chrono::seconds parse_reconcile_interval_seconds(const std::string& raw,
                                                      std::chrono::seconds fallback) {
    if (raw.empty()) return fallback;
    try {
        const auto parsed = std::stoll(raw);
        if (parsed <= 0) return fallback;
        return std::chrono::seconds(parsed);
    } catch (...) {
        return fallback;
    }
}

std::chrono::seconds parse_positive_seconds(const std::string& raw, std::chrono::seconds fallback) {
    if (raw.empty()) return fallback;
    try {
        const auto parsed = std::stoll(raw);
        if (parsed <= 0) return fallback;
        return std::chrono::seconds(parsed);
    } catch (...) {
        return fallback;
    }
}

bool derive_master_key(std::string_view secret, std::array<unsigned char, 32>& out_key) {
    if (secret.empty()) return false;

    unsigned int digest_len = 0;
    if (EVP_Digest(secret.data(), secret.size(), out_key.data(), &digest_len, EVP_sha256(),
                   nullptr) != 1) {
        return false;
    }
    return digest_len == out_key.size();
}

bool base64_encode(std::string_view input, std::string& out) {
    if (input.empty()) {
        out.clear();
        return true;
    }

    const int encoded_len = 4 * ((static_cast<int>(input.size()) + 2) / 3);
    out.assign(static_cast<size_t>(encoded_len), '\0');
    const int actual_len = EVP_EncodeBlock(reinterpret_cast<unsigned char*>(out.data()),
                                           reinterpret_cast<const unsigned char*>(input.data()),
                                           static_cast<int>(input.size()));
    if (actual_len <= 0) return false;
    out.resize(static_cast<size_t>(actual_len));
    return true;
}

bool base64_decode(std::string_view input, std::string& out) {
    if (input.empty()) {
        out.clear();
        return true;
    }

    if ((input.size() % 4) != 0) return false;

    out.assign((input.size() / 4) * 3, '\0');
    const int decoded_len = EVP_DecodeBlock(reinterpret_cast<unsigned char*>(out.data()),
                                            reinterpret_cast<const unsigned char*>(input.data()),
                                            static_cast<int>(input.size()));
    if (decoded_len < 0) return false;

    size_t actual_len = static_cast<size_t>(decoded_len);
    if (!input.empty() && input.back() == '=') actual_len--;
    if (input.size() > 1 && input[input.size() - 2] == '=') actual_len--;
    out.resize(actual_len);
    return true;
}

bool encrypt_key_blob(const std::array<unsigned char, 32>& master_key, std::string_view plaintext,
                      std::string& out_b64) {
    constexpr unsigned char kVersion = 1;
    constexpr size_t kNonceLen = 12;
    constexpr size_t kTagLen = 16;

    std::array<unsigned char, kNonceLen> nonce{};
    if (RAND_bytes(nonce.data(), static_cast<int>(nonce.size())) != 1) {
        return false;
    }

    std::unique_ptr<EVP_CIPHER_CTX, decltype(&EVP_CIPHER_CTX_free)> ctx(EVP_CIPHER_CTX_new(),
                                                                        &EVP_CIPHER_CTX_free);
    if (!ctx) return false;

    if (EVP_EncryptInit_ex(ctx.get(), EVP_aes_256_gcm(), nullptr, nullptr, nullptr) != 1) {
        return false;
    }
    if (EVP_CIPHER_CTX_ctrl(ctx.get(), EVP_CTRL_GCM_SET_IVLEN, static_cast<int>(kNonceLen),
                            nullptr) != 1) {
        return false;
    }
    if (EVP_EncryptInit_ex(ctx.get(), nullptr, nullptr, master_key.data(), nonce.data()) != 1) {
        return false;
    }

    std::string ciphertext;
    ciphertext.assign(plaintext.size(), '\0');

    int written = 0;
    if (!plaintext.empty() &&
        EVP_EncryptUpdate(ctx.get(), reinterpret_cast<unsigned char*>(ciphertext.data()), &written,
                          reinterpret_cast<const unsigned char*>(plaintext.data()),
                          static_cast<int>(plaintext.size())) != 1) {
        return false;
    }
    int total = written;

    int final_written = 0;
    if (EVP_EncryptFinal_ex(ctx.get(), reinterpret_cast<unsigned char*>(ciphertext.data() + total),
                            &final_written) != 1) {
        return false;
    }
    total += final_written;
    ciphertext.resize(static_cast<size_t>(total));

    std::array<unsigned char, kTagLen> tag{};
    if (EVP_CIPHER_CTX_ctrl(ctx.get(), EVP_CTRL_GCM_GET_TAG, static_cast<int>(kTagLen),
                            tag.data()) != 1) {
        return false;
    }

    std::string packed;
    packed.reserve(1 + kNonceLen + ciphertext.size() + kTagLen);
    packed.push_back(static_cast<char>(kVersion));
    packed.append(reinterpret_cast<const char*>(nonce.data()), nonce.size());
    packed.append(ciphertext);
    packed.append(reinterpret_cast<const char*>(tag.data()), tag.size());

    return base64_encode(packed, out_b64);
}

bool decrypt_key_blob(const std::array<unsigned char, 32>& master_key, std::string_view encoded,
                      std::string& out_plaintext) {
    constexpr unsigned char kVersion = 1;
    constexpr size_t kNonceLen = 12;
    constexpr size_t kTagLen = 16;

    std::string packed;
    if (!base64_decode(encoded, packed)) return false;
    if (packed.size() < (1 + kNonceLen + kTagLen)) return false;
    if (static_cast<unsigned char>(packed[0]) != kVersion) return false;

    const auto* nonce = reinterpret_cast<const unsigned char*>(packed.data() + 1);
    const size_t ciphertext_len = packed.size() - (1 + kNonceLen + kTagLen);
    const auto* ciphertext = reinterpret_cast<const unsigned char*>(packed.data() + 1 + kNonceLen);
    const auto* tag =
        reinterpret_cast<const unsigned char*>(packed.data() + packed.size() - kTagLen);

    std::unique_ptr<EVP_CIPHER_CTX, decltype(&EVP_CIPHER_CTX_free)> ctx(EVP_CIPHER_CTX_new(),
                                                                        &EVP_CIPHER_CTX_free);
    if (!ctx) return false;

    if (EVP_DecryptInit_ex(ctx.get(), EVP_aes_256_gcm(), nullptr, nullptr, nullptr) != 1) {
        return false;
    }
    if (EVP_CIPHER_CTX_ctrl(ctx.get(), EVP_CTRL_GCM_SET_IVLEN, static_cast<int>(kNonceLen),
                            nullptr) != 1) {
        return false;
    }
    if (EVP_DecryptInit_ex(ctx.get(), nullptr, nullptr, master_key.data(), nonce) != 1) {
        return false;
    }

    out_plaintext.assign(ciphertext_len, '\0');
    int written = 0;
    if (ciphertext_len > 0 &&
        EVP_DecryptUpdate(ctx.get(), reinterpret_cast<unsigned char*>(out_plaintext.data()),
                          &written, ciphertext, static_cast<int>(ciphertext_len)) != 1) {
        return false;
    }
    int total = written;

    if (EVP_CIPHER_CTX_ctrl(ctx.get(), EVP_CTRL_GCM_SET_TAG, static_cast<int>(kTagLen),
                            const_cast<unsigned char*>(tag)) != 1) {
        return false;
    }

    int final_written = 0;
    if (EVP_DecryptFinal_ex(ctx.get(),
                            reinterpret_cast<unsigned char*>(out_plaintext.data() + total),
                            &final_written) != 1) {
        return false;
    }
    total += final_written;
    out_plaintext.resize(static_cast<size_t>(total));
    return true;
}

SessionId next_recovery_session_id() {
    static std::atomic<uint64_t> counter{1};
    constexpr uint64_t kRecoverySessionPrefix = 1ull << 63;
    return kRecoverySessionPrefix | counter.fetch_add(1, std::memory_order_relaxed);
}

}  // namespace

VoiceService::VoiceService(std::string api_key, std::string api_secret,
                           infra::redis::RedisClient& redis, SessionManager& session_manager,
                           SubscriptionManager& subscription_manager,
                           app::services::HubService& hub_service,
                           net::outbound::IOutboundSink& outbound_sink,
                           const std::vector<core::LivekitNodeConfig>& nodes)
    : redis_(redis),
      session_manager_(session_manager),
      subscription_manager_(subscription_manager),
      hub_service_(hub_service),
      outbound_sink_(outbound_sink),
      token_service_(std::move(api_key), std::move(api_secret)),
      e2ee_key_ttl_(parse_positive_seconds(utils::EnvLoader::get_env("VOICE_E2EE_KEY_TTL_SEC", ""),
                                           kDefaultE2EEKeyTtl)),
      e2ee_rekey_guard_ttl_(parse_positive_seconds(
          utils::EnvLoader::get_env("VOICE_E2EE_REKEY_GUARD_SEC", ""), kDefaultE2EERekeyGuardTtl)),
      reconcile_interval_(parse_reconcile_interval_seconds(
          utils::EnvLoader::get_env("LIVEKIT_RECONCILE_INTERVAL_SEC", ""),
          kDefaultReconcileInterval)),
      livekit_missing_clear_ttl_(
          parse_positive_seconds(utils::EnvLoader::get_env("LIVEKIT_MISSING_CLEAR_SEC", ""),
                                 kDefaultLivekitMissingClearTtl)),
      remote_missing_evidence_(livekit_missing_clear_ttl_) {
    const auto storage_secret = utils::EnvLoader::get_env(
        "VOICE_E2EE_STORAGE_SECRET", utils::EnvLoader::get_env("LIVEKIT_API_SECRET", ""));
    e2ee_storage_key_ready_ = derive_master_key(storage_secret, e2ee_storage_master_key_);
    if (!e2ee_storage_key_ready_) {
        utils::EventLogger::instance().log(utils::EventCategory::VOICE, "",
                                           "e2ee_storage_master_key_unavailable", 0,
                                           "reason=missing_or_invalid_secret");
    }

    for (const auto& node : nodes) {
        nodes_.register_node({node.id, node.public_url, node.private_url});
    }
}

VoiceService::~VoiceService() { stop_reconcile_loop(); }

void VoiceService::start_reconcile_loop() {
    if (reconcile_running_.exchange(true)) return;
    reconcile_stop_requested_.store(false, std::memory_order_release);
    try {
        reconcile_full_state("startup");
    } catch (const std::exception& ex) {
        utils::EventLogger::instance().log(utils::EventCategory::VOICE, "",
                                           "reconcile_startup_failed", 0,
                                           std::string("error=") + ex.what());
    }
    reconcile_thread_ = std::thread(&VoiceService::run_reconcile_loop, this);
}

void VoiceService::stop_reconcile_loop() {
    if (!reconcile_running_.exchange(false)) return;
    reconcile_stop_requested_.store(true, std::memory_order_release);
    {
        std::lock_guard lock(reconcile_mutex_);
        pending_reconcile_channels_.clear();
    }
    reconcile_cv_.notify_all();
    if (reconcile_thread_.joinable()) {
        reconcile_thread_.join();
    }
}

VoiceService::JoinVoiceToken VoiceService::join_voice(const ChannelId& channel, const UserId& user,
                                                      SessionId app_session_id,
                                                      std::string_view intent_nonce) {
    JoinVoiceToken response;

    if (is_channel_rekey_in_progress(channel) && !clear_channel_rekey_if_empty(channel)) {
        response.error_reason = "voice_rekey_in_progress";
        return response;
    }

    auto node = nodes_.get_room_node(channel);
    if (!node) {
        node = nodes_.pick_node();
        if (node) nodes_.bind_room(channel, node->node_id);
    }

    if (!node) return response;
    clear_channel_remote_missing_confirmation(channel);

    std::optional<std::string> key = e2ee_keys_.get_key(channel);
    if (!key.has_value()) {
        key = load_channel_e2ee_key_from_storage(channel);
        if (key.has_value()) {
            e2ee_keys_.set_key(channel, *key);
            utils::EventLogger::instance().log(utils::EventCategory::VOICE, user.value,
                                               "e2ee_key_loaded", 0,
                                               "channel=" + channel.value + " source=redis");
        }
    }

    if (!key.has_value()) {
        const auto empty_state = is_channel_effectively_empty(channel);
        if (!empty_state.has_value() || !*empty_state) {
            utils::EventLogger::instance().log(utils::EventCategory::VOICE, user.value,
                                               "e2ee_key_missing_active_room", 0,
                                               "channel=" + channel.value);
            force_channel_rekey(channel, "missing_or_invalid_key_for_active_room");
            response.error_reason = "voice_rekey_in_progress";
            return response;
        }

        try {
            key = e2ee_keys_.get_or_create_key(channel);
        } catch (const std::exception& ex) {
            utils::EventLogger::instance().log(utils::EventCategory::VOICE, user.value,
                                               "e2ee_key_generate_failed", 0,
                                               "channel=" + channel.value + " error=" + ex.what());
            response.error_reason = "voice_key_unavailable";
            return response;
        }
        if (key.has_value()) {
            (void)persist_channel_e2ee_key_to_storage(channel, *key);
            utils::EventLogger::instance().log(
                utils::EventCategory::VOICE, user.value, "e2ee_key_generated", 0,
                "channel=" + channel.value + " reason=room_effectively_empty");
        }
    } else {
        (void)persist_channel_e2ee_key_to_storage(channel, *key);
    }

    if (!key.has_value()) {
        response.error_reason = "voice_key_unavailable";
        return response;
    }

    livekit::LiveKitTokenService::ParticipantTokenRequest req{
        .identity = user,
        .room = channel,
        .node_id = node->node_id,
        .app_session_id = app_session_id,
        .intent_nonce = std::string(intent_nonce),
        .can_publish = true,
        .can_subscribe = true,
        .ttl = std::chrono::minutes(10)};

    const std::string token = token_service_.mint_participant_token(req);

    response.token = token;
    response.livekit_url = node->public_host;
    response.expires_in = 600;
    response.e2ee_key = *key;
    response.resume_id = rotate_resume_id(user);

    return response;
}

bool VoiceService::stage_pending_join_intent(const UserId& user, const PendingJoinIntent& intent,
                                             uint64_t expires_in_seconds) {
    const auto ttl = std::chrono::seconds(expires_in_seconds + 10);

    try {
        json doc;
        doc["user_id"] = user.value;
        doc["session_id"] = intent.session_id;
        doc["intent_nonce"] = intent.intent_nonce;
        doc["to_channel"] = intent.to_channel.value;
        doc["from_channel"] = intent.has_from_channel ? intent.from_channel.value : "";
        doc["has_from_channel"] = intent.has_from_channel;
        doc["muted"] = intent.muted;
        doc["deafened"] = intent.deafened;
        doc["old_leave_seen"] = intent.old_leave_seen;
        doc["new_join_seen"] = intent.new_join_seen;
        doc["expires_at_unix"] = unix_now_seconds() + static_cast<uint64_t>(ttl.count());

        redis_.setex(pending_join_key(user), ttl, doc.dump());
        return true;
    } catch (const std::exception& ex) {
        utils::EventLogger::instance().log(utils::EventCategory::VOICE, user.value,
                                           "pending_join_write_failed", 0,
                                           std::string("error=") + ex.what());
        return false;
    }
}

void VoiceService::clear_pending_join_intent(const UserId& user) {
    try {
        redis_.del(pending_join_key(user));
    } catch (...) {
    }
}

std::optional<VoiceService::PendingJoinIntent> VoiceService::read_pending_join_intent(
    const UserId& user) const {
    try {
        const auto raw = redis_.get(pending_join_key(user));
        if (!raw.has_value()) return std::nullopt;

        const auto doc = json::parse(*raw, nullptr, false);
        if (!doc.is_object()) return std::nullopt;

        PendingJoinIntent intent;

        if (doc.contains("session_id")) {
            if (doc["session_id"].is_number_unsigned()) {
                intent.session_id = static_cast<SessionId>(doc["session_id"].get<uint64_t>());
            } else if (doc["session_id"].is_string()) {
                intent.session_id =
                    static_cast<SessionId>(std::stoull(doc["session_id"].get<std::string>()));
            }
        }

        if (doc.contains("intent_nonce") && doc["intent_nonce"].is_string()) {
            intent.intent_nonce = doc["intent_nonce"].get<std::string>();
        }

        if (!doc.contains("to_channel") || !doc["to_channel"].is_string()) {
            return std::nullopt;
        }
        intent.to_channel = ChannelId(doc["to_channel"].get<std::string>());

        if (doc.contains("has_from_channel") && doc["has_from_channel"].is_boolean()) {
            intent.has_from_channel = doc["has_from_channel"].get<bool>();
        }
        if (doc.contains("from_channel") && doc["from_channel"].is_string()) {
            const auto from = doc["from_channel"].get<std::string>();
            if (!from.empty()) {
                intent.from_channel = ChannelId(from);
                intent.has_from_channel = true;
            }
        }

        if (doc.contains("muted") && doc["muted"].is_boolean()) {
            intent.muted = doc["muted"].get<bool>();
        }
        if (doc.contains("deafened") && doc["deafened"].is_boolean()) {
            intent.deafened = doc["deafened"].get<bool>();
        }
        if (doc.contains("old_leave_seen") && doc["old_leave_seen"].is_boolean()) {
            intent.old_leave_seen = doc["old_leave_seen"].get<bool>();
        }
        if (doc.contains("new_join_seen") && doc["new_join_seen"].is_boolean()) {
            intent.new_join_seen = doc["new_join_seen"].get<bool>();
        }
        if (doc.contains("expires_at_unix") && doc["expires_at_unix"].is_number_unsigned()) {
            intent.expires_at_unix = doc["expires_at_unix"].get<uint64_t>();
        }

        return intent;
    } catch (...) {
        return std::nullopt;
    }
}

bool VoiceService::update_pending_join_intent(const UserId& user, const PendingJoinIntent& intent) {
    try {
        const auto ttl = redis_.ttl(pending_join_key(user));
        if (!ttl.has_value() || ttl->count() <= 0) {
            return false;
        }

        json doc;
        doc["user_id"] = user.value;
        doc["session_id"] = intent.session_id;
        doc["intent_nonce"] = intent.intent_nonce;
        doc["to_channel"] = intent.to_channel.value;
        doc["from_channel"] = intent.has_from_channel ? intent.from_channel.value : "";
        doc["has_from_channel"] = intent.has_from_channel;
        doc["muted"] = intent.muted;
        doc["deafened"] = intent.deafened;
        doc["old_leave_seen"] = intent.old_leave_seen;
        doc["new_join_seen"] = intent.new_join_seen;
        doc["expires_at_unix"] = intent.expires_at_unix;

        redis_.setex(pending_join_key(user), *ttl, doc.dump());
        return true;
    } catch (...) {
        return false;
    }
}

VoiceService::ParticipantMetadata VoiceService::parse_participant_metadata(
    std::string_view metadata_raw) {
    ParticipantMetadata out;
    if (metadata_raw.empty()) return out;

    const auto metadata = json::parse(std::string(metadata_raw), nullptr, false);
    if (!metadata.is_object()) {
        out.node_id = std::string(metadata_raw);
        return out;
    }

    out.structured = true;

    if (metadata.contains("node_id") && metadata["node_id"].is_string()) {
        out.node_id = metadata["node_id"].get<std::string>();
    }

    if (metadata.contains("app_session_id")) {
        if (metadata["app_session_id"].is_number_unsigned()) {
            out.app_session_id = metadata["app_session_id"].get<uint64_t>();
        } else if (metadata["app_session_id"].is_string()) {
            try {
                out.app_session_id = static_cast<uint64_t>(
                    std::stoull(metadata["app_session_id"].get<std::string>()));
            } catch (...) {
                out.app_session_id = 0;
            }
        }
    }

    if (metadata.contains("intent_nonce") && metadata["intent_nonce"].is_string()) {
        out.intent_nonce = metadata["intent_nonce"].get<std::string>();
    }

    return out;
}

std::optional<std::string> VoiceService::resolve_participant_node_assignment(
    const ChannelId& channel,
    const std::unordered_map<UserId, livekit::cli::ParticipantInfo>& participants,
    std::string_view reason) const {
    std::optional<std::string> resolved_node_id;
    for (const auto& [_, participant] : participants) {
        const auto metadata = parse_participant_metadata(participant.metadata);
        if (metadata.node_id.empty() || !nodes_.get_node(metadata.node_id)) continue;

        if (!resolved_node_id.has_value()) {
            resolved_node_id = metadata.node_id;
            continue;
        }
        if (*resolved_node_id == metadata.node_id) continue;

        utils::EventLogger::instance().log(
            utils::EventCategory::VOICE, "", "room_assignment_metadata_conflict", 0,
            "channel=" + channel.value + " first_node=" + *resolved_node_id +
                " conflicting_node=" + metadata.node_id + " reason=" + std::string(reason));
        return std::nullopt;
    }
    return resolved_node_id;
}

bool VoiceService::mark_webhook_event_seen(const std::string& event_id) {
    if (event_id.empty()) return true;

    try {
        return redis_.setnxex(webhook_seen_key(event_id), kWebhookDedupTtl, "1");
    } catch (const std::exception& ex) {
        utils::EventLogger::instance().log(
            utils::EventCategory::VOICE, "", "webhook_dedup_failed", 0,
            std::string("event_id=") + event_id + " error=" + ex.what());
        return true;
    }
}

bool VoiceService::verified_kick_user(const ChannelId& channel, const UserId& target) {
    auto node = nodes_.get_room_node(channel);
    if (!node) return true;

    livekit::cli::LivekitClient client(node->private_host, token_service_);

    if (client.RemoveParticipant(channel, target)) {
        return true;
    }

    std::vector<livekit::cli::ParticipantInfo> participants;
    try {
        participants = client.ListParticipants(channel);
    } catch (const std::exception& ex) {
        utils::EventLogger::instance().log(utils::EventCategory::VOICE, target.value,
                                           "kick_fallback_check_failed", 0,
                                           "channel=" + channel.value + " error=" + ex.what());
        return false;
    }

    if (contains_participant(participants, target)) {
        utils::EventLogger::instance().log(utils::EventCategory::VOICE, target.value,
                                           "kick_remove_failed", 0, "channel=" + channel.value);
        return false;
    }

    utils::EventLogger::instance().log(utils::EventCategory::VOICE, target.value,
                                       "kick_remove_fallback_absent", 0,
                                       "channel=" + channel.value);
    return true;
}

void VoiceService::on_channel_empty(const ChannelId& channel) {
    e2ee_keys_.clear_key(channel);
    clear_channel_e2ee_key_from_storage(channel);
    clear_channel_rekey_guard(channel);
}

void VoiceService::on_channel_finish(const ChannelId& channel) {
    reset_remote_missing_confirmations(channel);

    const auto removed_users = sessions_.clear_channel(channel);
    const auto channel_info = hub_service_.getChannel(channel);
    for (const auto& user : removed_users) {
        if (channel_info.has_value()) {
            publish_voice_participant_remove(channel_info->hub_id, channel, user);
        }
        publish_self_status(user, false, std::nullopt, std::nullopt);
        clear_resume_id(user);
    }

    nodes_.clear_room(channel);
    e2ee_keys_.clear_key(channel);
    clear_channel_e2ee_key_from_storage(channel);
    clear_channel_rekey_guard(channel);
    clear_channel_started_at_unix(channel);
}

void VoiceService::force_local_leave(const UserId& user, const ChannelId& channel,
                                     std::string_view reason) {
    clear_participant_remote_missing_confirmation(channel, user);

    const bool became_empty = sessions_.leave(channel, user);

    utils::EventLogger::instance().log(
        utils::EventCategory::VOICE, user.value, "reconcile_forced_leave", 0,
        "channel=" + channel.value + " reason=" + std::string(reason));

    if (auto channel_info = hub_service_.getChannel(channel)) {
        publish_voice_participant_remove(channel_info->hub_id, channel, user);
    }

    publish_self_status(user, false, std::nullopt, std::nullopt);
    clear_resume_id(user);

    if (became_empty) {
        clear_channel_started_at_unix(channel);
        on_channel_empty(channel);
    }
}

bool VoiceService::kick_user(const ChannelId& channel, const UserId& target) {
    auto node = nodes_.get_room_node(channel);
    if (!node) return false;

    livekit::cli::LivekitClient client(node->private_host, token_service_);
    return client.RemoveParticipant(channel, target);
}

void VoiceService::mark_channel_takeover(const ChannelId& channel) {
    std::lock_guard lock(takeover_guard_mutex_);
    channel_takeover_guard_[channel] = std::chrono::steady_clock::now() + kTakeoverGuardTtl;
}

void VoiceService::set_channel_started_at_unix(const ChannelId& channel, uint64_t started_at_unix) {
    std::lock_guard lock(channel_started_mutex_);
    channel_started_at_unix_[channel] = started_at_unix;
}

void VoiceService::clear_channel_started_at_unix(const ChannelId& channel) {
    std::lock_guard lock(channel_started_mutex_);
    channel_started_at_unix_.erase(channel);
}

uint64_t VoiceService::read_channel_started_at_unix(const ChannelId& channel) const {
    std::lock_guard lock(channel_started_mutex_);
    if (const auto it = channel_started_at_unix_.find(channel);
        it != channel_started_at_unix_.end()) {
        return it->second;
    }
    return 0;
}

uint64_t VoiceService::channel_started_at_unix(const ChannelId& channel) const {
    return read_channel_started_at_unix(channel);
}

bool VoiceService::consume_channel_takeover_guard(const ChannelId& channel) {
    std::lock_guard lock(takeover_guard_mutex_);
    auto it = channel_takeover_guard_.find(channel);
    if (it == channel_takeover_guard_.end()) return false;

    if (it->second < std::chrono::steady_clock::now()) {
        channel_takeover_guard_.erase(it);
        return false;
    }

    channel_takeover_guard_.erase(it);
    return true;
}

bool VoiceService::is_stale_participant_event(const livekit::webhook::LiveKitEvent& event) {
    if (event.timestamp_ms == 0 || event.user_id.value.empty()) return false;

    std::lock_guard lock(event_order_mutex_);
    auto& last_ts = user_last_event_ts_ms_[event.user_id];
    if (last_ts != 0 && event.timestamp_ms < last_ts) {
        return true;
    }
    if (event.timestamp_ms > last_ts) {
        last_ts = event.timestamp_ms;
    }
    return false;
}

bool VoiceService::is_stale_room_event(const livekit::webhook::LiveKitEvent& event) {
    if (event.timestamp_ms == 0 || event.channel_id.value.empty()) return false;

    std::lock_guard lock(event_order_mutex_);
    auto& last_ts = channel_last_room_event_ts_ms_[event.channel_id];
    if (last_ts != 0 && event.timestamp_ms < last_ts) {
        return true;
    }
    if (event.timestamp_ms > last_ts) {
        last_ts = event.timestamp_ms;
    }
    return false;
}

void VoiceService::request_channel_reconcile(const ChannelId& channel, std::string_view reason) {
    if (channel.value.empty()) return;

    bool inserted = false;
    {
        std::lock_guard lock(reconcile_mutex_);
        inserted = pending_reconcile_channels_.insert(channel).second;
    }

    if (inserted) {
        utils::EventLogger::instance().log(
            utils::EventCategory::VOICE, "", "reconcile_channel_requested", 0,
            "channel=" + channel.value + " reason=" + std::string(reason));
        reconcile_cv_.notify_one();
    }
}

bool VoiceService::confirm_channel_remote_missing(const ChannelId& channel,
                                                  std::string_view reason) {
    const auto result = remote_missing_evidence_.observe_channel_missing(channel, reason);

    utils::EventLogger::instance().log(
        utils::EventCategory::VOICE, "",
        result.confirmed ? "reconcile_room_missing_remote_confirmed"
                         : "reconcile_room_missing_remote_suspected",
        0,
        "channel=" + channel.value + " reason=" + std::string(reason) + " first_reason=" +
            result.first_reason + " first_seen=" + (result.first_observation ? "1" : "0") +
            " missing_clear_sec=" + std::to_string(livekit_missing_clear_ttl_.count()));
    return result.confirmed;
}

void VoiceService::clear_channel_remote_missing_confirmation(const ChannelId& channel) {
    remote_missing_evidence_.clear_channel_missing(channel);
}

bool VoiceService::confirm_participant_remote_missing(const ChannelId& channel, const UserId& user,
                                                      std::string_view reason) {
    const auto result = remote_missing_evidence_.observe_participant_missing(channel, user, reason);

    utils::EventLogger::instance().log(
        utils::EventCategory::VOICE, user.value,
        result.confirmed ? "reconcile_participant_missing_remote_confirmed"
                         : "reconcile_participant_missing_remote_suspected",
        0,
        "channel=" + channel.value + " reason=" + std::string(reason) + " first_reason=" +
            result.first_reason + " first_seen=" + (result.first_observation ? "1" : "0") +
            " missing_clear_sec=" + std::to_string(livekit_missing_clear_ttl_.count()));
    return result.confirmed;
}

void VoiceService::clear_participant_remote_missing_confirmation(const ChannelId& channel,
                                                                 const UserId& user) {
    remote_missing_evidence_.clear_participant_missing(channel, user);
}

void VoiceService::reset_remote_missing_confirmations(const ChannelId& channel) {
    remote_missing_evidence_.reset_channel(channel);
}

bool VoiceService::has_active_owner_connection(const UserId& user, const ChannelId& channel) const {
    const auto current_channel = sessions_.user_channel(user);
    if (!current_channel.has_value() || *current_channel != channel) {
        return false;
    }

    const auto owner_session = sessions_.user_session(user);
    return owner_session.has_value() && session_manager_.sessionIdHasConnections(*owner_session);
}

std::size_t VoiceService::active_owner_connection_count(const ChannelId& channel) const {
    std::size_t count = 0;
    for (const auto& participant : sessions_.participants_in_channel(channel)) {
        if (has_active_owner_connection(participant.user_id, channel)) {
            ++count;
        }
    }
    return count;
}

void VoiceService::run_reconcile_loop() {
    auto next_full_run = std::chrono::steady_clock::now() + reconcile_interval_;

    while (!reconcile_stop_requested_.load(std::memory_order_acquire)) {
        std::vector<ChannelId> channels_to_reconcile;
        {
            std::unique_lock lock(reconcile_mutex_);
            reconcile_cv_.wait_until(lock, next_full_run, [this]() {
                return reconcile_stop_requested_.load(std::memory_order_acquire) ||
                       !pending_reconcile_channels_.empty();
            });

            if (reconcile_stop_requested_.load(std::memory_order_acquire)) {
                break;
            }

            channels_to_reconcile.assign(pending_reconcile_channels_.begin(),
                                         pending_reconcile_channels_.end());
            pending_reconcile_channels_.clear();
        }

        for (const auto& channel : channels_to_reconcile) {
            try {
                reconcile_channel_state(channel, "on_demand");
            } catch (const std::exception& ex) {
                utils::EventLogger::instance().log(
                    utils::EventCategory::VOICE, "", "reconcile_channel_failed", 0,
                    "channel=" + channel.value + " error=" + ex.what());
            }
        }

        const auto now = std::chrono::steady_clock::now();
        if (now >= next_full_run) {
            try {
                reconcile_full_state("periodic");
            } catch (const std::exception& ex) {
                utils::EventLogger::instance().log(utils::EventCategory::VOICE, "",
                                                   "reconcile_periodic_failed", 0,
                                                   std::string("error=") + ex.what());
            }
            next_full_run = std::chrono::steady_clock::now() + reconcile_interval_;
        }
    }
}

void VoiceService::reconcile_full_state(std::string_view reason) {
    std::unordered_set<ChannelId> channels;
    for (const auto& channel : sessions_.active_channels()) {
        channels.insert(channel);
    }
    for (const auto& channel : remote_missing_evidence_.tracked_missing_channels()) {
        channels.insert(channel);
    }

    for (const auto& node : nodes_.list_nodes()) {
        livekit::cli::LivekitClient client(node.private_host, token_service_);

        std::vector<ChannelId> rooms;
        try {
            rooms = client.ListRooms();
        } catch (const std::exception& ex) {
            utils::EventLogger::instance().log(
                utils::EventCategory::VOICE, "", "reconcile_list_rooms_failed", 0,
                "node=" + node.node_id + " method=ListRooms endpoint=" + node.private_host +
                    " error=" + ex.what());
            continue;
        }

        for (const auto& room : rooms) {
            channels.insert(room);
        }
    }

    for (const auto& channel : channels) {
        reconcile_channel_state(channel, reason);
    }
}

void VoiceService::reconcile_channel_state(const ChannelId& channel, std::string_view reason) {
    std::unordered_map<UserId, livekit::cli::ParticipantInfo> remote_by_user;
    auto assigned_node = nodes_.get_room_node(channel);
    std::string assigned_node_id = assigned_node ? assigned_node->node_id : "";
    std::string assigned_private_host = assigned_node ? assigned_node->private_host : "";
    std::string first_remote_node_id;
    std::string first_remote_private_host;
    ReconcileQuerySummary query_summary;
    const auto configured_nodes = nodes_.list_nodes();
    query_summary.configured_queries = configured_nodes.size();

    for (const auto& node : configured_nodes) {
        livekit::cli::LivekitClient client(node.private_host, token_service_);

        std::vector<livekit::cli::ParticipantInfo> participants;
        try {
            participants = client.ListParticipants(channel);
            ++query_summary.successful_queries;
            query_summary.participant_count += participants.size();
        } catch (const std::exception& ex) {
            ++query_summary.failed_queries;
            utils::EventLogger::instance().log(
                utils::EventCategory::VOICE, "", "reconcile_list_participants_failed", 0,
                "node=" + node.node_id + " method=ListParticipants endpoint=" + node.private_host +
                    " channel=" + channel.value + " error=" + ex.what());
            continue;
        } catch (...) {
            ++query_summary.failed_queries;
            utils::EventLogger::instance().log(
                utils::EventCategory::VOICE, "", "reconcile_list_participants_failed", 0,
                "node=" + node.node_id + " method=ListParticipants endpoint=" + node.private_host +
                    " channel=" + channel.value + " error=unknown");
            continue;
        }

        if (!participants.empty() && first_remote_node_id.empty()) {
            first_remote_node_id = node.node_id;
            first_remote_private_host = node.private_host;
        }

        for (const auto& participant : participants) {
            if (participant.identity.value.empty()) continue;
            remote_by_user.emplace(participant.identity, participant);
        }
    }
    const auto local_participants = sessions_.participants_in_channel(channel);
    const std::string query_details =
        " configured_queries=" + std::to_string(query_summary.configured_queries) +
        " successful_queries=" + std::to_string(query_summary.successful_queries) +
        " failed_queries=" + std::to_string(query_summary.failed_queries) +
        " participant_count=" + std::to_string(query_summary.participant_count);
    const bool all_queries_succeeded = query_summary.all_queries_succeeded();
    if (!all_queries_succeeded) {
        reset_remote_missing_confirmations(channel);
        utils::EventLogger::instance().log(
            utils::EventCategory::VOICE, "", "reconcile_participants_inconclusive", 0,
            "channel=" + channel.value + " reason=" + std::string(reason) + query_details);
    }

    if (remote_by_user.empty()) {
        if (!all_queries_succeeded) {
            return;
        }
        if (!query_summary.unanimously_absent()) {
            clear_channel_remote_missing_confirmation(channel);
            utils::EventLogger::instance().log(
                utils::EventCategory::VOICE, "", "reconcile_participants_unusable", 0,
                "channel=" + channel.value + " reason=" + std::string(reason) + query_details);
            return;
        }

        utils::EventLogger::instance().log(
            utils::EventCategory::VOICE, "", "reconcile_room_missing_remote", 0,
            "channel=" + channel.value + " reason=" + std::string(reason) + query_details);
        const auto active_owner_count = active_owner_connection_count(channel);
        if (active_owner_count > 0) {
            if (confirm_channel_remote_missing(channel, "room_missing_with_active_owner")) {
                utils::EventLogger::instance().log(
                    utils::EventCategory::VOICE, "",
                    "reconcile_room_missing_active_owner_confirmed", 0,
                    "channel=" + channel.value + " reason=" + std::string(reason) +
                        " active_owner_connections=" + std::to_string(active_owner_count) +
                        query_details);

                on_channel_finish(channel);
            } else {
                utils::EventLogger::instance().log(
                    utils::EventCategory::VOICE, "", "reconcile_room_missing_remote_deferred", 0,
                    "channel=" + channel.value + " reason=" + std::string(reason) +
                        " active_owner_connections=" + std::to_string(active_owner_count) +
                        query_details);
            }
        } else if (confirm_channel_remote_missing(channel, reason)) {
            on_channel_finish(channel);
        }
        return;
    }

    clear_channel_remote_missing_confirmation(channel);

    const auto metadata_node_id =
        resolve_participant_node_assignment(channel, remote_by_user, "reconcile");
    if (metadata_node_id.has_value()) {
        auto metadata_node = nodes_.get_node(*metadata_node_id);
        if (metadata_node) {
            if (assigned_node_id.empty()) {
                utils::EventLogger::instance().log(
                    utils::EventCategory::VOICE, "", "reconcile_room_assignment_recovered", 0,
                    "channel=" + channel.value + " assigned_node=" + metadata_node->node_id +
                        " source=participant_metadata");
            } else if (assigned_node_id != metadata_node->node_id) {
                utils::EventLogger::instance().log(
                    utils::EventCategory::VOICE, "", "reconcile_room_assignment_repaired", 0,
                    "channel=" + channel.value + " previous_node=" + assigned_node_id +
                        " assigned_node=" + metadata_node->node_id +
                        " source=participant_metadata");
            }
            assigned_node_id = metadata_node->node_id;
            assigned_private_host = metadata_node->private_host;
            nodes_.bind_room(channel, assigned_node_id);
        }
    }

    if (assigned_node_id.empty()) {
        if (!first_remote_node_id.empty()) {
            assigned_node_id = first_remote_node_id;
            assigned_private_host = first_remote_private_host;
            utils::EventLogger::instance().log(
                utils::EventCategory::VOICE, "", "reconcile_room_assignment_recovered", 0,
                "channel=" + channel.value + " assigned_node=" + assigned_node_id +
                    " source=cluster_endpoint_fallback");
            nodes_.bind_room(channel, assigned_node_id);
        }
    }

    if (!assigned_node_id.empty()) {
        nodes_.increment_room(channel, assigned_node_id);
    }

    std::unordered_set<UserId> local_users;
    for (const auto& participant : local_participants) {
        local_users.insert(participant.user_id);
    }

    std::unordered_set<UserId> remote_users;
    for (const auto& [user_id, _] : remote_by_user) {
        remote_users.insert(user_id);
    }

    if (all_queries_succeeded) {
        for (const auto& participant : local_participants) {
            if (remote_users.find(participant.user_id) != remote_users.end()) continue;
            if (has_active_owner_connection(participant.user_id, channel)) {
                if (confirm_participant_remote_missing(channel, participant.user_id,
                                                       "participant_missing_with_active_owner")) {
                    utils::EventLogger::instance().log(
                        utils::EventCategory::VOICE, participant.user_id.value,
                        "reconcile_participant_missing_active_owner_confirmed", 0,
                        "channel=" + channel.value +
                            " reason=participant_missing_in_livekit active_owner_connection=1");

                    force_local_leave(participant.user_id, channel,
                                      "participant_missing_with_active_owner");
                } else {
                    utils::EventLogger::instance().log(
                        utils::EventCategory::VOICE, participant.user_id.value,
                        "reconcile_participant_missing_remote_deferred", 0,
                        "channel=" + channel.value +
                            " reason=participant_missing_in_livekit active_owner_connection=1");
                }
                continue;
            }

            if (confirm_participant_remote_missing(channel, participant.user_id,
                                                   "participant_missing_in_livekit")) {
                force_local_leave(participant.user_id, channel, "participant_missing_in_livekit");
            }
        }
    }

    const std::string remove_private_host =
        !assigned_private_host.empty() ? assigned_private_host : first_remote_private_host;
    livekit::cli::LivekitClient remove_client(remove_private_host, token_service_);
    for (const auto& [user_id, participant] : remote_by_user) {
        clear_participant_remote_missing_confirmation(channel, user_id);
        if (local_users.find(user_id) != local_users.end()) continue;

        const auto metadata = parse_participant_metadata(participant.metadata);
        const auto pending = read_pending_join_intent(user_id);

        bool correlation_valid = false;
        if (pending.has_value() && pending->to_channel == channel) {
            const bool session_match =
                metadata.app_session_id != 0 &&
                static_cast<SessionId>(metadata.app_session_id) == pending->session_id;
            const bool nonce_match = !pending->intent_nonce.empty() &&
                                     !metadata.intent_nonce.empty() &&
                                     pending->intent_nonce == metadata.intent_nonce;
            correlation_valid = session_match && nonce_match;
        } else if (metadata.app_session_id != 0) {
            const auto sessions = session_manager_.getUserSessionIds(user_id);
            correlation_valid = std::any_of(sessions.begin(), sessions.end(), [&](SessionId sid) {
                return sid == static_cast<SessionId>(metadata.app_session_id);
            });
        }

        if (!correlation_valid) {
            bool removed = false;
            if (all_queries_succeeded && !remove_private_host.empty()) {
                removed = remove_client.RemoveParticipant(channel, user_id);
            }
            utils::EventLogger::instance().log(
                utils::EventCategory::VOICE, user_id.value,
                "reconcile_untrusted_remote_participant", 0,
                "channel=" + channel.value + " reason=" + std::string(reason) +
                    " app_session_id=" + std::to_string(metadata.app_session_id) +
                    " has_intent_nonce=" + (metadata.intent_nonce.empty() ? "0" : "1") +
                    " all_queries_succeeded=" + (all_queries_succeeded ? "1" : "0") +
                    " removed=" + (removed ? "1" : "0"));
            continue;
        }

        livekit::webhook::LiveKitEvent synthetic_event;
        synthetic_event.type = livekit::webhook::LiveKitEventType::PARTICIPANT_JOINED;
        synthetic_event.raw_event_name = "participant_joined(reconcile)";
        synthetic_event.channel_id = channel;
        synthetic_event.user_id = user_id;
        synthetic_event.participant_sid = participant.sid;
        synthetic_event.participant_metadata = participant.metadata;
        synthetic_event.app_session_id = metadata.app_session_id;
        synthetic_event.intent_nonce = metadata.intent_nonce;
        synthetic_event.node_id = metadata.node_id.empty() ? assigned_node_id : metadata.node_id;
        synthetic_event.timestamp_ms =
            static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(
                                      std::chrono::system_clock::now().time_since_epoch())
                                      .count());

        handle_participant_joined(synthetic_event);

        const auto active_channel = sessions_.user_channel(user_id);
        if (active_channel.has_value() && *active_channel == channel) {
            const std::string node_id =
                assigned_node_id.empty() ? synthetic_event.node_id : assigned_node_id;
            if (!node_id.empty()) {
                nodes_.increment_user(node_id);
            }
        }
    }
}

void VoiceService::emit(net::outbound::OutgoingMessage msg) {
    (void)outbound_sink_.push(std::move(msg));
    utils::metrics::counters().outbound_msgs_total.fetch_add(1, std::memory_order_relaxed);
}

void VoiceService::publish_voice_snapshot(const HubId& hub, const ChannelId& channel,
                                          uint64_t started_at_unix) {
    utils::metrics::counters().fanout_subscriber_snapshot_total.fetch_add(
        1, std::memory_order_relaxed);
    auto subs = subscription_manager_.getSubscribers(Topic::HubTopic(hub));
    if (!subs || subs->empty()) return;

    sercom::protocol::event::StateDelta delta;
    auto* hub_delta = delta.add_hubs();
    hub_delta->set_hub_id(hub.value);
    auto* channel_delta = hub_delta->add_channels();
    channel_delta->set_channel_id(channel.value);
    auto* snapshot = channel_delta->add_voice_ops()->mutable_snapshot()->mutable_state();
    snapshot->set_started_at_unix(started_at_unix);
    for (const auto& participant : sessions_.participants_in_channel(channel)) {
        auto* out_participant = snapshot->add_participants();
        out_participant->set_user_id(participant.user_id.value);
        out_participant->set_muted(participant.muted);
        out_participant->set_deafened(participant.deafened);
    }

    std::vector<GlobalConnId> conns{subs->begin(), subs->end()};
    auto msg = ::app::make_outgoing_message(net::outbound::Target::many(std::move(conns)),
                                            ::app::make_state_delta(delta));
    emit(std::move(msg));
}

void VoiceService::publish_voice_participant_upsert(const HubId& hub, const ChannelId& channel,
                                                    const UserId& user, bool muted, bool deafened) {
    utils::metrics::counters().fanout_subscriber_snapshot_total.fetch_add(
        1, std::memory_order_relaxed);
    auto subs = subscription_manager_.getSubscribers(Topic::HubTopic(hub));
    if (!subs || subs->empty()) return;

    sercom::protocol::event::StateDelta delta;
    auto* hub_delta = delta.add_hubs();
    hub_delta->set_hub_id(hub.value);
    auto* channel_delta = hub_delta->add_channels();
    channel_delta->set_channel_id(channel.value);
    auto* upsert = channel_delta->add_voice_ops()->mutable_upsert()->mutable_participant();
    upsert->set_user_id(user.value);
    upsert->set_muted(muted);
    upsert->set_deafened(deafened);

    std::vector<GlobalConnId> conns{subs->begin(), subs->end()};
    auto msg = ::app::make_outgoing_message(net::outbound::Target::many(std::move(conns)),
                                            ::app::make_state_delta(delta));
    emit(std::move(msg));
}

void VoiceService::publish_voice_participant_remove(const HubId& hub, const ChannelId& channel,
                                                    const UserId& user) {
    utils::metrics::counters().fanout_subscriber_snapshot_total.fetch_add(
        1, std::memory_order_relaxed);
    auto subs = subscription_manager_.getSubscribers(Topic::HubTopic(hub));
    if (!subs || subs->empty()) return;

    sercom::protocol::event::StateDelta delta;
    auto* hub_delta = delta.add_hubs();
    hub_delta->set_hub_id(hub.value);
    auto* channel_delta = hub_delta->add_channels();
    channel_delta->set_channel_id(channel.value);
    channel_delta->add_voice_ops()->mutable_remove()->set_user_id(user.value);

    std::vector<GlobalConnId> conns{subs->begin(), subs->end()};
    auto msg = ::app::make_outgoing_message(net::outbound::Target::many(std::move(conns)),
                                            ::app::make_state_delta(delta));
    emit(std::move(msg));
}

void VoiceService::publish_self_status(const UserId& user, bool connected,
                                       const std::optional<SessionId>& owner_session_id,
                                       const std::optional<ChannelId>& channel,
                                       std::optional<SessionId> only_session_id) {
    std::optional<HubId> hub_id;
    if (connected && channel.has_value()) {
        if (const auto channel_info = hub_service_.getChannel(*channel)) {
            hub_id = channel_info->hub_id;
        }
    }

    const auto resume_id = connected ? read_resume_id(user) : std::nullopt;

    for (const auto& session_id : session_manager_.getUserSessionIds(user)) {
        if (only_session_id.has_value() && *only_session_id != session_id) continue;

        auto session_conns = session_manager_.getSessionIdConnections(session_id);
        if (session_conns.empty()) continue;

        const bool is_owner =
            connected && owner_session_id.has_value() && owner_session_id.value() == session_id;

        std::string bytes = make_voice_self_status_disconnected();
        if (connected && channel.has_value() && hub_id.has_value()) {
            bytes = make_voice_self_status_connected(is_owner, *hub_id, *channel,
                                                     is_owner ? resume_id : std::nullopt);
        }

        auto msg = ::app::make_outgoing_message(
            net::outbound::Target::many(std::move(session_conns)), std::move(bytes));
        emit(std::move(msg));
    }
}

void VoiceService::handle_participant_joined(const livekit::webhook::LiveKitEvent& event) {
    clear_channel_remote_missing_confirmation(event.channel_id);
    clear_participant_remote_missing_confirmation(event.channel_id, event.user_id);

    const auto pending_opt = read_pending_join_intent(event.user_id);

    PendingJoinIntent intent;
    bool has_correlated_intent = false;

    if (pending_opt.has_value()) {
        intent = *pending_opt;
        if (event.channel_id == intent.to_channel) {
            if (event.app_session_id != 0 || !event.intent_nonce.empty()) {
                has_correlated_intent = event.app_session_id == intent.session_id &&
                                        !intent.intent_nonce.empty() &&
                                        event.intent_nonce == intent.intent_nonce;
            } else {
                const auto sessions = session_manager_.getUserSessionIds(event.user_id);
                has_correlated_intent =
                    sessions.size() == 1 && sessions.front() == intent.session_id;
            }
        }
    } else if (event.app_session_id == 0 && event.intent_nonce.empty()) {
        const auto sessions = session_manager_.getUserSessionIds(event.user_id);
        if (sessions.size() == 1) {
            has_correlated_intent = true;
            intent.session_id = sessions.front();
            intent.to_channel = event.channel_id;
        }
    }

    if (!has_correlated_intent) {
        utils::EventLogger::instance().log(
            utils::EventCategory::VOICE, event.user_id.value, "join_intent_mismatch", 0,
            "event_channel=" + event.channel_id.value +
                " event_session=" + std::to_string(event.app_session_id));
        request_channel_reconcile(event.channel_id, "join_intent_mismatch");
        (void)kick_user(event.channel_id, event.user_id);
        return;
    }

    const bool first_join_in_channel =
        sessions_.join(event.channel_id, event.user_id, intent.session_id);
    (void)sessions_.set_deafened(event.user_id, intent.deafened);
    if (!intent.deafened) {
        (void)sessions_.set_muted(event.user_id, intent.muted);
    }

    if (first_join_in_channel) {
        const uint64_t started_at_unix =
            event.timestamp_ms > 0 ? (event.timestamp_ms / 1000) : unix_now_seconds();
        set_channel_started_at_unix(event.channel_id, started_at_unix);
    }

    bool final_muted = false;
    bool final_deafened = false;
    for (const auto& p : sessions_.participants_in_channel(event.channel_id)) {
        if (p.user_id == event.user_id) {
            final_muted = p.muted;
            final_deafened = p.deafened;
            break;
        }
    }

    const std::string_view join_source =
        event.raw_event_name == "participant_joined(reconcile)" ? "reconcile" : "webhook";
    utils::EventLogger::instance().voice_join(event.user_id.value, event.channel_id.value,
                                              join_source);

    if (!read_resume_id(event.user_id).has_value()) {
        (void)rotate_resume_id(event.user_id);
    }

    if (auto channel = hub_service_.getChannel(event.channel_id)) {
        if (first_join_in_channel) {
            publish_voice_snapshot(channel->hub_id, event.channel_id,
                                   read_channel_started_at_unix(event.channel_id));
        }
        publish_voice_participant_upsert(channel->hub_id, event.channel_id, event.user_id,
                                         final_muted, final_deafened);
    }

    publish_self_status(event.user_id, true, intent.session_id, event.channel_id);

    if (pending_opt.has_value()) {
        if (intent.has_from_channel && intent.from_channel != intent.to_channel) {
            intent.new_join_seen = true;
            if (intent.old_leave_seen) {
                clear_pending_join_intent(event.user_id);
            } else {
                (void)update_pending_join_intent(event.user_id, intent);
            }
        } else {
            clear_pending_join_intent(event.user_id);
        }
    }
}

void VoiceService::handle_participant_left(const livekit::webhook::LiveKitEvent& event) {
    const auto pending_opt = read_pending_join_intent(event.user_id);

    const auto current_channel = sessions_.user_channel(event.user_id);
    const auto current_owner = sessions_.user_session(event.user_id);

    const bool matches_owner_session =
        event.app_session_id == 0 ||
        (current_owner.has_value() && *current_owner == event.app_session_id);

    const bool nonce_mismatch_to_pending =
        pending_opt.has_value() && pending_opt->to_channel == event.channel_id &&
        !pending_opt->intent_nonce.empty() && !event.intent_nonce.empty() &&
        event.intent_nonce != pending_opt->intent_nonce;

    const bool leaving_current_voice = current_channel.has_value() &&
                                       *current_channel == event.channel_id &&
                                       matches_owner_session && !nonce_mismatch_to_pending;

    const bool old_leave_for_switch = pending_opt.has_value() && pending_opt->has_from_channel &&
                                      pending_opt->from_channel == event.channel_id &&
                                      pending_opt->from_channel != pending_opt->to_channel;

    if (leaving_current_voice || old_leave_for_switch) {
        clear_participant_remote_missing_confirmation(event.channel_id, event.user_id);
    }

    bool became_empty = false;
    if (leaving_current_voice || old_leave_for_switch) {
        became_empty = sessions_.leave(event.channel_id, event.user_id);
    }

    if (leaving_current_voice) {
        utils::EventLogger::instance().voice_leave(event.user_id.value, event.channel_id.value);

        if (auto channel = hub_service_.getChannel(event.channel_id)) {
            publish_voice_participant_remove(channel->hub_id, event.channel_id, event.user_id);
        }

        publish_self_status(event.user_id, false, std::nullopt, std::nullopt);

        if (!old_leave_for_switch) {
            clear_resume_id(event.user_id);
        }

        if (became_empty) {
            clear_channel_started_at_unix(event.channel_id);
            on_channel_empty(event.channel_id);
        }

        if (old_leave_for_switch) {
            auto intent = *pending_opt;
            intent.old_leave_seen = true;
            if (intent.new_join_seen) {
                clear_pending_join_intent(event.user_id);
            } else {
                (void)update_pending_join_intent(event.user_id, intent);
            }
        }
        return;
    }

    if (old_leave_for_switch) {
        if (auto channel = hub_service_.getChannel(event.channel_id)) {
            publish_voice_participant_remove(channel->hub_id, event.channel_id, event.user_id);
        }

        if (sessions_.is_empty(event.channel_id)) {
            clear_channel_started_at_unix(event.channel_id);
            on_channel_empty(event.channel_id);
        }

        auto intent = *pending_opt;
        intent.old_leave_seen = true;
        if (intent.new_join_seen) {
            clear_pending_join_intent(event.user_id);
        } else {
            (void)update_pending_join_intent(event.user_id, intent);
        }
        return;
    }

    utils::EventLogger::instance().log(
        utils::EventCategory::VOICE, event.user_id.value, "ignored_participant_left", 0,
        "channel=" + event.channel_id.value +
            " event_session=" + std::to_string(event.app_session_id));
    request_channel_reconcile(event.channel_id, "ignored_participant_left");
}

void VoiceService::recover_from_restart() {
    for (const auto& channel : sessions_.active_channels()) {
        sessions_.clear_channel(channel);
        nodes_.clear_room(channel);
    }
    {
        std::lock_guard lock(channel_started_mutex_);
        channel_started_at_unix_.clear();
    }
    {
        std::lock_guard lock(event_order_mutex_);
        user_last_event_ts_ms_.clear();
        channel_last_room_event_ts_ms_.clear();
    }
    {
        std::lock_guard lock(resume_id_mutex_);
        user_resume_ids_.clear();
    }
    {
        std::lock_guard lock(e2ee_rekey_mutex_);
        e2ee_rekey_guard_until_.clear();
    }
    remote_missing_evidence_.reset_all();

    std::unordered_set<ChannelId> recovered_rooms;
    std::unordered_map<UserId, ChannelId> recovered_users;
    std::size_t recovered_participant_count = 0;
    const uint64_t recovered_started_at = unix_now_seconds();

    const auto nodes = nodes_.list_nodes();

    for (const auto& node : nodes) {
        livekit::cli::LivekitClient client(node.private_host, token_service_);

        std::vector<ChannelId> rooms;
        try {
            rooms = client.ListRooms();
        } catch (const std::exception& ex) {
            utils::EventLogger::instance().log(utils::EventCategory::VOICE, "",
                                               "recovery_list_rooms_failed", 0,
                                               "node=" + node.node_id + " error=" + ex.what());
            continue;
        }

        for (const auto& room : rooms) {
            if (recovered_rooms.insert(room).second) {
                nodes_.bind_room(room, node.node_id);
                nodes_.increment_room(room, node.node_id);
                set_channel_started_at_unix(room, recovered_started_at);
            }

            std::vector<livekit::cli::ParticipantInfo> participants;
            try {
                participants = client.ListParticipants(room);
            } catch (const std::exception& ex) {
                utils::EventLogger::instance().log(utils::EventCategory::VOICE, "",
                                                   "recovery_list_participants_failed", 0,
                                                   "room=" + room.value + " error=" + ex.what());
                continue;
            }

            std::unordered_map<UserId, livekit::cli::ParticipantInfo> participants_by_user;
            for (const auto& participant : participants) {
                if (participant.identity.value.empty()) continue;
                participants_by_user.emplace(participant.identity, participant);
            }
            const auto metadata_node_id =
                resolve_participant_node_assignment(room, participants_by_user, "recovery");
            if (metadata_node_id.has_value()) {
                const auto previous_node = nodes_.get_room_node(room);
                if (!previous_node || previous_node->node_id != *metadata_node_id) {
                    utils::EventLogger::instance().log(
                        utils::EventCategory::VOICE, "", "recovery_room_assignment_repaired", 0,
                        "channel=" + room.value + " previous_node=" +
                            (previous_node ? previous_node->node_id : std::string("unknown")) +
                            " assigned_node=" + *metadata_node_id + " source=participant_metadata");
                }
                nodes_.bind_room(room, *metadata_node_id);
                nodes_.increment_room(room, *metadata_node_id);
            }

            if (!participants.empty()) {
                const auto recovered_key = load_channel_e2ee_key_from_storage(room);
                if (!recovered_key.has_value()) {
                    utils::EventLogger::instance().log(
                        utils::EventCategory::VOICE, "", "e2ee_key_missing_active_room", 0,
                        "channel=" + room.value + " reason=recovery_missing_or_invalid");
                    force_channel_rekey(room, "recovery_missing_or_invalid");
                    continue;
                }

                e2ee_keys_.set_key(room, *recovered_key);
                (void)persist_channel_e2ee_key_to_storage(room, *recovered_key);
                clear_channel_rekey_guard(room);
                utils::EventLogger::instance().log(
                    utils::EventCategory::VOICE, "", "e2ee_key_loaded", 0,
                    "channel=" + room.value + " source=redis_recovery");
            } else {
                clear_channel_rekey_guard(room);
            }

            for (const auto& p : participants) {
                if (p.identity.value.empty()) {
                    continue;
                }

                const auto [it, inserted] = recovered_users.emplace(p.identity, room);
                if (!inserted) {
                    if (it->second != room) {
                        utils::EventLogger::instance().log(
                            utils::EventCategory::VOICE, p.identity.value,
                            "recovery_user_multiple_channels", 0,
                            "first_room=" + it->second.value + " second_room=" + room.value);
                    }
                    continue;
                }

                const auto metadata = parse_participant_metadata(p.metadata);
                const SessionId recovered_session =
                    metadata.app_session_id != 0 ? static_cast<SessionId>(metadata.app_session_id)
                                                 : next_recovery_session_id();

                const bool first_join_in_channel =
                    sessions_.join(room, p.identity, recovered_session);
                (void)sessions_.set_muted(p.identity, false);
                (void)sessions_.set_deafened(p.identity, false);

                if (first_join_in_channel) {
                    set_channel_started_at_unix(room, recovered_started_at);
                }

                const std::string effective_node_id =
                    !metadata.node_id.empty() ? metadata.node_id : node.node_id;
                if (!effective_node_id.empty()) {
                    nodes_.increment_user(effective_node_id);
                }

                if (!read_resume_id(p.identity).has_value()) {
                    if (!load_resume_id_from_storage(p.identity).has_value()) {
                        (void)rotate_resume_id(p.identity);
                    }
                }

                recovered_participant_count++;
            }
        }
    }

    utils::EventLogger::instance().log(
        utils::EventCategory::VOICE, "", "recovery_completed", 0,
        "rooms=" + std::to_string(recovered_rooms.size()) +
            " participants=" + std::to_string(recovered_participant_count));
}

void VoiceService::on_livekit_event(const livekit::webhook::LiveKitEvent& event) {
    using Type = livekit::webhook::LiveKitEventType;

    if (!mark_webhook_event_seen(event.event_id)) {
        utils::EventLogger::instance().log(utils::EventCategory::VOICE, event.user_id.value,
                                           "duplicate_webhook_event", 0,
                                           "event_id=" + event.event_id);
        return;
    }

    if ((event.type == Type::PARTICIPANT_JOINED || event.type == Type::PARTICIPANT_LEFT ||
         event.type == Type::PARTICIPANT_CONNECTION_ABORTED) &&
        is_stale_participant_event(event)) {
        utils::EventLogger::instance().log(
            utils::EventCategory::VOICE, event.user_id.value, "stale_participant_event", 0,
            "event_id=" + event.event_id + " raw_event=" + event.raw_event_name + " channel=" +
                event.channel_id.value + " timestamp_ms=" + std::to_string(event.timestamp_ms));
        request_channel_reconcile(event.channel_id, "stale_participant_event");
        return;
    }

    if ((event.type == Type::ROOM_STARTED || event.type == Type::ROOM_FINISHED) &&
        is_stale_room_event(event)) {
        utils::EventLogger::instance().log(
            utils::EventCategory::VOICE, "", "stale_room_event", 0,
            "event_id=" + event.event_id + " raw_event=" + event.raw_event_name + " channel=" +
                event.channel_id.value + " timestamp_ms=" + std::to_string(event.timestamp_ms));
        request_channel_reconcile(event.channel_id, "stale_room_event");
        return;
    }

    auto bound_node = nodes_.get_room_node(event.channel_id);
    const std::string current_node_id = bound_node ? bound_node->node_id : "";

    switch (event.type) {
        case Type::ROOM_STARTED:
            if (!current_node_id.empty()) nodes_.increment_room(event.channel_id, current_node_id);
            break;

        case Type::ROOM_FINISHED:
            nodes_.decrement_room(event.channel_id);

            if (consume_channel_takeover_guard(event.channel_id)) {
                utils::EventLogger::instance().log(utils::EventCategory::VOICE, "",
                                                   "room_finished_guarded", 0,
                                                   "channel=" + event.channel_id.value);
                break;
            }

            utils::EventLogger::instance().log(utils::EventCategory::VOICE, "",
                                               "room_finished_requires_reconcile", 0,
                                               "channel=" + event.channel_id.value);
            reset_remote_missing_confirmations(event.channel_id);
            request_channel_reconcile(event.channel_id, "room_finished");
            break;

        case Type::PARTICIPANT_JOINED: {
            const auto before_channel = sessions_.user_channel(event.user_id);
            std::string effective_node_id = current_node_id;
            if (!event.node_id.empty() && current_node_id.empty()) {
                nodes_.bind_room(event.channel_id, event.node_id);
                effective_node_id = event.node_id;
            } else if (!event.node_id.empty() && event.node_id != current_node_id) {
                utils::EventLogger::instance().log(utils::EventCategory::VOICE, event.user_id.value,
                                                   "participant_joined_node_mismatch", 0,
                                                   "event_node=" + event.node_id +
                                                       " current_node=" + current_node_id +
                                                       " channel=" + event.channel_id.value);
            }

            handle_participant_joined(event);

            const auto after_channel = sessions_.user_channel(event.user_id);
            const bool entered_channel =
                after_channel.has_value() && *after_channel == event.channel_id &&
                (!before_channel.has_value() || *before_channel != event.channel_id);
            if (entered_channel) {
                if (!effective_node_id.empty()) {
                    nodes_.increment_user(effective_node_id);
                }
            }
            break;
        }

        case Type::PARTICIPANT_LEFT:
        case Type::PARTICIPANT_CONNECTION_ABORTED: {
            const bool node_mismatch = !event.node_id.empty() && !current_node_id.empty() &&
                                       event.node_id != current_node_id;
            if (node_mismatch) {
                utils::EventLogger::instance().log(utils::EventCategory::VOICE, event.user_id.value,
                                                   "participant_left_node_mismatch", 0,
                                                   "event_node=" + event.node_id +
                                                       " current_node=" + current_node_id +
                                                       " channel=" + event.channel_id.value);
            }

            const auto before_channel = sessions_.user_channel(event.user_id);
            handle_participant_left(event);

            const auto after_channel = sessions_.user_channel(event.user_id);
            const bool left_channel =
                before_channel.has_value() && *before_channel == event.channel_id &&
                (!after_channel.has_value() || *after_channel != event.channel_id);
            if (left_channel) {
                if (!event.node_id.empty()) {
                    nodes_.decrement_user(event.node_id);
                } else if (!current_node_id.empty()) {
                    nodes_.decrement_user(current_node_id);
                }
            }
            if (node_mismatch) {
                request_channel_reconcile(event.channel_id, "participant_left_node_mismatch");
            }
            break;
        }

        case Type::TRACK_PUBLISHED:
        case Type::TRACK_UNPUBLISHED:
        case Type::EGRESS_STARTED:
        case Type::EGRESS_UPDATED:
        case Type::EGRESS_ENDED:
        case Type::INGRESS_STARTED:
        case Type::INGRESS_ENDED: {
            utils::EventLogger::instance().log(
                utils::EventCategory::VOICE, event.user_id.value, "livekit_telemetry_event", 0,
                "event_id=" + event.event_id + " raw_event=" + event.raw_event_name +
                    " channel=" + event.channel_id.value + " track_sid=" + event.track_sid +
                    " egress_id=" + event.egress_id + " ingress_id=" + event.ingress_id);
            break;
        }

        case Type::UNKNOWN:
        default:
            utils::EventLogger::instance().log(utils::EventCategory::VOICE, event.user_id.value,
                                               "unknown_or_unsupported_webhook_event", 0,
                                               "event_id=" + event.event_id +
                                                   " raw_event=" + event.raw_event_name +
                                                   " channel=" + event.channel_id.value);
            break;
    }
}

void VoiceService::on_session_destroyed(const UserId& user) {
    const auto voice_channel = sessions_.user_channel(user);
    if (!voice_channel.has_value()) return;
    request_channel_reconcile(*voice_channel, "session_destroyed");
}

std::string VoiceService::generate_intent_nonce() const { return generate_nonce_hex(); }

std::uint64_t VoiceService::active_voice_user_count() const {
    return sessions_.active_voice_user_count();
}

std::optional<std::string> VoiceService::current_resume_id(const UserId& user) const {
    return read_resume_id(user);
}

std::optional<VoiceService::ResumeTransferResult> VoiceService::try_resume_voice_ownership(
    const UserId& user, const HubId& hub, const ChannelId& channel, std::string_view resume_id,
    SessionId next_owner_session) {
    if (resume_id.empty()) return std::nullopt;

    const auto active_channel = sessions_.user_channel(user);
    if (!active_channel.has_value() || *active_channel != channel) {
        return std::nullopt;
    }

    const auto channel_info = hub_service_.getChannel(*active_channel);
    if (!channel_info || channel_info->hub_id != hub) {
        return std::nullopt;
    }

    const auto owner_session = sessions_.user_session(user);
    if (!owner_session.has_value() || *owner_session == next_owner_session) {
        return std::nullopt;
    }

    const auto current_resume = read_resume_id(user);
    if (!current_resume.has_value() || *current_resume != resume_id) {
        return std::nullopt;
    }

    if (!sessions_.transfer_owner_session(user, *owner_session, next_owner_session)) {
        return std::nullopt;
    }

    ResumeTransferResult result;
    result.previous_owner_session = *owner_session;
    result.channel = *active_channel;
    result.resume_id = rotate_resume_id(user);
    return result;
}

std::string VoiceService::channel_e2ee_key_storage_key(const ChannelId& channel) {
    return "voice:e2ee:channel:" + channel.value;
}

std::optional<std::string> VoiceService::load_channel_e2ee_key_from_storage(
    const ChannelId& channel) {
    if (!e2ee_storage_key_ready_) return std::nullopt;

    try {
        const auto raw = redis_.get(channel_e2ee_key_storage_key(channel));
        if (!raw.has_value() || raw->empty()) return std::nullopt;

        std::string decrypted_key;
        if (!decrypt_key_blob(e2ee_storage_master_key_, *raw, decrypted_key)) {
            utils::EventLogger::instance().log(utils::EventCategory::VOICE, "",
                                               "e2ee_key_decrypt_failed", 0,
                                               "channel=" + channel.value);
            return std::nullopt;
        }

        redis_.setex(channel_e2ee_key_storage_key(channel), e2ee_key_ttl_, *raw);
        return decrypted_key;
    } catch (const std::exception& ex) {
        utils::EventLogger::instance().log(utils::EventCategory::VOICE, "", "e2ee_key_load_failed",
                                           0, "channel=" + channel.value + " error=" + ex.what());
        return std::nullopt;
    }
}

bool VoiceService::persist_channel_e2ee_key_to_storage(const ChannelId& channel,
                                                       std::string_view key) {
    if (!e2ee_storage_key_ready_ || key.empty()) return false;

    try {
        std::string encrypted_blob;
        if (!encrypt_key_blob(e2ee_storage_master_key_, key, encrypted_blob)) {
            utils::EventLogger::instance().log(utils::EventCategory::VOICE, "",
                                               "e2ee_key_encrypt_failed", 0,
                                               "channel=" + channel.value);
            return false;
        }

        redis_.setex(channel_e2ee_key_storage_key(channel), e2ee_key_ttl_, encrypted_blob);
        return true;
    } catch (const std::exception& ex) {
        utils::EventLogger::instance().log(utils::EventCategory::VOICE, "",
                                           "e2ee_key_persist_failed", 0,
                                           "channel=" + channel.value + " error=" + ex.what());
        return false;
    }
}

void VoiceService::clear_channel_e2ee_key_from_storage(const ChannelId& channel) {
    try {
        redis_.del(channel_e2ee_key_storage_key(channel));
    } catch (const std::exception& ex) {
        utils::EventLogger::instance().log(utils::EventCategory::VOICE, "", "e2ee_key_clear_failed",
                                           0, "channel=" + channel.value + " error=" + ex.what());
    }
}

std::optional<bool> VoiceService::is_channel_effectively_empty(const ChannelId& channel) {
    if (!sessions_.is_empty(channel)) return false;

    // Sweep every configured node, not just the bound one: a participant can be live
    // on a different node than the current room binding (stale/repaired binding). If we
    // only checked the bound node we could wrongly conclude "empty" and mint a fresh
    // E2EE key while that participant still holds the old one -> two keys in one channel.
    const auto configured_nodes = nodes_.list_nodes();
    if (configured_nodes.empty()) return true;

    std::size_t successful_queries = 0;
    for (const auto& node : configured_nodes) {
        try {
            livekit::cli::LivekitClient client(node.private_host, token_service_);
            const auto participants = client.ListParticipants(channel);
            ++successful_queries;
            if (!participants.empty()) return false;
        } catch (const std::exception& ex) {
            utils::EventLogger::instance().log(
                utils::EventCategory::VOICE, "", "e2ee_empty_check_failed", 0,
                "channel=" + channel.value + " node=" + node.node_id + " error=" + ex.what());
        }
    }

    // Only conclude "empty" when every node confirmed zero participants. A partial
    // sweep is inconclusive: returning nullopt makes the caller fail safe (force rekey)
    // rather than mint a divergent key for a room that may be live on an unreachable node.
    if (successful_queries != configured_nodes.size()) return std::nullopt;
    return true;
}

void VoiceService::mark_channel_rekey_in_progress(const ChannelId& channel) {
    std::lock_guard lock(e2ee_rekey_mutex_);
    e2ee_rekey_guard_until_[channel] = std::chrono::steady_clock::now() + e2ee_rekey_guard_ttl_;
}

bool VoiceService::is_channel_rekey_in_progress(const ChannelId& channel) {
    std::lock_guard lock(e2ee_rekey_mutex_);
    auto it = e2ee_rekey_guard_until_.find(channel);
    if (it == e2ee_rekey_guard_until_.end()) return false;

    if (it->second <= std::chrono::steady_clock::now()) {
        e2ee_rekey_guard_until_.erase(it);
        return false;
    }

    return true;
}

bool VoiceService::clear_channel_rekey_if_empty(const ChannelId& channel) {
    const auto empty = is_channel_effectively_empty(channel);
    if (!empty.has_value() || !*empty) return false;

    clear_channel_rekey_guard(channel);
    return true;
}

void VoiceService::clear_channel_rekey_guard(const ChannelId& channel) {
    std::lock_guard lock(e2ee_rekey_mutex_);
    e2ee_rekey_guard_until_.erase(channel);
}

void VoiceService::force_channel_rekey(const ChannelId& channel, std::string_view reason) {
    mark_channel_rekey_in_progress(channel);

    e2ee_keys_.clear_key(channel);
    clear_channel_e2ee_key_from_storage(channel);

    auto node = nodes_.get_room_node(channel);
    if (!node) {
        utils::EventLogger::instance().log(
            utils::EventCategory::VOICE, "", "e2ee_forced_rekey", 0,
            "channel=" + channel.value + " removed_participants=0 reason=" + std::string(reason) +
                " node=unknown");
        return;
    }

    livekit::cli::LivekitClient client(node->private_host, token_service_);
    std::vector<livekit::cli::ParticipantInfo> participants;
    try {
        participants = client.ListParticipants(channel);
    } catch (const std::exception& ex) {
        utils::EventLogger::instance().log(
            utils::EventCategory::VOICE, "", "e2ee_forced_rekey_list_failed", 0,
            "channel=" + channel.value + " reason=" + std::string(reason) + " error=" + ex.what());
        return;
    }

    std::size_t removed_count = 0;
    for (const auto& participant : participants) {
        if (participant.identity.value.empty()) continue;
        if (client.RemoveParticipant(channel, participant.identity)) {
            removed_count++;
        }
    }

    utils::EventLogger::instance().log(
        utils::EventCategory::VOICE, "", "e2ee_forced_rekey", 0,
        "channel=" + channel.value + " node=" + node->node_id + " removed_participants=" +
            std::to_string(removed_count) + " reason=" + std::string(reason));
}

std::string VoiceService::pending_join_key(const UserId& user) {
    return "voice:pending_join:" + user.value;
}

std::string VoiceService::webhook_seen_key(const std::string& event_id) {
    return "voice:webhook_seen:" + event_id;
}

std::string VoiceService::resume_key(const UserId& user) { return "voice:resume_id:" + user.value; }

std::string VoiceService::rotate_resume_id(const UserId& user) {
    const auto next = generate_nonce_hex();
    {
        std::lock_guard lock(resume_id_mutex_);
        user_resume_ids_[user] = next;
    }

    try {
        redis_.setex(resume_key(user), kResumeIdTtl, next);
    } catch (const std::exception& ex) {
        utils::EventLogger::instance().log(utils::EventCategory::VOICE, user.value,
                                           "resume_id_persist_failed", 0,
                                           std::string("error=") + ex.what());
    }

    return next;
}

std::optional<std::string> VoiceService::load_resume_id_from_storage(const UserId& user) {
    try {
        const auto stored = redis_.get(resume_key(user));
        if (!stored.has_value() || stored->empty()) return std::nullopt;

        std::lock_guard lock(resume_id_mutex_);
        user_resume_ids_[user] = *stored;
        return stored;
    } catch (const std::exception& ex) {
        utils::EventLogger::instance().log(utils::EventCategory::VOICE, user.value,
                                           "resume_id_load_failed", 0,
                                           std::string("error=") + ex.what());
        return std::nullopt;
    }
}

std::optional<std::string> VoiceService::read_resume_id(const UserId& user) const {
    std::lock_guard lock(resume_id_mutex_);
    auto it = user_resume_ids_.find(user);
    if (it == user_resume_ids_.end()) return std::nullopt;
    return it->second;
}

void VoiceService::clear_resume_id(const UserId& user) {
    {
        std::lock_guard lock(resume_id_mutex_);
        user_resume_ids_.erase(user);
    }

    try {
        redis_.del(resume_key(user));
    } catch (const std::exception& ex) {
        utils::EventLogger::instance().log(utils::EventCategory::VOICE, user.value,
                                           "resume_id_clear_failed", 0,
                                           std::string("error=") + ex.what());
    }
}

}  // namespace app::services::voice

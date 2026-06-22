#include "app/services/voice/ChannelKeyService.h"

#include "livekit/cli/LivekitClient.h"
#include "utils/EnvLoader.h"
#include "utils/EventLogger.h"

#include <openssl/evp.h>
#include <openssl/rand.h>
#include <openssl/sha.h>

#include <array>
#include <vector>

namespace app::services::voice {
namespace {

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

}  // namespace

ChannelKeyService::ChannelKeyService(infra::redis::RedisClient& redis,
                                     livekit::LivekitNodeRegistry& nodes,
                                     livekit::LiveKitTokenService& token_service,
                                     VoiceSessionManager& sessions, VoicePublisher& publisher,
                                     app::services::HubService& hub_service)
    : redis_(redis),
      nodes_(nodes),
      token_service_(token_service),
      sessions_(sessions),
      publisher_(publisher),
      hub_service_(hub_service),
      key_ttl_(parse_positive_seconds(utils::EnvLoader::get_env("VOICE_E2EE_KEY_TTL_SEC", ""),
                                      kDefaultKeyTtl)),
      rekey_guard_ttl_(parse_positive_seconds(
          utils::EnvLoader::get_env("VOICE_E2EE_REKEY_GUARD_SEC", ""), kDefaultRekeyGuardTtl)) {
    const auto storage_secret = utils::EnvLoader::get_env(
        "VOICE_E2EE_STORAGE_SECRET", utils::EnvLoader::get_env("LIVEKIT_API_SECRET", ""));
    storage_key_ready_ = derive_master_key(storage_secret, storage_master_key_);
    if (!storage_key_ready_) {
        utils::EventLogger::instance().log(utils::EventCategory::VOICE, "",
                                           "e2ee_storage_master_key_unavailable", 0,
                                           "reason=missing_or_invalid_secret");
    }
}

std::mutex& ChannelKeyService::channel_rotation_mutex(const ChannelId& channel) {
    std::lock_guard map_lock(rotation_mutex_map_mutex_);
    auto& slot = channel_rotation_mutexes_[channel];
    if (!slot) slot = std::make_unique<std::mutex>();
    return *slot;
}

bool ChannelKeyService::rekey_blocks_join(const ChannelId& channel) {
    return is_rekey_in_progress(channel) && !clear_rekey_if_empty(channel);
}

ChannelKeyService::AcquireResult ChannelKeyService::acquire_for_join(const ChannelId& channel,
                                                                     const UserId& user) {
    AcquireResult result;

    // A genuine new join into a channel that already has tracked members rotates the
    // key: the joiner and existing members converge on a fresh key the joiner never saw
    // used before (backward secrecy), and any key a recent leaver held is replaced.
    // A resume/reconnect into the same channel, or the first person into an empty
    // channel, keeps/creates the current key (no rotation).
    const auto current_channel = sessions_.user_channel(user);
    const bool already_member = current_channel.has_value() && *current_channel == channel;
    const bool other_members_present = !already_member && !sessions_.is_empty(channel);

    std::optional<livekit::E2EEKeyManager::ChannelKey> ck;

    if (other_members_present) {
        ck = rotate_and_broadcast(channel, "participant_join");
    } else {
        ck = e2ee_keys_.get_key(channel);
        if (!ck.has_value()) {
            // Compute emptiness OUTSIDE the rotation lock (it does LiveKit network I/O),
            // then mutate key state under the lock so a concurrent clear_channel cannot
            // interleave with our create (the create-vs-clear race).
            const auto empty_state = is_channel_effectively_empty(channel);
            bool need_force_rekey = false;
            {
                std::lock_guard rotation_lock(channel_rotation_mutex(channel));
                ck = e2ee_keys_.get_key(channel);  // re-check under the lock
                if (ck.has_value()) {
                    (void)persist_to_storage(channel, ck->key, ck->key_index);
                } else if (empty_state.has_value() && *empty_state) {
                    // Confirmed empty (server + LiveKit agree): mint a FRESH key. Never
                    // reuse stored material here — a departed member could otherwise decrypt
                    // the new session if a prior clear's Redis del had failed.
                    try {
                        ck = e2ee_keys_.get_or_create_key(channel);
                    } catch (const std::exception& ex) {
                        utils::EventLogger::instance().log(
                            utils::EventCategory::VOICE, user.value, "e2ee_key_generate_failed", 0,
                            "channel=" + channel.value + " error=" + ex.what());
                    }
                    if (ck.has_value()) {
                        (void)persist_to_storage(channel, ck->key, ck->key_index);
                        utils::EventLogger::instance().log(
                            utils::EventCategory::VOICE, user.value, "e2ee_key_generated", 0,
                            "channel=" + channel.value + " reason=room_effectively_empty");
                    }
                } else if (auto loaded = load_from_storage(channel)) {
                    // Active room (or inconclusive query) with our in-memory key missing:
                    // restore the live key so the ongoing session keeps decrypting.
                    e2ee_keys_.set_key(channel, loaded->key, loaded->key_index);
                    ck = loaded;
                    (void)persist_to_storage(channel, ck->key, ck->key_index);
                    utils::EventLogger::instance().log(utils::EventCategory::VOICE, user.value,
                                                       "e2ee_key_loaded", 0,
                                                       "channel=" + channel.value + " source=redis");
                } else {
                    need_force_rekey = true;
                }
            }
            if (need_force_rekey) {
                utils::EventLogger::instance().log(utils::EventCategory::VOICE, user.value,
                                                   "e2ee_key_missing_active_room", 0,
                                                   "channel=" + channel.value);
                force_rekey(channel, "missing_or_invalid_key_for_active_room");
                result.error_reason = "voice_rekey_in_progress";
                return result;
            }
        } else {
            (void)persist_to_storage(channel, ck->key, ck->key_index);
        }
    }

    if (!ck.has_value() || ck->key.empty()) {
        result.error_reason = "voice_key_unavailable";
        return result;
    }

    result.key = ck;
    return result;
}

livekit::E2EEKeyManager::ChannelKey ChannelKeyService::rotate_and_broadcast(
    const ChannelId& channel, std::string_view reason) {
    std::lock_guard rotation_lock(channel_rotation_mutex(channel));

    livekit::E2EEKeyManager::ChannelKey rotated;
    try {
        rotated = e2ee_keys_.rotate_key(channel);
    } catch (const std::exception& ex) {
        utils::EventLogger::instance().log(
            utils::EventCategory::VOICE, "", "e2ee_rotate_failed", 0,
            "channel=" + channel.value + " reason=" + std::string(reason) + " error=" + ex.what());
        return {};
    }

    (void)persist_to_storage(channel, rotated.key, rotated.key_index);

    if (const auto channel_info = hub_service_.getChannel(channel)) {
        publisher_.publish_voice_key_update(channel_info->hub_id, channel, rotated.key,
                                            rotated.key_index);
    }

    utils::EventLogger::instance().log(
        utils::EventCategory::VOICE, "", "e2ee_key_rotated", 0,
        "channel=" + channel.value + " key_index=" + std::to_string(rotated.key_index) +
            " reason=" + std::string(reason));
    return rotated;
}

void ChannelKeyService::resync_to_user(const UserId& user) {
    const auto channel = sessions_.user_channel(user);
    if (!channel.has_value()) return;

    // Serialize with rotations on this channel so we read a stable key and our send is
    // ordered relative to any concurrent rotation broadcast for it.
    std::lock_guard rotation_lock(channel_rotation_mutex(*channel));

    const auto ck = e2ee_keys_.get_key(*channel);
    if (!ck.has_value()) return;

    const auto channel_info = hub_service_.getChannel(*channel);
    if (!channel_info) return;

    publisher_.publish_voice_key_update_to_user(channel_info->hub_id, *channel, user, ck->key,
                                                ck->key_index);
}

void ChannelKeyService::clear_channel(const ChannelId& channel) {
    {
        // Serialize with key acquisition/rotation so this clear cannot interleave with a
        // concurrent first-join's key create (create-vs-clear race).
        std::lock_guard rotation_lock(channel_rotation_mutex(channel));
        e2ee_keys_.clear_key(channel);
        clear_storage(channel);
    }
    clear_rekey_guard(channel);
}

bool ChannelKeyService::restore_key_for_recovery(const ChannelId& channel) {
    const auto recovered_key = load_from_storage(channel);
    if (!recovered_key.has_value()) {
        utils::EventLogger::instance().log(utils::EventCategory::VOICE, "",
                                           "e2ee_key_missing_active_room", 0,
                                           "channel=" + channel.value +
                                               " reason=recovery_missing_or_invalid");
        force_rekey(channel, "recovery_missing_or_invalid");
        return false;
    }

    e2ee_keys_.set_key(channel, recovered_key->key, recovered_key->key_index);
    (void)persist_to_storage(channel, recovered_key->key, recovered_key->key_index);
    clear_rekey_guard(channel);
    utils::EventLogger::instance().log(utils::EventCategory::VOICE, "", "e2ee_key_loaded", 0,
                                       "channel=" + channel.value + " source=redis_recovery");
    return true;
}

std::string ChannelKeyService::key_storage_key(const ChannelId& channel) {
    return "voice:e2ee:channel:" + channel.value;
}

std::string ChannelKeyService::index_storage_key(const ChannelId& channel) {
    return "voice:e2ee_idx:channel:" + channel.value;
}

std::optional<livekit::E2EEKeyManager::ChannelKey> ChannelKeyService::load_from_storage(
    const ChannelId& channel) {
    if (!storage_key_ready_) return std::nullopt;

    try {
        const auto raw = redis_.get(key_storage_key(channel));
        if (!raw.has_value() || raw->empty()) return std::nullopt;

        std::string decrypted_key;
        if (!decrypt_key_blob(storage_master_key_, *raw, decrypted_key)) {
            utils::EventLogger::instance().log(utils::EventCategory::VOICE, "",
                                               "e2ee_key_decrypt_failed", 0,
                                               "channel=" + channel.value);
            return std::nullopt;
        }

        // The key index is not secret and is stored alongside (plain) so a restart
        // restores the same index the connected clients still hold.
        uint32_t key_index = 0;
        try {
            const auto raw_idx = redis_.get(index_storage_key(channel));
            if (raw_idx.has_value() && !raw_idx->empty()) {
                key_index = static_cast<uint32_t>(std::stoul(*raw_idx)) %
                            livekit::E2EEKeyManager::kKeyringSize;
            }
        } catch (...) {
            key_index = 0;
        }

        redis_.setex(key_storage_key(channel), key_ttl_, *raw);
        redis_.setex(index_storage_key(channel), key_ttl_, std::to_string(key_index));
        return livekit::E2EEKeyManager::ChannelKey{decrypted_key, key_index};
    } catch (const std::exception& ex) {
        utils::EventLogger::instance().log(utils::EventCategory::VOICE, "", "e2ee_key_load_failed",
                                           0, "channel=" + channel.value + " error=" + ex.what());
        return std::nullopt;
    }
}

bool ChannelKeyService::persist_to_storage(const ChannelId& channel, std::string_view key,
                                           uint32_t key_index) {
    if (!storage_key_ready_ || key.empty()) return false;

    try {
        std::string encrypted_blob;
        if (!encrypt_key_blob(storage_master_key_, key, encrypted_blob)) {
            utils::EventLogger::instance().log(utils::EventCategory::VOICE, "",
                                               "e2ee_key_encrypt_failed", 0,
                                               "channel=" + channel.value);
            return false;
        }

        redis_.setex(key_storage_key(channel), key_ttl_, encrypted_blob);
        redis_.setex(index_storage_key(channel), key_ttl_, std::to_string(key_index));
        return true;
    } catch (const std::exception& ex) {
        utils::EventLogger::instance().log(utils::EventCategory::VOICE, "",
                                           "e2ee_key_persist_failed", 0,
                                           "channel=" + channel.value + " error=" + ex.what());
        return false;
    }
}

void ChannelKeyService::clear_storage(const ChannelId& channel) {
    try {
        redis_.del(key_storage_key(channel));
        redis_.del(index_storage_key(channel));
    } catch (const std::exception& ex) {
        utils::EventLogger::instance().log(utils::EventCategory::VOICE, "", "e2ee_key_clear_failed",
                                           0, "channel=" + channel.value + " error=" + ex.what());
    }
}

std::optional<bool> ChannelKeyService::is_channel_effectively_empty(const ChannelId& channel) {
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

void ChannelKeyService::mark_rekey_in_progress(const ChannelId& channel) {
    std::lock_guard lock(rekey_mutex_);
    rekey_guard_until_[channel] = std::chrono::steady_clock::now() + rekey_guard_ttl_;
}

bool ChannelKeyService::is_rekey_in_progress(const ChannelId& channel) {
    std::lock_guard lock(rekey_mutex_);
    auto it = rekey_guard_until_.find(channel);
    if (it == rekey_guard_until_.end()) return false;

    if (it->second <= std::chrono::steady_clock::now()) {
        rekey_guard_until_.erase(it);
        return false;
    }

    return true;
}

bool ChannelKeyService::clear_rekey_if_empty(const ChannelId& channel) {
    const auto empty = is_channel_effectively_empty(channel);
    if (!empty.has_value() || !*empty) return false;

    clear_rekey_guard(channel);
    return true;
}

void ChannelKeyService::clear_rekey_guard(const ChannelId& channel) {
    std::lock_guard lock(rekey_mutex_);
    rekey_guard_until_.erase(channel);
}

void ChannelKeyService::force_rekey(const ChannelId& channel, std::string_view reason) {
    mark_rekey_in_progress(channel);

    e2ee_keys_.clear_key(channel);
    clear_storage(channel);

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

}  // namespace app::services::voice

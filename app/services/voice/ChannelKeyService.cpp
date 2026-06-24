#include "app/services/voice/ChannelKeyService.h"

#include "livekit/cli/LivekitClient.h"
#include "utils/EnvLoader.h"
#include "utils/EventLogger.h"

#include <array>
#include <openssl/evp.h>
#include <openssl/rand.h>
#include <openssl/sha.h>
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
      join_rotate_debounce_ttl_(parse_positive_seconds(
          utils::EnvLoader::get_env("VOICE_E2EE_JOIN_ROTATE_DEBOUNCE_SEC", ""),
          kDefaultJoinRotateDebounceTtl)) {
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
        if (record_join_and_should_debounce_rotation(channel, user)) {
            // This same user already triggered a join rotation moments ago. A client that
            // keeps re-issuing JoinVoiceChannelRequest without ever landing in LiveKit (no
            // participant_joined webhook) would otherwise re-rotate on every retry, churning
            // keys for the members already in the room. The joiner already holds the freshest
            // key, so reuse the current one instead of rotating again.
            // get_key is internally atomic; no persist here — the key was already persisted
            // when it was rotated/created, and an unlocked re-write could race a concurrent
            // rotation's persist and clobber storage with a stale key.
            ck = e2ee_keys_.get_key(channel);
            if (ck.has_value()) {
                utils::EventLogger::instance().log(utils::EventCategory::VOICE, user.value,
                                                   "e2ee_join_rotation_debounced", 0,
                                                   "channel=" + channel.value);
            } else {
                // No live key despite members being present — fall back to a rotation to
                // establish one rather than failing the join.
                ck = rotate_and_broadcast(channel, "participant_join");
            }
        } else {
            ck = rotate_and_broadcast(channel, "participant_join");
        }
    } else {
        ck = e2ee_keys_.get_key(channel);
        if (!ck.has_value()) {
            // Compute emptiness OUTSIDE the rotation lock (it does LiveKit network I/O),
            // then mutate key state under the lock so a concurrent clear_channel cannot
            // interleave with our create (the create-vs-clear race).
            const auto empty_state = is_channel_effectively_empty(channel);
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
                    utils::EventLogger::instance().log(
                        utils::EventCategory::VOICE, user.value, "e2ee_key_loaded", 0,
                        "channel=" + channel.value + " source=redis");
                } else {
                    // Active room (or inconclusive query) with the key gone from BOTH memory
                    // and storage. Instead of kicking everyone (a hard drop), recover
                    // seamlessly: mint a fresh key and broadcast it to the members already in
                    // the channel. They install it via the LiveKit key provider and KEEP their
                    // transport; the joiner receives the same key in their token. No one is
                    // dropped. Broadcasting only to server-tracked members also excludes any
                    // untrusted LiveKit ghost (it never receives the new key). The
                    // restart-recovery path (restore_key_for_recovery) likewise never kicks —
                    // it defers re-keying to the next membership change.
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
                            utils::EventCategory::VOICE, user.value,
                            "e2ee_key_recovered_active_room", 0,
                            "channel=" + channel.value +
                                " key_index=" + std::to_string(ck->key_index));
                        if (const auto channel_info = hub_service_.getChannel(channel)) {
                            publisher_.publish_voice_key_update(channel_info->hub_id, channel,
                                                                ck->key, ck->key_index);
                        } else {
                            // Recovered the key but couldn't fan it out (hub lookup failed):
                            // existing members would silently fail to decrypt. Make it
                            // traceable instead of a silent desync.
                            utils::EventLogger::instance().log(
                                utils::EventCategory::VOICE, user.value,
                                "e2ee_key_broadcast_skipped", 0,
                                "channel=" + channel.value +
                                    " key_index=" + std::to_string(ck->key_index) +
                                    " reason=recovered_active_room cause=channel_lookup_failed");
                        }
                    }
                }
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
    } else {
        // The key rotated but we couldn't resolve the hub to fan it out — members keep the
        // OLD key while the store advances, so they silently fail to decrypt new audio (looks
        // like a drop). Surface it loudly so it is traceable rather than a mystery.
        utils::EventLogger::instance().log(
            utils::EventCategory::VOICE, "", "e2ee_key_broadcast_skipped", 0,
            "channel=" + channel.value + " key_index=" + std::to_string(rotated.key_index) +
                " reason=" + std::string(reason) + " cause=channel_lookup_failed");
    }

    utils::EventLogger::instance().log(utils::EventCategory::VOICE, "", "e2ee_key_rotated", 0,
                                       "channel=" + channel.value +
                                           " key_index=" + std::to_string(rotated.key_index) +
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
    clear_join_rotation_debounce(channel);
}

void ChannelKeyService::restore_key_for_recovery(const ChannelId& channel) {
    const auto recovered_key = load_from_storage(channel);
    if (recovered_key.has_value()) {
        e2ee_keys_.set_key(channel, recovered_key->key, recovered_key->key_index);
        (void)persist_to_storage(channel, recovered_key->key, recovered_key->key_index);
        utils::EventLogger::instance().log(utils::EventCategory::VOICE, "", "e2ee_key_loaded", 0,
                                           "channel=" + channel.value + " source=redis_recovery");
        return;
    }

    // No stored key for a still-active room. Do NOT kick the participants: a control-plane
    // restart does not disconnect them from LiveKit's media plane, so they remain connected
    // holding the key they already have and keep talking to each other — the server losing
    // its copy doesn't break their media. Re-keying here would be actively harmful: clients'
    // WebSocket sessions reconnect at staggered times, so a broadcast would reach some
    // members and not others, splitting the room. Instead leave the key absent and let it be
    // re-established seamlessly on the next membership change — the normal rotate path mints a
    // fresh key (index 0 when none exists) and broadcasts it to all members, who are present
    // together by then. Kicking here dropped healthy users over a server-side bookkeeping gap.
    utils::EventLogger::instance().log(
        utils::EventCategory::VOICE, "", "e2ee_key_recovery_deferred", 0,
        "channel=" + channel.value + " reason=no_stored_key_active_room");
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

bool ChannelKeyService::record_join_and_should_debounce_rotation(const ChannelId& channel,
                                                                 const UserId& user) {
    const auto now = std::chrono::steady_clock::now();
    std::lock_guard lock(join_rotate_mutex_);
    auto& last = join_rotate_seen_[channel][user];
    const bool debounce =
        last.time_since_epoch().count() != 0 && now < last + join_rotate_debounce_ttl_;
    // Always refresh so a continuing retry storm keeps sliding the window forward and stays
    // suppressed; the first (genuine) join records the timestamp without debouncing.
    last = now;
    return debounce;
}

void ChannelKeyService::clear_join_rotation_debounce(const ChannelId& channel) {
    std::lock_guard lock(join_rotate_mutex_);
    join_rotate_seen_.erase(channel);
}

}  // namespace app::services::voice

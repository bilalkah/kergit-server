#include "app/services/livekit/LiveKitTokenService.h"

#include <chrono>
#include <jwt-cpp/jwt.h>
#include <openssl/evp.h>
#include <openssl/hmac.h>
#include <stdexcept>

namespace app::services::livekit {

LiveKitTokenService::LiveKitTokenService(std::string api_key, std::string api_secret)
    : api_key_(std::move(api_key)), api_secret_(std::move(api_secret)) {
    if (api_key_.empty() || api_secret_.empty()) {
        throw std::runtime_error("LiveKit API key/secret must not be empty");
    }
}

std::string LiveKitTokenService::mint_token(const TokenRequest& req) const {
    using clock = std::chrono::system_clock;

    const auto now = clock::now();
    const auto exp = now + req.ttl;

    picojson::object video_claim;
    video_claim["room"] = picojson::value(req.room.value);
    video_claim["roomJoin"] = picojson::value(true);
    video_claim["canPublish"] = picojson::value(req.can_publish);
    video_claim["canSubscribe"] = picojson::value(req.can_subscribe);

    auto token = jwt::create()
                     // header
                     .set_header_claim("kid", jwt::claim(api_key_))

                     // standard claims
                     .set_issuer(api_key_)             // iss
                     .set_subject(req.identity.value)  // sub
                     .set_not_before(now)              // nbf
                     .set_expires_at(exp)              // exp

                     // LiveKit-specific permissions
                     .set_payload_claim("video", jwt::claim(picojson::value(video_claim)));

    return token.sign(jwt::algorithm::hs256{api_secret_});
}

std::string LiveKitTokenService::generate_unique_e2ee_key(const ChannelId& channel) const {
    // Generate a unique E2EE key using HMAC-SHA256
    // Input: "e2ee:" + channel_id + ":" + timestamp_nanoseconds
    // Key: api_secret_
    const auto now = std::chrono::high_resolution_clock::now();
    const auto nanos =
        std::chrono::duration_cast<std::chrono::nanoseconds>(now.time_since_epoch()).count();

    const std::string data = "e2ee:" + channel.value + ":" + std::to_string(nanos);

    unsigned char hmac_result[EVP_MAX_MD_SIZE];
    unsigned int hmac_len = 0;

    HMAC(EVP_sha256(), api_secret_.data(), static_cast<int>(api_secret_.size()),
         reinterpret_cast<const unsigned char*>(data.data()), data.size(), hmac_result, &hmac_len);

    // Take first 32 bytes (HMAC-SHA256 produces exactly 32 bytes)
    std::string raw_key(reinterpret_cast<char*>(hmac_result), 32);

    // Base64 encode using jwt-cpp's base64 utilities
    return jwt::base::encode<jwt::alphabet::base64>(raw_key);
}

std::string LiveKitTokenService::get_or_create_e2ee_key(const ChannelId& channel,
                                                        bool is_channel_empty) {
    std::lock_guard<std::mutex> lock(e2ee_mutex_);

    auto it = e2ee_keys_.find(channel);
    if (it != e2ee_keys_.end()) {
        // Key exists - return it (existing voice session)
        return it->second;
    }

    // No key exists - generate a new unique one
    // (This happens when first user joins OR when channel was empty)
    std::string new_key = generate_unique_e2ee_key(channel);
    e2ee_keys_[channel] = new_key;

    return new_key;
}

void LiveKitTokenService::clear_e2ee_key(const ChannelId& channel) {
    std::lock_guard<std::mutex> lock(e2ee_mutex_);

    auto erased = e2ee_keys_.erase(channel);
    (void)erased;
}

}  // namespace app::services::livekit

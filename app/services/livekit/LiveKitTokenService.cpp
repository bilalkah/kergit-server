#include "app/services/livekit/LiveKitTokenService.h"

#include "utils/Logger.h"

#include <chrono>
#include <jwt-cpp/jwt.h>
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

    utils::log_line(utils::LogLevel::INFO,
                    fmt::format("LIVEKIT signing with key=[{}], secret=[{}]", api_key_, api_secret_));

    picojson::object video_claim;
    video_claim["room"] = picojson::value(req.room.value);
    video_claim["roomJoin"] = picojson::value(true);
    video_claim["canPublish"] = picojson::value(req.can_publish);
    video_claim["canSubscribe"] = picojson::value(req.can_subscribe);

    auto token =
        jwt::create()
            // header
            .set_header_claim("kid", jwt::claim(api_key_))

            // standard claims
            .set_issuer(api_key_)       // iss
            .set_subject(req.identity.value)  // sub
            .set_not_before(now)        // nbf
            .set_expires_at(exp)        // exp

            // LiveKit-specific permissions
            .set_payload_claim("video", jwt::claim(picojson::value(video_claim)));

    return token.sign(jwt::algorithm::hs256{api_secret_});
}

}  // namespace services::livekit

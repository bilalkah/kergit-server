#include "livekit/token/LiveKitTokenService.h"

#include <chrono>
#include <jwt-cpp/jwt.h>
#include <picojson/picojson.h>

namespace livekit {

LiveKitTokenService::LiveKitTokenService(std::string api_key, std::string api_secret)
    : api_key_(std::move(api_key)), api_secret_(std::move(api_secret)) {}

std::string LiveKitTokenService::mint_participant_token(const ParticipantTokenRequest& req) const {
    const auto now = std::chrono::system_clock::now();
    const auto exp = now + req.ttl;

    picojson::object video;
    video["room"] = picojson::value(req.room.value);
    video["roomJoin"] = picojson::value(true);
    video["canPublish"] = picojson::value(req.can_publish);
    video["canSubscribe"] = picojson::value(req.can_subscribe);
    video["canPublishData"] = picojson::value(true);

    return jwt::create()
        .set_issuer(api_key_)
        .set_subject(req.identity.value)
        .set_issued_at(now)
        .set_expires_at(exp)
        .set_payload_claim("metadata", picojson::value(req.node_id))
        .set_payload_claim("video", jwt::claim(picojson::value(video)))
        .sign(jwt::algorithm::hs256{api_secret_});
}

std::string LiveKitTokenService::mint_admin_token(std::chrono::seconds ttl) const {
    const auto now = std::chrono::system_clock::now();
    const auto exp = now + ttl;

    picojson::object video;
    video["roomAdmin"] = picojson::value(true);
    video["roomCreate"] = picojson::value(true);
    video["roomList"] = picojson::value(true);

    return jwt::create()
        .set_issuer(api_key_)
        .set_issued_at(now)
        .set_expires_at(exp)
        .set_payload_claim("video", jwt::claim(picojson::value(video)))
        .sign(jwt::algorithm::hs256{api_secret_});
}

}  // namespace livekit
#include "livekit/webhook/LivekitWebhookServer.h"

#include <gtest/gtest.h>
#include <jwt-cpp/jwt.h>
#include <openssl/evp.h>

#include <memory>
#include <string>
#include <string_view>

namespace livekit::webhook {

class LivekitWebhookServerTestAccess {
   public:
    static LivekitWebhookServer::HandleResult handle_body(LivekitWebhookServer& server,
                                                          std::string_view body,
                                                          std::string_view auth_header) {
        return server.handle_body(body, auth_header);
    }
};

namespace {

std::string sha256_base64(std::string_view body) {
    std::unique_ptr<EVP_MD_CTX, decltype(&EVP_MD_CTX_free)> ctx(EVP_MD_CTX_new(), &EVP_MD_CTX_free);
    if (!ctx) return {};

    unsigned char digest[EVP_MAX_MD_SIZE];
    unsigned int digest_len = 0;
    if (EVP_DigestInit_ex(ctx.get(), EVP_sha256(), nullptr) != 1 ||
        EVP_DigestUpdate(ctx.get(), body.data(), body.size()) != 1 ||
        EVP_DigestFinal_ex(ctx.get(), digest, &digest_len) != 1) {
        return {};
    }

    std::string out;
    out.resize(4 * ((digest_len + 2) / 3));
    const int encoded_len = EVP_EncodeBlock(reinterpret_cast<unsigned char*>(&out[0]), digest,
                                            static_cast<int>(digest_len));
    if (encoded_len <= 0) {
        return {};
    }
    out.resize(static_cast<size_t>(encoded_len));
    return out;
}

std::string make_auth_header(std::string_view api_key, std::string_view api_secret,
                             std::string_view body) {
    const auto hash = sha256_base64(body);
    const auto token =
        jwt::create()
            .set_issuer(std::string(api_key))
            .set_payload_claim("sha256", jwt::claim(hash))
            .sign(jwt::algorithm::hs256{std::string(api_secret)});
    return "Bearer " + token;
}

TEST(LivekitWebhookServerTest, HandleBodyAcceptsExtendedEventTypes) {
    LivekitWebhookServer server;
    server.set_signing_credentials("dev-key", "dev-secret");

    LiveKitEvent captured;
    server.set_callback([&captured](const LiveKitEvent& event) { captured = event; });

    const std::string body =
        R"({"id":"evt_1","event":"track_published","createdAt":1700000000,"room":{"name":"00000000-0000-0000-0000-000000000001"},"participant":{"identity":"00000000-0000-0000-0000-000000000002","sid":"PA_1","metadata":"{\"node_id\":\"livekit-a\",\"app_session_id\":\"123\",\"intent_nonce\":\"abc\"}"},"track":{"sid":"TR_1","type":"audio","source":"microphone"}})";
    const auto auth = make_auth_header("dev-key", "dev-secret", body);

    const auto result = LivekitWebhookServerTestAccess::handle_body(server, body, auth);
    EXPECT_EQ(result, LivekitWebhookServer::HandleResult::OK);

    EXPECT_EQ(captured.type, LiveKitEventType::TRACK_PUBLISHED);
    EXPECT_EQ(captured.raw_event_name, "track_published");
    EXPECT_EQ(captured.channel_id.value, "00000000-0000-0000-0000-000000000001");
    EXPECT_EQ(captured.user_id.value, "00000000-0000-0000-0000-000000000002");
    EXPECT_EQ(captured.track_sid, "TR_1");
    EXPECT_EQ(captured.track_type, "audio");
    EXPECT_EQ(captured.track_source, "microphone");
}

TEST(LivekitWebhookServerTest, HandleBodyRejectsInvalidSignature) {
    LivekitWebhookServer server;
    server.set_signing_credentials("dev-key", "dev-secret");

    const std::string body = R"({"id":"evt_2","event":"participant_joined"})";
    const auto result =
        LivekitWebhookServerTestAccess::handle_body(server, body, "Bearer invalid-token");
    EXPECT_EQ(result, LivekitWebhookServer::HandleResult::UNAUTHORIZED);
}

TEST(LivekitWebhookServerTest, HandleBodyReturnsBadRequestForMalformedPayload) {
    LivekitWebhookServer server;
    server.set_signing_credentials("dev-key", "dev-secret");

    const std::string body = R"({"id":"evt_3","event":"participant_left")";
    const auto auth = make_auth_header("dev-key", "dev-secret", body);
    const auto result = LivekitWebhookServerTestAccess::handle_body(server, body, auth);
    EXPECT_EQ(result, LivekitWebhookServer::HandleResult::BAD_REQUEST);
}

TEST(LivekitWebhookServerTest, ParseLiveKitEventRecognizesTelemetryEvents) {
    EXPECT_EQ(parseLiveKitEvent("track_published"), LiveKitEventType::TRACK_PUBLISHED);
    EXPECT_EQ(parseLiveKitEvent("track_unpublished"), LiveKitEventType::TRACK_UNPUBLISHED);
    EXPECT_EQ(parseLiveKitEvent("egress_started"), LiveKitEventType::EGRESS_STARTED);
    EXPECT_EQ(parseLiveKitEvent("egress_updated"), LiveKitEventType::EGRESS_UPDATED);
    EXPECT_EQ(parseLiveKitEvent("egress_ended"), LiveKitEventType::EGRESS_ENDED);
    EXPECT_EQ(parseLiveKitEvent("ingress_started"), LiveKitEventType::INGRESS_STARTED);
    EXPECT_EQ(parseLiveKitEvent("ingress_ended"), LiveKitEventType::INGRESS_ENDED);
}

}  // namespace
}  // namespace livekit::webhook

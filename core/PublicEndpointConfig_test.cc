#include "core/PublicEndpointConfig.h"

#include "utils/EnvLoader.h"

#include <gtest/gtest.h>

namespace core {
namespace {

TEST(PublicEndpointConfigTest, DerivesPublicEndpointsFromCanonicalOrigins) {
    const auto config =
        PublicEndpointConfig::from_origins("https://app.test", "https://project.supabase.test");

    EXPECT_EQ(config.app_origin(), "https://app.test");
    EXPECT_EQ(config.websocket_origin(), "wss://app.test");
    EXPECT_EQ(config.invite_base_url(), "https://app.test/invite");
    EXPECT_EQ(config.livekit_node_url("node-a"), "https://app.test/livekit/node-a");
    EXPECT_EQ(config.supabase_issuer(), "https://project.supabase.test/auth/v1");
}

TEST(PublicEndpointConfigTest, AcceptsNonDefaultHttpsPort) {
    const auto config =
        PublicEndpointConfig::from_origins("https://app.test:8443", "https://project.test:9443");

    EXPECT_EQ(config.websocket_origin(), "wss://app.test:8443");
}

TEST(PublicEndpointConfigTest, RejectsNonCanonicalOrigins) {
    const auto expect_invalid_app = [](const std::string& origin) {
        EXPECT_THROW(PublicEndpointConfig::from_origins(origin, "https://project.test"),
                     std::runtime_error);
    };

    expect_invalid_app("");
    expect_invalid_app("http://app.test");
    expect_invalid_app("https://app.test/");
    expect_invalid_app("https://user@app.test");
    expect_invalid_app("https://app.test/path");
    expect_invalid_app("https://app.test?query");
    expect_invalid_app("https://app.test#fragment");
    expect_invalid_app("https://app.test:443");
    expect_invalid_app("https://app.test:");
    expect_invalid_app("https://app.test:0");
    expect_invalid_app("https://APP.test");
    expect_invalid_app("https://bad_host.test");
    expect_invalid_app("https://bad..test");
}

TEST(PublicEndpointConfigTest, RejectsRetiredPublicUrlVariables) {
    utils::EnvLoader::clear_env();
    utils::EnvLoader::set_env("WEB_DOMAIN", "https://app.test");
    utils::EnvLoader::set_env("NUXT_PUBLIC_SUPABASE_URL", "https://project.test");
    utils::EnvLoader::set_env("LIVEKIT_NODE1_PUBLIC_URL", "");

    EXPECT_THROW(PublicEndpointConfig::from_env(), std::runtime_error);

    utils::EnvLoader::clear_env();
}

}  // namespace
}  // namespace core

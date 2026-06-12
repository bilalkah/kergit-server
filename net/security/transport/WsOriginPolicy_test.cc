#include "net/security/transport/WsOriginPolicy.h"

#include <gtest/gtest.h>

namespace net::security::transport {
namespace {

TEST(WsOriginPolicyTest, AllowsOnlyConfiguredBrowserOrigin) {
    const auto policy =
        WsOriginPolicy::from_file("config/ws_origin_policy.yaml", "https://app.test");

    EXPECT_TRUE(policy.is_allowed("https://app.test"));
    EXPECT_TRUE(policy.is_allowed("https://APP.TEST/"));
    EXPECT_FALSE(policy.is_allowed("https://other.test"));
    EXPECT_FALSE(policy.is_allowed("https://app.test:443"));
}

TEST(WsOriginPolicyTest, LoadsTrustedProxyCidrsFromFile) {
    const auto policy =
        WsOriginPolicy::from_file("config/ws_origin_policy.yaml", "https://app.test");

    EXPECT_TRUE(policy.is_trusted_proxy("127.0.0.1"));
    EXPECT_TRUE(policy.is_trusted_proxy("10.20.30.40"));
    EXPECT_FALSE(policy.is_trusted_proxy("203.0.113.10"));
}

TEST(WsOriginPolicyTest, RejectsInvalidConfiguredOrigin) {
    EXPECT_THROW(WsOriginPolicy::from_file("config/ws_origin_policy.yaml", "not-an-origin"),
                 std::runtime_error);
}

}  // namespace
}  // namespace net::security::transport

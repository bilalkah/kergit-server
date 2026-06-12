#include "core/LivekitClusterConfig.h"

#include "utils/EnvLoader.h"

#include <gtest/gtest.h>

namespace core {
namespace {

const auto kEndpoints =
    PublicEndpointConfig::from_origins("https://app.test", "https://project.test");

TEST(LivekitClusterConfigTest, ParsesAndDerivesOrderedNodes) {
    const auto config = LivekitClusterConfig::from_json(
        R"([{"id":"node-a","signal_port":7880,"rtc_tcp_port":7881,"rtc_udp_start":50000,"rtc_udp_end":50100,"prometheus_port":6789},{"id":"node-b","signal_port":7890,"rtc_tcp_port":7891,"rtc_udp_start":50101,"rtc_udp_end":50200,"prometheus_port":6790}])",
        kEndpoints);

    ASSERT_EQ(config.nodes().size(), 2U);
    EXPECT_EQ(config.nodes()[0].id, "node-a");
    EXPECT_EQ(config.nodes()[0].private_url, "http://node-a:7880");
    EXPECT_EQ(config.nodes()[0].public_url, "https://app.test/livekit/node-a");
    EXPECT_EQ(config.nodes()[0].caddy_target, "node-a:7880");
    EXPECT_EQ(config.nodes()[0].metrics_target, "node-a:6789");
    EXPECT_EQ(config.nodes()[1].id, "node-b");
}

TEST(LivekitClusterConfigTest, RequiresNodeIpForProduction) {
    EXPECT_THROW(
        LivekitClusterConfig::from_json(
            R"([{"id":"node-a","signal_port":7880,"rtc_tcp_port":7881,"rtc_udp_start":50000,"rtc_udp_end":50100,"prometheus_port":6789}])",
            kEndpoints, true),
        std::runtime_error);

    EXPECT_NO_THROW(LivekitClusterConfig::from_json(
        R"([{"id":"node-a","signal_port":7880,"rtc_tcp_port":7881,"rtc_udp_start":50000,"rtc_udp_end":50100,"prometheus_port":6789,"node_ip":"203.0.113.10"}])",
        kEndpoints, true));
}

TEST(LivekitClusterConfigTest, RejectsInvalidRegistries) {
    const auto expect_invalid = [](std::string_view raw) {
        EXPECT_THROW(LivekitClusterConfig::from_json(raw, kEndpoints), std::runtime_error);
    };

    expect_invalid("");
    expect_invalid("[]");
    expect_invalid(R"([{"id":"Bad_Node"}])");
    expect_invalid(
        R"([{"id":"node-a","signal_port":7880,"rtc_tcp_port":7881,"rtc_udp_start":50000,"rtc_udp_end":50100,"prometheus_port":6789,"extra":1}])");
    expect_invalid(
        R"([{"id":"node-a","signal_port":7880,"rtc_tcp_port":7881,"rtc_udp_start":50000,"rtc_udp_end":50100,"prometheus_port":6789},{"id":"node-a","signal_port":7890,"rtc_tcp_port":7891,"rtc_udp_start":50101,"rtc_udp_end":50200,"prometheus_port":6790}])");
    expect_invalid(
        R"([{"id":"node-a","signal_port":7880,"rtc_tcp_port":7881,"rtc_udp_start":50000,"rtc_udp_end":50100,"prometheus_port":6789},{"id":"node-b","signal_port":7880,"rtc_tcp_port":7891,"rtc_udp_start":50101,"rtc_udp_end":50200,"prometheus_port":6790}])");
    expect_invalid(
        R"([{"id":"node-a","signal_port":7880,"rtc_tcp_port":7881,"rtc_udp_start":50000,"rtc_udp_end":50100,"prometheus_port":6789},{"id":"node-ab","signal_port":7890,"rtc_tcp_port":7891,"rtc_udp_start":50101,"rtc_udp_end":50200,"prometheus_port":6790}])");
    expect_invalid(
        R"([{"id":"redis-node","signal_port":7880,"rtc_tcp_port":7881,"rtc_udp_start":50000,"rtc_udp_end":50100,"prometheus_port":6789}])");
    expect_invalid(
        R"([{"id":"server-node","signal_port":7880,"rtc_tcp_port":7881,"rtc_udp_start":50000,"rtc_udp_end":50100,"prometheus_port":6789}])");
    expect_invalid(
        R"([{"id":"node-a","signal_port":9001,"rtc_tcp_port":7881,"rtc_udp_start":50000,"rtc_udp_end":50100,"prometheus_port":6789}])");
    expect_invalid(
        R"([{"id":"node-a","signal_port":7880,"rtc_tcp_port":7881,"rtc_udp_start":50000,"rtc_udp_end":50100,"prometheus_port":6789},{"id":"node-b","signal_port":7890,"rtc_tcp_port":7891,"rtc_udp_start":50050,"rtc_udp_end":50200,"prometheus_port":6790}])");
}

TEST(LivekitClusterConfigTest, RejectsRetiredNodeVariables) {
    utils::EnvLoader::clear_env();
    utils::EnvLoader::set_env(
        "LIVEKIT_NODES",
        R"([{"id":"node-a","signal_port":7880,"rtc_tcp_port":7881,"rtc_udp_start":50000,"rtc_udp_end":50100,"prometheus_port":6789}])");
    utils::EnvLoader::set_env("LIVEKIT_NODE1_PRIVATE_URL", "");

    EXPECT_THROW(LivekitClusterConfig::from_env(kEndpoints), std::runtime_error);
    utils::EnvLoader::clear_env();
}

TEST(LivekitClusterConfigTest, FromEnvRequiresNodeIpInProductionMode) {
    utils::EnvLoader::clear_env();
    utils::EnvLoader::set_env(
        "LIVEKIT_NODES",
        R"([{"id":"node-a","signal_port":7880,"rtc_tcp_port":7881,"rtc_udp_start":50000,"rtc_udp_end":50100,"prometheus_port":6789}])");
    utils::EnvLoader::set_env("LIVEKIT_PRODUCTION_MODE", "1");

    EXPECT_THROW(LivekitClusterConfig::from_env(kEndpoints), std::runtime_error);
    utils::EnvLoader::clear_env();
}

}  // namespace
}  // namespace core

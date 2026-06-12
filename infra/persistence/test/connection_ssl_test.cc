#include <gtest/gtest.h>
#include <pqxx/pqxx>

#include <iostream>
#include <string>

#include "core/ServerConfig.h"
#include "infra/persistence/PersistenceGateway.h"
#include "utils/EnvLoader.h"

class ConnectionSslTest : public ::testing::Test {
   protected:
    void SetUp() override { utils::EnvLoader::load_env_file(".env"); }

    bool IsConfigured() {
        const auto host = utils::EnvLoader::get_env("DB_HOST", "");
        const auto name = utils::EnvLoader::get_env("DB_NAME", "");
        const auto user = utils::EnvLoader::get_env("DB_USER", "");
        const auto password = utils::EnvLoader::get_env("DB_PASSWORD", "");
        const auto ssl = utils::EnvLoader::get<bool>("DB_SSL", false);

        return !host.empty() && !name.empty() && !user.empty() && !password.empty() && ssl;
    }

    core::DataBaseConfig build_config() {
        core::DataBaseConfig config;
        config.engine = utils::EnvLoader::get_env("DB_ENGINE", "postgresql");
        config.host = utils::EnvLoader::get_env("DB_HOST", "localhost");
        config.port = utils::EnvLoader::get<uint16_t>("DB_PORT", 5432);
        config.db_name = utils::EnvLoader::get_env("DB_NAME", "postgres");
        config.user = utils::EnvLoader::get_env("DB_USER", "postgres");
        config.password = utils::EnvLoader::get_env("DB_PASSWORD", "");
        config.ssl = utils::EnvLoader::get<bool>("DB_SSL", false);
        config.read_pool_size = 1;
        config.write_pool_size = 1;
        return config;
    }
};

TEST_F(ConnectionSslTest, ConnectWithSsl) {
    if (!IsConfigured()) {
        GTEST_SKIP() << "Requires DB_HOST, DB_NAME, DB_USER, DB_PASSWORD, and DB_SSL=1 in local "
                        ".env or the process environment";
    }
    auto config = build_config();
    std::string conninfo = config.to_connection_string();
    std::cerr << "[TEST] conninfo: " << conninfo << std::endl;

    try {
        pqxx::connection conn{conninfo};
        ASSERT_TRUE(conn.is_open()) << "Connection is not open";

        pqxx::work tx{conn};
        auto result = tx.exec("SELECT 1 AS ok");
        tx.commit();
        EXPECT_EQ(result[0]["ok"].as<int>(), 1);

        pqxx::nontransaction ntx{conn};
        auto ssl_result = ntx.exec("SHOW ssl");
        std::cerr << "[TEST] SSL status: " << ssl_result[0][0].c_str() << std::endl;

    } catch (const pqxx::broken_connection& e) {
        FAIL() << "pqxx::broken_connection: " << e.what();
    } catch (const std::exception& e) {
        FAIL() << "std::exception: " << e.what();
    }
}

TEST_F(ConnectionSslTest, PersistenceGatewayWithSsl) {
    if (!IsConfigured()) {
        GTEST_SKIP() << "Requires DB_HOST, DB_NAME, DB_USER, DB_PASSWORD, and DB_SSL=1 in local "
                        ".env or the process environment";
    }
    auto config = build_config();

    try {
        PersistenceGateway gateway(config);

        (void)gateway.users();
        (void)gateway.hubs();
        std::cerr << "[TEST] PersistenceGateway created, all repos accessible" << std::endl;

    } catch (const pqxx::broken_connection& e) {
        FAIL() << "pqxx::broken_connection: " << e.what();
    } catch (const std::exception& e) {
        FAIL() << "std::exception: " << e.what();
    }
}

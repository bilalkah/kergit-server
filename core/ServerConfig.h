#ifndef CORE_SERVERCONFIG_H
#define CORE_SERVERCONFIG_H

#include "utils/EnvLoader.h"
#include "utils/TlsConfig.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <thread>

namespace core {

struct DataBaseConfig {
    std::string url{};

    std::string engine;
    std::string user;
    std::string password;
    std::string host;
    uint16_t port;
    std::string db_name;
    bool ssl{false};
    std::size_t pool_size{1};
    std::size_t read_pool_size{0};
    std::size_t write_pool_size{0};
    std::string to_connection_string() const {
        if (!url.empty()) return url;

        if (engine == "postgres" || engine == "postgresql") {
            std::string u = "postgresql://" + user + ":" + password + "@" + host + ":" +
                            std::to_string(port) + "/" + db_name;
            if (ssl) u += "?sslmode=require";
            return u;
        }
        throw std::runtime_error("Unsupported DB engine: " + engine);
    }
};

struct NetworkStackConfig {
    std::string host{"0.0.0.0"};
    uint16_t port{9001};
    std::string ws_path{"/*"};
    utils::TlsConfig tls{};
    std::string net_stack_name{"default_netstack"};

    size_t socket_threads{2};
    std::size_t outbound_queue_capacity{50000};
};

struct AppStackConfig {
    std::string app_stack_name{"default_appstack"};
    std::string environment{"development"};
    std::string log_level{"info"};
    size_t worker_threads{3};
    std::size_t event_queue_capacity{30000};
};

struct ControlPlaneConfig {
    std::string host{"127.0.0.1"};
    uint16_t port{8081};
};

struct ServerConfig {
    NetworkStackConfig network{};
    DataBaseConfig database{};
    AppStackConfig app_stack{};
    ControlPlaneConfig control{};
};

class ServerConfigFiller {
   public:
    static void fill_from_env(ServerConfig& cfg) {
        cfg.network.host = utils::EnvLoader::get_env("SERVER_HOST", cfg.network.host);
        cfg.network.port = std::stoi(utils::EnvLoader::get_env("SERVER_PORT", "9001"));
        cfg.network.ws_path = utils::EnvLoader::get_env("SOCKET_PATTERN", cfg.network.ws_path);
        cfg.network.outbound_queue_capacity = static_cast<std::size_t>(std::stoul(
            utils::EnvLoader::get_env("OUTBOUND_QUEUE_CAPACITY",
                                      std::to_string(cfg.network.outbound_queue_capacity))));
        cfg.database.engine = utils::EnvLoader::get_env("DB_ENGINE", "postgresql");
        cfg.database.user = utils::EnvLoader::get_env("DB_USER", "postgres");
        cfg.database.password = utils::EnvLoader::get_env("DB_PASSWORD", "password");
        cfg.database.host = utils::EnvLoader::get_env("DB_HOST", "localhost");
        cfg.database.port = std::stoi(utils::EnvLoader::get_env("DB_PORT", "5432"));
        cfg.database.db_name = utils::EnvLoader::get_env("DB_NAME", "postgres");
        cfg.database.ssl = utils::EnvLoader::get_env("DB_SSL", "false") == "true";
        cfg.database.pool_size =
            static_cast<std::size_t>(std::stoul(utils::EnvLoader::get_env("DB_POOL_SIZE", "3")));
        cfg.database.read_pool_size = static_cast<std::size_t>(std::stoul(
            utils::EnvLoader::get_env("DB_READ_POOL_SIZE",
                                      std::to_string(cfg.database.pool_size))));
        cfg.database.write_pool_size = static_cast<std::size_t>(std::stoul(
            utils::EnvLoader::get_env("DB_WRITE_POOL_SIZE",
                                      std::to_string(cfg.database.pool_size))));
        cfg.app_stack.event_queue_capacity = static_cast<std::size_t>(std::stoul(
            utils::EnvLoader::get_env("EVENT_QUEUE_CAPACITY",
                                      std::to_string(cfg.app_stack.event_queue_capacity))));
        cfg.control.host = utils::EnvLoader::get_env("CONTROL_HOST", cfg.control.host);
        cfg.control.port = std::stoi(utils::EnvLoader::get_env(
            "CONTROL_PORT", std::to_string(cfg.control.port)));
    }
};

};  // namespace core

#endif  // CORE_SERVERCONFIG_H

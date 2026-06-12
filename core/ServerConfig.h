#ifndef CORE_SERVERCONFIG_H
#define CORE_SERVERCONFIG_H

#include "core/LivekitClusterConfig.h"
#include "core/PublicEndpointConfig.h"
#include "utils/EnvLoader.h"
#include "utils/PathResolver.h"
#include "utils/TlsConfig.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <string>
#include <thread>
#include <utility>
#include <vector>

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
            std::string u = "host=" + host + " port=" + std::to_string(port) +
                            " dbname=" + db_name + " user=" + user + " password=" + password;
            if (ssl) {
                const auto root_cert = utils::EnvLoader::get_env(
                    "DB_SSL_ROOT_CERT", "certs/database/root.crt");
                u += " sslmode=verify-full"
                     " sslrootcert=" +
                     utils::resolve_path(root_cert);
            } else {
                u += " sslmode=disable";
            }
            u += " connect_timeout=5";
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
    std::size_t port_index{0};  // Index in socket_ports array, for metrics tracking
    std::string ws_origin_policy_path{"config/ws_origin_policy.yaml"};
    std::string public_origin{};
    std::size_t max_connections{255};
    std::size_t max_message_size{256 * 1024};

    size_t socket_threads{2};
    std::size_t outbound_queue_capacity{50000};
};

struct AppStackConfig {
    std::string app_stack_name{"default_appstack"};
    std::string environment{"development"};
    std::string log_level{"info"};
    size_t worker_threads{3};
    std::size_t max_sessions_per_user{0};
    std::size_t event_queue_capacity{30000};
    std::size_t db_write_queue_capacity{10000};
    std::size_t db_write_max_retries{3};
    std::size_t db_write_retry_ms{25};
};

struct ControlPlaneConfig {
    std::string host{"127.0.0.1"};
    uint16_t port{8081};
};

struct RedisConfig {
    std::string host{"localhost"};
    uint16_t port{6379};
};

struct ServerConfig {
    explicit ServerConfig(PublicEndpointConfig endpoints)
        : public_endpoints(std::move(endpoints)),
          livekit_cluster(LivekitClusterConfig::from_env(public_endpoints)) {
        network.public_origin = public_endpoints.app_origin();
    }

    PublicEndpointConfig public_endpoints;
    LivekitClusterConfig livekit_cluster;
    NetworkStackConfig network{};
    DataBaseConfig database{};
    AppStackConfig app_stack{};
    ControlPlaneConfig control{};
    RedisConfig redis{};
    std::vector<uint16_t> socket_ports{};
};

class ServerConfigFiller {
   public:
    static void fill_from_env(ServerConfig& cfg) {
        cfg.network.host = utils::EnvLoader::get_env("LISTEN_HOST", cfg.network.host);
        auto socket_ports = utils::EnvLoader::get_env_list("SOCKET_PORT");
        if (!socket_ports.empty()) {
            cfg.socket_ports.clear();
            cfg.socket_ports.reserve(socket_ports.size());
            for (const auto& port_str : socket_ports) {
                cfg.socket_ports.push_back(static_cast<uint16_t>(std::stoi(port_str)));
            }
        } else {
            cfg.network.port = utils::EnvLoader::get<uint16_t>("SOCKET_PORT", cfg.network.port);
        }
        cfg.network.ws_path = utils::EnvLoader::get_env("SOCKET_PATTERN", cfg.network.ws_path);
        cfg.network.ws_origin_policy_path =
            utils::EnvLoader::get_env("WS_ORIGIN_POLICY_PATH", cfg.network.ws_origin_policy_path);
        cfg.network.max_connections = std::max<std::size_t>(
            std::size_t{1},
            utils::EnvLoader::get<std::size_t>("MAX_CONNECTIONS", cfg.network.max_connections));
        cfg.network.max_message_size = std::max<std::size_t>(
            std::size_t{1},
            utils::EnvLoader::get<std::size_t>("MAX_MESSAGE_SIZE", cfg.network.max_message_size));
        cfg.network.outbound_queue_capacity = utils::EnvLoader::get<std::size_t>(
            "OUTBOUND_QUEUE_CAPACITY", cfg.network.outbound_queue_capacity);
        cfg.database.engine = utils::EnvLoader::get_env("DB_ENGINE", "postgresql");
        cfg.database.user = utils::EnvLoader::get_env("DB_USER", "postgres");
        cfg.database.password = utils::EnvLoader::get_env("DB_PASSWORD", "password");
        cfg.database.host = utils::EnvLoader::get_env("DB_HOST", "localhost");
        cfg.database.port = utils::EnvLoader::get<uint16_t>("DB_PORT", 5432);
        cfg.database.db_name = utils::EnvLoader::get_env("DB_NAME", "postgres");
        cfg.database.ssl = utils::EnvLoader::get<bool>("DB_SSL", cfg.database.ssl);
        cfg.database.pool_size = utils::EnvLoader::get<std::size_t>("DB_POOL_SIZE", 3);
        cfg.database.read_pool_size =
            utils::EnvLoader::get<std::size_t>("DB_READ_POOL_SIZE", cfg.database.pool_size);
        cfg.database.write_pool_size =
            utils::EnvLoader::get<std::size_t>("DB_WRITE_POOL_SIZE", cfg.database.pool_size);
        cfg.app_stack.worker_threads =
            utils::EnvLoader::get<std::size_t>("WORKER_THREADS", cfg.app_stack.worker_threads);
        cfg.app_stack.max_sessions_per_user = utils::EnvLoader::get<std::size_t>(
            "MAX_SESSIONS_PER_USER", cfg.app_stack.max_sessions_per_user);
        cfg.app_stack.event_queue_capacity = utils::EnvLoader::get<std::size_t>(
            "EVENT_QUEUE_CAPACITY", cfg.app_stack.event_queue_capacity);
        cfg.app_stack.db_write_queue_capacity = utils::EnvLoader::get<std::size_t>(
            "DB_WRITE_QUEUE_CAPACITY", cfg.app_stack.db_write_queue_capacity);
        cfg.app_stack.db_write_max_retries = utils::EnvLoader::get<std::size_t>(
            "DB_WRITE_MAX_RETRIES", cfg.app_stack.db_write_max_retries);
        cfg.app_stack.db_write_retry_ms = utils::EnvLoader::get<std::size_t>(
            "DB_WRITE_RETRY_MS", cfg.app_stack.db_write_retry_ms);
        cfg.control.host = utils::EnvLoader::get_env("CONTROL_HOST", cfg.control.host);
        cfg.control.port = utils::EnvLoader::get<uint16_t>("CONTROL_PORT", cfg.control.port);

        cfg.redis.host = utils::EnvLoader::get_env("REDIS_HOST", cfg.redis.host);
        cfg.redis.port = utils::EnvLoader::get<uint16_t>("REDIS_PORT", cfg.redis.port);
    }
};

};  // namespace core

#endif  // CORE_SERVERCONFIG_H

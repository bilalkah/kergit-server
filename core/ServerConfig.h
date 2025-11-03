#ifndef CORE_SERVERCONFIG_H
#define CORE_SERVERCONFIG_H

#include "utils/TlsConfig.h"
#include "utils/EnvLoader.h"

#include <cstdint>
#include <string>

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

struct ServerConfig {
    std::string host{"0.0.0.0"};
    uint16_t port{9001};
    std::string ws_path{"/*"};
    utils::TlsConfig tls{};
    DataBaseConfig database{};

    std::string environment{"development"};
    std::string log_level{"info"};
    bool debug_gateway{true};
    size_t worker_threads{std::thread::hardware_concurrency()};
};

class ServerConfigFiller {
   public:
    static void fill_from_env(ServerConfig& cfg) {
        cfg.host = utils::EnvLoader::get_env("SERVER_HOST", cfg.host);
        cfg.port = std::stoi(utils::EnvLoader::get_env("SERVER_PORT", "9001"));
        cfg.ws_path = utils::EnvLoader::get_env("SOCKET_PATTERN", cfg.ws_path);
        cfg.database.engine = utils::EnvLoader::get_env("DB_ENGINE", "postgresql");
        cfg.database.user = utils::EnvLoader::get_env("DB_USER", "postgres");
        cfg.database.password = utils::EnvLoader::get_env("DB_PASSWORD", "password");
        cfg.database.host = utils::EnvLoader::get_env("DB_HOST", "localhost");
        cfg.database.port = std::stoi(utils::EnvLoader::get_env("DB_PORT", "5432"));
        cfg.database.db_name = utils::EnvLoader::get_env("DB_NAME", "postgres");
        cfg.database.ssl = utils::EnvLoader::get_env("DB_SSL", "false") == "true";
    }
};

};  // namespace core

#endif  // CORE_SERVERCONFIG_H

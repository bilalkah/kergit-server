#ifndef CORE_SERVERCONFIG_H
#define CORE_SERVERCONFIG_H

#include "utils/TlsConfig.h"

#include <cstdint>
#include <string>

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

        if (engine == "sqlite") {
            // e.g., name=":memory:" or "/path/to/file.sqlite"
            return "sqlite://" + name;
        } else if (engine == "postgres" || engine == "postgresql") {
            std::string u = "postgresql://" + user + ":" + password + "@" + host + ":" +
                            std::to_string(port) + "/" + name;
            if (ssl) u += "?sslmode=require";
            return u;
        }
        throw std::runtime_error("Unsupported DB engine: " + engine);
    }
};

struct ServerConfig {
    std::string host{"0.0.0.0"};
    uint16_t port{9001};
    TlsConfig tls{};
    DataBaseConfig database{};

    std::string environment{"development"};
    std::string log_level{"info"};
    int worker_threads{std::thread::hardware_concurrency()};
}

#endif  // CORE_SERVERCONFIG_H

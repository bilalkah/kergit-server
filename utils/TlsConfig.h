#ifndef UTILS_TLS_CONFIG_H
#define UTILS_TLS_CONFIG_H

#include "utils/Loggable.h"

#include <filesystem>
#include <iostream>
#include <string>

namespace utils {

struct TlsConfig : public Loggable {
#ifdef USE_SSL
    bool enabled{true};
    std::string key_path{"certs/key.pem"};
    std::string cert_path{"certs/cert.pem"};
#else
    bool enabled{false};
    std::string key_path;
    std::string cert_path;
#endif
    TlsConfig() = default;

    bool validate_and_log() const {
        using std::filesystem::exists;
        bool ok = true;
        if (!enabled) {
            log(LogLevel::WARN, "SSL mode: OFF");
            return false;
        }
        if (!exists(key_path)) {
            log(LogLevel::ERROR, "Missing key: ", key_path);
            ok = false;
        }
        if (!exists(cert_path)) {
            log(LogLevel::ERROR, "Missing cert: ", cert_path);
            ok = false;
        }
        if (ok) {
            log(LogLevel::WARN, "SSL mode: ON - key=", key_path, " - cert=", cert_path);
        }
        return ok;
    }
};

}  // namespace utils

#endif  // UTILS_TLS_CONFIG_H

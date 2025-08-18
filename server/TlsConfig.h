// server/TlsConfig.h
#pragma once

#include "server/Config.h"

#include <filesystem>
#include <iostream>
#include <string>

struct TlsConfig {
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
            std::cerr << "[SERVER] SSL mode: OFF\n";
            return true;
        }
        if (!exists(key_path)) {
            std::cerr << "[SSL] Missing key:  " << key_path << "\n";
            ok = false;
        }
        if (!exists(cert_path)) {
            std::cerr << "[SSL] Missing cert: " << cert_path << "\n";
            ok = false;
        }
        if (ok) {
            std::cerr << "[SERVER] SSL mode: ON\n"
                      << "         key=" << key_path << "\n"
                      << "         cert=" << cert_path << "\n";
        }
        return ok;
    }
};

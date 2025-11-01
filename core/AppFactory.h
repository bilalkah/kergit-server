// core/AppFactory.h
#pragma once
#include "core/IApp.h"
#include "utils/TlsConfig.h"
#include "core/ServerConfig.h"

#include <memory>

struct AppFactory {
    static std::unique_ptr<IApp> create(const ServerConfig& cfg) {
        if (cfg.tls_enabled) return std::make_unique<TLSApp>(cfg.tls);
        return std::make_unique<PlainApp>();
    }
};
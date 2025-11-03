#ifndef CORE_APPFACTORY_H
#define CORE_APPFACTORY_H

#include "core/IApp.h"
#include "core/PlainApp.h"
#include "core/ServerConfig.h"
#include "core/TlsApp.h"
#include "utils/TlsConfig.h"

#include <memory>

namespace core {

struct AppFactory : public utils::Loggable {
    static std::unique_ptr<IApp> create(const ServerConfig& cfg) {
        if (cfg.tls.validate_and_log()) {
            uWS::SocketContextOptions opts{
                .key_file_name = cfg.tls.key_path.c_str(),
                .cert_file_name = cfg.tls.cert_path.c_str(),
            };
            return std::make_unique<TLSApp>(opts);
        } else {
            return std::make_unique<PlainApp>();
        }
    }
};

}  // namespace core

#endif  // CORE_APPFACTORY_H

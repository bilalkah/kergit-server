#ifndef SERVER_SERVER_H
#define SERVER_SERVER_H

// Config file
#include "core/ServerConfig.h"

// Appstack and network stack
#include "app/AppStack.h"
#include "control/http/HttpServer.h"
#include "net/NetworkRouter.h"
#include "net/NetworkStack.h"

namespace server {

class Server {
   public:
    explicit Server(const core::ServerConfig& config);

    void start();

    void stop();

   private:
    void init_stacks();

    core::ServerConfig config_;

    app::AppStack app_stack_;
    net::NetworkRouter network_router_;
    control::http::HttpServer control_http_;
};

}  // namespace server

#endif  // SERVER_SERVER_H

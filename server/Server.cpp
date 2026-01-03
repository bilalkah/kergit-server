#include "server/Server.h"

namespace server {

Server::Server(const core::ServerConfig& config) : config_(config), app_stack_(config_) {}

void Server::init_stacks() {
    for (size_t i = 0; i < config_.network.socket_threads; ++i) {
        auto net_stack = std::make_unique<net::NetworkStack>(config_.network);
        net_stack->attach_event_sink(app_stack_.event_sink());
        network_router_.register_stack(std::move(net_stack));
    }
    app_stack_.attach_outbound_sink(network_router_);
}

void Server::start() {
    init_stacks();
    app_stack_.bootstrap();
    app_stack_.start();
    network_router_.start_all();
}

void Server::stop() {
    app_stack_.stop();
    network_router_.stop_all();
}

};  // namespace server

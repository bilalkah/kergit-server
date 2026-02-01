#include "server/Server.h"
#include "utils/Metrics.h"

namespace server {

Server::Server(const core::ServerConfig& config)
    : config_(config),
      app_stack_(config_),
      control_http_(config_.control) {}

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
    utils::metrics::start_timeseries();
    network_router_.start_all();
    control_http_.start();
}

void Server::stop() {
    control_http_.stop();
    app_stack_.stop();
    network_router_.stop_all();
    utils::metrics::stop_timeseries();
}

};  // namespace server

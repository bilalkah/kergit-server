#include "net/NetworkStack.h"

namespace net {

NetworkStack::NetworkStack(core::NetworkStackConfig cfg)
    : id_(IdGenerator::next(cfg.net_stack_name)), cfg_(std::move(cfg)) {}

NetworkStack::~NetworkStack() {}

LoopId NetworkStack::loop_id() const {
    if (transport_layer_) {
        auto* loop = transport_layer_->loop_id();
        if (loop) {
            return reinterpret_cast<LoopId>(loop);
        }
    }
    return 0;
}

std::expected<bool, std::string> NetworkStack::start() {
    if (event_sink_ == nullptr) {
        return std::unexpected("Event sink not attached");
    }

    if (running_.exchange(true) || started_.exchange(true))
        return std::unexpected("NetworkStack already started or running");

    log(utils::LogLevel::WARN, "Starting server thread for " + id_.value + " ...");
    server_thread_ = std::jthread(&NetworkStack::run_server, this);

    // Wait for server to start
    auto timeout = std::chrono::seconds(5);
    auto start_time = std::chrono::system_clock::now();
    while (!started_.load()) {
        if (std::chrono::system_clock::now() - start_time > timeout) {
            running_.store(false);
            started_.store(false);
            return std::unexpected("Failed to start server within timeout");
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    if (!running_.load()) {
        started_.store(false);
        return std::unexpected("Server failed to start");
    }
    started_.store(true);
    return true;
}

std::expected<bool, std::string> NetworkStack::stop() {
    log(utils::LogLevel::WARN, "Stop NetworkStack is requested for " + id_.value);

    // Guarantee we only stop once
    bool was_running = running_.exchange(false);
    if (!was_running) {
        log(utils::LogLevel::INFO, "NetworkStack already stopping / stopped.");
        return true;
    }

    if (transport_layer_) {
        transport_layer_->stop();
    }

    server_thread_.request_stop();

    stopped_.store(true);
    log(utils::LogLevel::INFO, "Stop NetworkStack is completed.");
    return true;
}

void NetworkStack::run_server() {
    log(utils::LogLevel::WARN, "Starting run_server");

    wire_components();

    if (!transport_layer_) {
        log(utils::LogLevel::ERROR, "No transport server configured.");
        running_.store(false);
        started_.store(false);
        return;
    }

    transport_layer_->start();  // Program will block here until stop is called

    log(utils::LogLevel::WARN, "Exiting run_server");
    running_.store(false);
    stopped_.store(true);
}

void NetworkStack::wire_components() {
    // Connection registry
    connection_registry_ = std::make_unique<connection::ConnectionRegistery>();

    // Outgoing message queue
    outgoing_queue_ = std::make_unique<outbound::OutgoingQueue>();

    // Transport later
    transport_layer_ = std::make_unique<transport::websocket::TextWSServer>(
        cfg_, *connection_registry_, *outgoing_queue_);

    transport_layer_->set_hooks(
        {.on_open =
             [this](const ConnId& connid) {
                 event_sink_->push(app::queue::Event{.conn_id = GlobalConnId{id_, connid},
                                                     .body = app::queue::ConnectionEvent{}});
             },
         .on_message =
             [this](const ConnId& connid, std::string_view raw) {
                 event_sink_->push(app::queue::Event{
                     .conn_id = GlobalConnId{id_, connid},
                     .body = app::queue::MessageEvent{
                         .payload = app::queue::Payload{.data = std::string(raw)}}});
             },
         .on_close =
             [this](const ConnId& connid, int code, std::string_view reason) {
                 event_sink_->push(
                     app::queue::Event{.conn_id = GlobalConnId{id_, connid},
                                       .body = app::queue::DisconnectionEvent{
                                           .code = code, .reason = std::string(reason)}});
             }});
}
}  // namespace net

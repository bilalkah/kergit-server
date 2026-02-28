#include "net/NetworkStack.h"

using namespace app::queue;

namespace net {
namespace {
constexpr auto timeout = std::chrono::seconds(5);
}

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

net::outbound::IOutboundSink& NetworkStack::outbound_sink() { return *outgoing_queue_; }

void NetworkStack::attach_event_sink(IEventSink& sink) { event_sink_ = &sink; }

NetStackId NetworkStack::id() const { return id_; }

bool NetworkStack::start() {
    if (event_sink_ == nullptr) {
        log(utils::LogLevel::ERROR, "Cannot start NetworkStack without an event sink attached.");
        return false;
    }

    if (started_.exchange(true)) return true;  // already started

    log(utils::LogLevel::WARN, "Starting server thread for stack_id " + id_.value + " ...");
    server_thread_ = std::jthread(&NetworkStack::run_server, this);

    // Wait for server to start
    auto start_time = std::chrono::system_clock::now();
    while (!started_.load()) {
        if (std::chrono::system_clock::now() - start_time > timeout) {
            started_.store(false);
            return false;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    return true;
}

bool NetworkStack::stop() {
    log(utils::LogLevel::WARN, "Stop NetworkStack is requested for stack_id " + id_.value);

    // Guarantee we only stop once
    if (stopped_.load()) {
        log(utils::LogLevel::WARN, "NetworkStack is already stopped for stack_id " + id_.value);
        return true;
    }

    if (transport_layer_) {
        transport_layer_->stop();
    }

    server_thread_.request_stop();

    // Wait for thread to stop
    auto start_time = std::chrono::system_clock::now();
    while (!stopped_.load(std::memory_order_acquire)) {
        if (std::chrono::system_clock::now() - start_time > timeout) {
            log(utils::LogLevel::ERROR, "Timeout waiting for server thread to stop for stack_id " + id_.value);
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    if (server_thread_.joinable()) {
        log(utils::LogLevel::WARN, "Joining server thread for stack_id " + id_.value + "...");
        server_thread_.join();
    }
    started_.store(false, std::memory_order_release);
    log(utils::LogLevel::WARN, "Stop NetworkStack is completed for stack_id " + id_.value);
    return stopped_.load(std::memory_order_acquire);
}

void NetworkStack::run_server() {
    log(utils::LogLevel::WARN, "Starting run_server for stack_id " + id_.value + "...");

    wire_components();

    if (!transport_layer_) {
        log(utils::LogLevel::ERROR, "No transport server configured.");
        started_.store(false);
        return;
    }
    started_.store(true);
    transport_layer_->start();  // Program will block here until stop is called

    log(utils::LogLevel::WARN, "Exiting run_server for stack_id " + id_.value);
    started_.store(false);
    stopped_.store(true);
}

void NetworkStack::wire_components() {
    // Connection registry
    connection_registry_ = std::make_unique<connection::ConnectionRegistery>();

    // Outgoing message queue
    outgoing_queue_ = std::make_unique<outbound::OutgoingQueue>(cfg_.outbound_queue_capacity);

    // Transport later
    auto ws_origin_policy =
        security::transport::WsOriginPolicy::from_file(cfg_.ws_origin_policy_path);
    transport::websocket::WsLimits ws_limits{
        .max_message_bytes = cfg_.max_message_size,
        .max_connections = cfg_.max_connections,
    };
    transport_layer_ = std::make_unique<transport::websocket::TextWSServer>(
        cfg_, *connection_registry_, *outgoing_queue_, std::move(ws_origin_policy),
        std::move(ws_limits));

    transport_layer_->set_hooks(
        {.on_open =
             [this](const ConnId& connid, const UserId& user_id) {
                 event_sink_->push(Event{
                     ConnectionEvent{.conn_id = GlobalConnId{id_, connid}, .user_id = user_id}});
             },
         .on_message =
             [this](const ConnId& connid, sercom::protocol::Envelope&& env) {
                 event_sink_->push(Event{MessageEvent{.conn_id = GlobalConnId{id_, connid},
                                                      .payload = Payload{.env = std::move(env)}}});
             },
         .on_close =
             [this](const ConnId& connid, int code, std::string_view reason) {
                 event_sink_->push(Event{DisconnectionEvent{.conn_id = GlobalConnId{id_, connid},
                                                            .code = code,
                                                            .reason = std::string(reason)}});
            }});
}
}  // namespace net

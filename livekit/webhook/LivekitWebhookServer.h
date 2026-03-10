#ifndef LIVEKIT_WEBHOOK_LivekitWebhookServer_H_
#define LIVEKIT_WEBHOOK_LivekitWebhookServer_H_

#include "App.h"
#include "livekit/webhook/LivekitEvent.h"
#include "utils/Loggable.h"

#include <atomic>
#include <cstdint>
#include <functional>
#include <string>
#include <thread>

namespace livekit::webhook {

using EventCallback = std::function<void(const LiveKitEvent&)>;

struct Config {
    std::string host{"0.0.0.0"};
    uint16_t port{8080};
    std::string path{"/webhook"};
};

class LivekitWebhookServer : public utils::Loggable {
   public:
    LivekitWebhookServer();
    explicit LivekitWebhookServer(Config config);
    ~LivekitWebhookServer();

    void set_callback(EventCallback cb) { callback_ = std::move(cb); }
    void start();
    void stop();

   private:
    Config config_;
    EventCallback callback_;

    std::thread LivekitWebhookServer_thread_;
    std::atomic<bool> running_{false};
    std::atomic<bool> stop_requested_{false};
    std::atomic<uWS::Loop*> loop_{nullptr};
    std::atomic<us_listen_socket_t*> listen_socket_{nullptr};

    void run();
    void handle_body(std::string_view body);
};

}  // namespace livekit::webhook

#endif  // LIVEKIT_WEBHOOK_LivekitWebhookServer_H_
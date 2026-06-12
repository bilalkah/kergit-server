#ifndef CONTROL_HTTP_HTTPSERVER_H
#define CONTROL_HTTP_HTTPSERVER_H

#include "core/ServerConfig.h"
#include "utils/Loggable.h"

#include <atomic>
#include <thread>

namespace control::http {

class HttpServer : public utils::Loggable {
   public:
    explicit HttpServer(core::ControlPlaneConfig cfg);
    ~HttpServer();

    bool start();
    void stop();

   private:
    void run();

    core::ControlPlaneConfig cfg_;
    std::jthread thread_;
    std::atomic<bool> started_{false};
    std::atomic<bool> starting_{false};
    std::atomic<bool> stopped_{false};
    std::atomic<bool> stop_requested_{false};
    std::atomic<void*> loop_{nullptr};
    void* app_{nullptr};
};

}  // namespace control::http

#endif  // CONTROL_HTTP_HTTPSERVER_H

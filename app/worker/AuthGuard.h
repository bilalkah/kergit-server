#ifndef APP_WORKER_AUTHGUARD_H
#define APP_WORKER_AUTHGUARD_H

#include "app/managers/session/SessionManager.h"
#include "net/outbound/IOutBoundSink.h"

#include <chrono>
#include <condition_variable>
#include <mutex>
#include <queue>
#include <thread>

namespace app::worker {

class AuthGuard {
   public:
    AuthGuard(SessionManager& sessions, net::outbound::IOutboundSink& out_sink,
              std::chrono::seconds grace = std::chrono::seconds(10));
    ~AuthGuard();

    void start();
    void stop();
    void schedule(const GlobalConnId& conn, std::chrono::seconds grace_override);

   private:
    struct AuthDeadline {
        std::chrono::steady_clock::time_point at;
        GlobalConnId conn;
    };
    struct DeadlineCmp {
        bool operator()(const AuthDeadline& a, const AuthDeadline& b) const {
            return a.at > b.at;  // min-heap
        }
    };

    SessionManager& sessions_;
    net::outbound::IOutboundSink& out_sink_;
    std::chrono::seconds default_grace_;

    std::priority_queue<AuthDeadline, std::vector<AuthDeadline>, DeadlineCmp> deadlines_;
    std::condition_variable cv_;
    std::mutex mtx_;
    std::jthread thread_;
    bool stop_flag_{false};

    void loop();
};

}  // namespace app::worker

#endif  // APP_WORKER_AUTHGUARD_H

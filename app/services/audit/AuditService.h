#ifndef APP_SERVICES_AUDIT_SERVICE_H
#define APP_SERVICES_AUDIT_SERVICE_H

#include "core/base/ThreadSafeQueue.h"
#include "infra/persistence/repositories/AuditRepository.h"

#include <atomic>
#include <condition_variable>
#include <mutex>
#include <thread>

namespace app::services {

class AuditService {
   public:
    explicit AuditService(AuditRepository& repo);
    ~AuditService();

    AuditService(const AuditService&) = delete;
    AuditService& operator=(const AuditService&) = delete;

    void start();
    void stop();

    // Best-effort async audit log.
    // Must never throw into command/business flow.
    void log(AuditRepository::Event event) noexcept;

   private:
    void run();
    void runPurgeLoop();
    void purgeOldEvents() noexcept;

    AuditRepository& repo_;
    ThreadSafeQueue<AuditRepository::Event> queue_;

    std::atomic<bool> running_{false};

    std::thread worker_;
    std::thread purge_worker_;

    std::mutex purge_mutex_;
    std::condition_variable purge_cv_;

    static constexpr int kPurgeLimit = 1000;
};

}  // namespace app::services
#endif  // APP_SERVICES_AUDIT_SERVICE_H
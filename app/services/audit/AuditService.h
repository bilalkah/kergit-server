#ifndef APP_SERVICES_AUDIT_SERVICE_H
#define APP_SERVICES_AUDIT_SERVICE_H

#include "core/base/ThreadSafeQueue.h"
#include "infra/persistence/repositories/AuditRepository.h"

#include <atomic>
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

    AuditRepository& repo_;
    ThreadSafeQueue<AuditRepository::Event> queue_;

    std::atomic<bool> running_{false};
    std::thread worker_;
};

}  // namespace app::services
#endif  // APP_SERVICES_AUDIT_SERVICE_H
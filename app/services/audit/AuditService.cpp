#include "app/services/audit/AuditService.h"

#include "utils/EventLogger.h"

namespace app::services {

AuditService::AuditService(AuditRepository& repo) : repo_(repo) {}

AuditService::~AuditService() { stop(); }

void AuditService::start() {
    bool expected = false;
    if (!running_.compare_exchange_strong(expected, true)) {
        return;
    }

    worker_ = std::thread([this] { run(); });
}

void AuditService::stop() {
    bool expected = true;
    if (!running_.compare_exchange_strong(expected, false)) {
        return;
    }

    queue_.stop();

    if (worker_.joinable()) {
        worker_.join();
    }
}

void AuditService::log(AuditRepository::Event event) noexcept { queue_.push(std::move(event)); }

void AuditService::run() {
    while (true) {
        auto item = queue_.pop();

        if (!item.has_value()) {
            break;
        }

        try {
            repo_.logEvent(std::move(item.value()));
        } catch (const std::exception& ex) {
            utils::EventLogger::instance().log(utils::EventCategory::SYSTEM, "",
                                               "AUDIT_LOG_WRITE_FAILED", 0, ex.what());
        } catch (...) {
            utils::EventLogger::instance().log(utils::EventCategory::SYSTEM, "",
                                               "AUDIT_LOG_WRITE_FAILED", 0, "unknown error");
        }
    }
}

}  // namespace app::services
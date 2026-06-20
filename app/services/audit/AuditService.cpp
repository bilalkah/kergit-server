#include "app/services/audit/AuditService.h"

#include <chrono>

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
    purge_worker_ = std::thread([this] { runPurgeLoop(); });
}

void AuditService::stop() {
    bool expected = true;
    if (!running_.compare_exchange_strong(expected, false)) {
        return;
    }

    queue_.stop();
    purge_cv_.notify_all();

    if (worker_.joinable()) {
        worker_.join();
    }

    if (purge_worker_.joinable()) {
        purge_worker_.join();
    }
}

void AuditService::log(AuditRepository::Event event) noexcept {
    queue_.push(std::move(event));
}

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

void AuditService::runPurgeLoop() {
    using namespace std::chrono_literals;

    purgeOldEvents();
    
    while (running_.load(std::memory_order_acquire)) {
        std::unique_lock<std::mutex> lock(purge_mutex_);

        const bool stopped = purge_cv_.wait_for(lock, 24h, [this] {
            return !running_.load(std::memory_order_acquire);
        });

        if (stopped) {
            break;
        }

        lock.unlock();
        purgeOldEvents();
    }
}

void AuditService::purgeOldEvents() noexcept {
    try {
        const int deleted = repo_.purgeOldEvents(kPurgeLimit);

        utils::EventLogger::instance().log(
            utils::EventCategory::SYSTEM, "", "AUDIT_LOG_PURGE_DONE", deleted,
            "old audit events purged");
    } catch (const std::exception& ex) {
        utils::EventLogger::instance().log(utils::EventCategory::SYSTEM, "",
                                           "AUDIT_LOG_PURGE_FAILED", 0, ex.what());
    } catch (...) {
        utils::EventLogger::instance().log(utils::EventCategory::SYSTEM, "",
                                           "AUDIT_LOG_PURGE_FAILED", 0, "unknown error");
    }
}

}  // namespace app::services
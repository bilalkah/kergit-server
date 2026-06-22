#include "app/services/voice/VoiceReconciler.h"

#include "utils/EnvLoader.h"
#include "utils/EventLogger.h"

#include <string>

namespace app::services::voice {
namespace {

std::chrono::seconds parse_reconcile_interval_seconds(const std::string& raw,
                                                      std::chrono::seconds fallback) {
    if (raw.empty()) return fallback;
    try {
        const auto parsed = std::stoll(raw);
        if (parsed <= 0) return fallback;
        return std::chrono::seconds(parsed);
    } catch (...) {
        return fallback;
    }
}

std::chrono::seconds parse_positive_seconds(const std::string& raw, std::chrono::seconds fallback) {
    if (raw.empty()) return fallback;
    try {
        const auto parsed = std::stoll(raw);
        if (parsed <= 0) return fallback;
        return std::chrono::seconds(parsed);
    } catch (...) {
        return fallback;
    }
}

}  // namespace

VoiceReconciler::VoiceReconciler(ReconcileTarget& target)
    : target_(target),
      reconcile_interval_(parse_reconcile_interval_seconds(
          utils::EnvLoader::get_env("LIVEKIT_RECONCILE_INTERVAL_SEC", ""),
          kDefaultReconcileInterval)),
      livekit_missing_clear_ttl_(
          parse_positive_seconds(utils::EnvLoader::get_env("LIVEKIT_MISSING_CLEAR_SEC", ""),
                                 kDefaultLivekitMissingClearTtl)),
      remote_missing_evidence_(livekit_missing_clear_ttl_) {}

VoiceReconciler::~VoiceReconciler() { stop(); }

void VoiceReconciler::start() {
    if (running_.exchange(true)) return;
    stop_requested_.store(false, std::memory_order_release);
    try {
        target_.reconcile_full("startup");
    } catch (const std::exception& ex) {
        utils::EventLogger::instance().log(utils::EventCategory::VOICE, "",
                                           "reconcile_startup_failed", 0,
                                           std::string("error=") + ex.what());
    }
    reconcile_thread_ = std::thread(&VoiceReconciler::run_loop, this);
}

void VoiceReconciler::stop() {
    if (!running_.exchange(false)) return;
    stop_requested_.store(true, std::memory_order_release);
    {
        std::lock_guard lock(mutex_);
        pending_channels_.clear();
    }
    cv_.notify_all();
    if (reconcile_thread_.joinable()) {
        reconcile_thread_.join();
    }
}

void VoiceReconciler::request(const ChannelId& channel, std::string_view reason) {
    if (channel.value.empty()) return;

    bool inserted = false;
    {
        std::lock_guard lock(mutex_);
        inserted = pending_channels_.insert(channel).second;
    }

    if (inserted) {
        utils::EventLogger::instance().log(
            utils::EventCategory::VOICE, "", "reconcile_channel_requested", 0,
            "channel=" + channel.value + " reason=" + std::string(reason));
        cv_.notify_one();
    }
}

bool VoiceReconciler::confirm_channel_remote_missing(const ChannelId& channel,
                                                     std::string_view reason) {
    const auto result = remote_missing_evidence_.observe_channel_missing(channel, reason);

    utils::EventLogger::instance().log(
        utils::EventCategory::VOICE, "",
        result.confirmed ? "reconcile_room_missing_remote_confirmed"
                         : "reconcile_room_missing_remote_suspected",
        0,
        "channel=" + channel.value + " reason=" + std::string(reason) + " first_reason=" +
            result.first_reason + " first_seen=" + (result.first_observation ? "1" : "0") +
            " missing_clear_sec=" + std::to_string(livekit_missing_clear_ttl_.count()));
    return result.confirmed;
}

void VoiceReconciler::clear_channel_remote_missing_confirmation(const ChannelId& channel) {
    remote_missing_evidence_.clear_channel_missing(channel);
}

bool VoiceReconciler::confirm_participant_remote_missing(const ChannelId& channel,
                                                         const UserId& user,
                                                         std::string_view reason) {
    const auto result = remote_missing_evidence_.observe_participant_missing(channel, user, reason);

    utils::EventLogger::instance().log(
        utils::EventCategory::VOICE, user.value,
        result.confirmed ? "reconcile_participant_missing_remote_confirmed"
                         : "reconcile_participant_missing_remote_suspected",
        0,
        "channel=" + channel.value + " reason=" + std::string(reason) + " first_reason=" +
            result.first_reason + " first_seen=" + (result.first_observation ? "1" : "0") +
            " missing_clear_sec=" + std::to_string(livekit_missing_clear_ttl_.count()));
    return result.confirmed;
}

void VoiceReconciler::clear_participant_remote_missing_confirmation(const ChannelId& channel,
                                                                    const UserId& user) {
    remote_missing_evidence_.clear_participant_missing(channel, user);
}

void VoiceReconciler::reset_remote_missing_confirmations(const ChannelId& channel) {
    remote_missing_evidence_.reset_channel(channel);
}

std::vector<ChannelId> VoiceReconciler::tracked_missing_channels() const {
    return remote_missing_evidence_.tracked_missing_channels();
}

void VoiceReconciler::run_loop() {
    auto next_full_run = std::chrono::steady_clock::now() + reconcile_interval_;

    while (!stop_requested_.load(std::memory_order_acquire)) {
        std::vector<ChannelId> channels_to_reconcile;
        {
            std::unique_lock lock(mutex_);
            cv_.wait_until(lock, next_full_run, [this]() {
                return stop_requested_.load(std::memory_order_acquire) ||
                       !pending_channels_.empty();
            });

            if (stop_requested_.load(std::memory_order_acquire)) {
                break;
            }

            channels_to_reconcile.assign(pending_channels_.begin(), pending_channels_.end());
            pending_channels_.clear();
        }

        for (const auto& channel : channels_to_reconcile) {
            try {
                target_.reconcile_channel(channel, "on_demand");
            } catch (const std::exception& ex) {
                utils::EventLogger::instance().log(
                    utils::EventCategory::VOICE, "", "reconcile_channel_failed", 0,
                    "channel=" + channel.value + " error=" + ex.what());
            }
        }

        const auto now = std::chrono::steady_clock::now();
        if (now >= next_full_run) {
            try {
                target_.reconcile_full("periodic");
            } catch (const std::exception& ex) {
                utils::EventLogger::instance().log(utils::EventCategory::VOICE, "",
                                                   "reconcile_periodic_failed", 0,
                                                   std::string("error=") + ex.what());
            }
            next_full_run = std::chrono::steady_clock::now() + reconcile_interval_;
        }
    }
}

}  // namespace app::services::voice

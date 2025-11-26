#ifndef APP_SERVICES_HUBPUBLISHER_H
#define APP_SERVICES_HUBPUBLISHER_H

#include "app/queue/OutgoingQueue.h"
#include "core/IApp.h"
#include "domains/Channel.h"
#include "domains/ids/Ids.h"
#include "infra/persistence/PersistenceGateway.h"
#include "net/ConnectionManager.h"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <nlohmann/json.hpp>
#include <string>
#include <thread>
#include <unordered_set>
#include <vector>

namespace app::services {

class PublicIdService;

class HubPublisher {
   public:
    HubPublisher(core::IApp& app, PersistenceGateway& db, net::ConnectionManager& connections,
                 OutgoingQueue& out_queue, PublicIdService& ids,
                 std::chrono::milliseconds interval = std::chrono::milliseconds(15000));
    ~HubPublisher();

    void start();
    void stop();

    // These now act as "mark hub(s) dirty" – they request that these hubs
    // be re-published on the next tick.
    void publish_hub(const HubId& hub_id);
    void publish_hubs(const std::unordered_set<HubId>& hub_ids);

    static std::string topic_for(const HubId& hub_id);

   private:
    // Background worker loop
    void run();

    // Helpers used by the worker thread
    std::unordered_set<HubId> collect_all_hubs() const;
    nlohmann::json collect_online_for_hub(const HubId& hub_id) const;
    std::vector<Channel> load_channels(const HubId& hub_id) const;
    nlohmann::json build_snapshot(const HubId& hub_id, const std::vector<Channel>& channels,
                                  const nlohmann::json& online) const;

    PersistenceGateway& db_;
    net::ConnectionManager& connections_;
    OutgoingQueue& out_queue_;
    PublicIdService& ids_;
    std::chrono::milliseconds interval_;

    // Thread & coordination
    std::thread worker_;
    std::atomic<bool> stop_flag_{false};
    std::mutex mu_;
    std::condition_variable cv_;
    bool running_{false};

    // Hubs scheduled for re-publish
    std::unordered_set<HubId> dirty_hubs_;
};

}  // namespace app::services

#endif  // APP_SERVICES_HUBPUBLISHER_H

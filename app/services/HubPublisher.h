#ifndef APP_SERVICES_HUBPUBLISHER_H
#define APP_SERVICES_HUBPUBLISHER_H

#include "core/IApp.h"
#include "domains/Channel.h"
#include "domains/ids/Ids.h"
#include "infra/persistence/chatdb.h"
#include "net/ClientGateway.h"
#include "net/ConnectionManager.h"

#include <chrono>
#include <string>
#include <unordered_set>
#include <vector>

#include <nlohmann/json.hpp>

struct us_timer_t;

namespace app::services {

class HubPublisher {
   public:
    HubPublisher(core::IApp& app, ChatDB& db, net::ConnectionManager& connections,
                 net::ClientGateway& gateway,
                 std::chrono::milliseconds interval = std::chrono::milliseconds(15000));
    ~HubPublisher();

    void start();
    void stop();

    void publish_hub(const HubId& hub_id);
    void publish_hubs(const std::unordered_set<HubId>& hub_ids);

    static std::string topic_for(const HubId& hub_id);

   private:
    static void on_timer(us_timer_t* timer);
    void tick();

    std::unordered_set<HubId> collect_all_hubs() const;
    nlohmann::json collect_online_for_hub(const HubId& hub_id) const;
    std::vector<Channel> load_channels(const HubId& hub_id) const;
    nlohmann::json build_snapshot(const HubId& hub_id,
                                  const std::vector<Channel>& channels,
                                  const nlohmann::json& online) const;

    core::IApp& app_;
    ChatDB& db_;
    net::ConnectionManager& connections_;
    net::ClientGateway& gateway_;
    std::chrono::milliseconds interval_;
    ::us_timer_t* timer_{nullptr};
    bool running_{false};
};

}  // namespace app::services

#endif  // APP_SERVICES_HUBPUBLISHER_H

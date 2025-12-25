#ifndef APP_ICOMMAND_H
#define APP_ICOMMAND_H

#include "app/queue/OutgoingQueue.h"
#include "domains/ids/Ids.h"
#include "net/PerSocketData.h"
#include "app/services/HubPublisher.h"
#include "app/services/PublicIdService.h"
#include "app/memory/OnMemoryCache.h"
#include "infra/persistence/PersistenceGateway.h"
#include "net/ClientGateway.h"
#include "net/ConnectionManager.h"

#include <chrono>
#include <nlohmann/json.hpp>
#include <string>

namespace app {

namespace {
using namespace std::chrono;
}
struct Input {
    nlohmann::json data;
    system_clock::time_point received_at;
};

struct Output {
    bool success;
    std::string error_code;
    std::string error_message;

    std::vector<OutgoingMessage> messages;
    system_clock::time_point sent_at;
};

struct CommandContext {
    ConnId conn_id;

    ::net::Snapshot snapshot;

    Input input;
    Output output;
};

struct ServiceObjects {
    ServiceObjects(PersistenceGateway& d, net::ClientGateway& g, net::ConnectionManager& c,
                   app::services::HubPublisher& h, app::services::PublicIdService& i,
                   app::memory::ICache& cache)
        : db_(d), gateway_(g), connections_(c), hub_publisher_(h), ids_(i), cache_(cache) {}
    ::PersistenceGateway& db_;
    ::net::ClientGateway& gateway_;
    ::net::ConnectionManager& connections_;
    app::services::HubPublisher& hub_publisher_;
    app::services::PublicIdService& ids_;
    app::memory::ICache& cache_;
};

class ICommand {
   public:
    virtual ~ICommand() = default;
    virtual void execute(CommandContext&) = 0;
};

}  // namespace app

#endif  // APP_ICOMMAND_H

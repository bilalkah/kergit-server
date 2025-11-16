#ifndef APP_SERVICES_USERSERVICES_H
#define APP_SERVICES_USERSERVICES_H

#include "app/services/PublicIdService.h"
#include "domains/Channel.h"
#include "domains/Hub.h"
#include "domains/ids/Ids.h"
#include "infra/persistence/PersistenceGateway.h"
#include "net/ClientGateway.h"
#include "net/ConnectionManager.h"
#include "net/PerSocketData.h"

#include <nlohmann/json.hpp>

namespace app::services {

class UserServices {
   public:
    UserServices(PersistenceGateway& db, net::ClientGateway& gateway,
                 net::ConnectionManager& connections, app::services::PublicIdService& ids)
        : db_(db), gateway_(gateway), connections_(connections), ids_(ids) {}

   private:
    PersistenceGateway& db_;
    net::ClientGateway& gateway_;
    net::ConnectionManager& connections_;
    app::services::PublicIdService& ids_;
};

}  // namespace app::services

#endif  // APP_SERVICES_USERSERVICES_H

#ifndef APP_SERVICES_INVITE_INVITESERVICE_H
#define APP_SERVICES_INVITE_INVITESERVICE_H

#include "domains/ids/Ids.h"
#include "infra/redis/RedisClient.h"

#include <optional>
#include <string>

namespace app::services {

class InviteService {
   public:
    InviteService(infra::redis::RedisClient& redis, const std::string& base_url);

    std::string createInvite(const HubId& hub_id);
    std::optional<HubId> resolveInvite(const std::string& token);

   private:
    static std::string generate_token();

    infra::redis::RedisClient& redis_;
    std::string base_url_;
};

}  // namespace app::services

#endif  // APP_SERVICES_INVITE_INVITESERVICE_H

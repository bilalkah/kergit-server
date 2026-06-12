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

    // Invalidates the hub's active invite (token + reverse lookup), so existing
    // invite links stop working until a new code is created. Used on member kick
    // so links shared before the kick can no longer be used to rejoin.
    void revokeInvitesForHub(const HubId& hub_id);

   private:
    static std::string generate_token();

    infra::redis::RedisClient& redis_;
    std::string base_url_;
};

}  // namespace app::services

#endif  // APP_SERVICES_INVITE_INVITESERVICE_H

#ifndef APP_SERVICES_LIVEKIT_LIVEKITTOKENSERVICE_H_
#define APP_SERVICES_LIVEKIT_LIVEKITTOKENSERVICE_H_

#include "domains/ids/Ids.h"

#include <chrono>
#include <string>

namespace app::services::livekit {

class LiveKitTokenService {
   public:
    struct TokenRequest {
        UserId identity;   // user id
        ChannelId room;    // voice room name
        bool can_publish;  // audio
        bool can_subscribe;
        std::chrono::seconds ttl;
    };

    explicit LiveKitTokenService(std::string api_key, std::string api_secret);

    std::string mint_token(const TokenRequest& req) const;

   private:
    std::string api_key_;
    std::string api_secret_;
};

}  // namespace services::livekit

#endif  // APP_SERVICES_LIVEKIT_LIVEKITTOKENSERVICE_H_

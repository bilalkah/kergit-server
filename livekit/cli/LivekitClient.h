#ifndef LIVEKIT_CLI_LIVEKITCLIENT_H_
#define LIVEKIT_CLI_LIVEKITCLIENT_H_

#include "domains/ids/Ids.h"
#include "livekit/token/LiveKitTokenService.h"

#include <string>
#include <vector>

namespace livekit::cli {

struct ParticipantInfo {
    UserId identity;
    std::string sid;
    bool is_publisher = false;
};

class LivekitClient {
   public:
    LivekitClient(std::string host, const LiveKitTokenService& token_service);

    bool RemoveParticipant(const ChannelId& room, const UserId& identity);

    std::vector<ParticipantInfo> ListParticipants(const ChannelId& room);

    // Returns names of all currently active rooms on this node.
    std::vector<ChannelId> ListRooms();

    void CreateRoom(const std::string& name, const std::string& metadata = "");
    void DeleteRoom(const std::string& name);

   private:
    std::string host_;

    const LiveKitTokenService& token_service_;

    std::string PostJson(const std::string& endpoint, const std::string& body,
                         const std::string& token) const;
};

}  // namespace livekit::cli

#endif
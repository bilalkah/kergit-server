#ifndef INFRA_PERSISTENCE_CHATDB_H
#define INFRA_PERSISTENCE_CHATDB_H
#include "domains/Channel.h"
#include "domains/Hub.h"
#include "domains/Message.h"
#include "domains/ids/Ids.h"

#include <optional>
#include <pqxx/pqxx>
#include <string>
#include <vector>

class ChatDB {
   public:
    explicit ChatDB(const std::string& conninfo);

    // CRUD
    HubId createHub(const std::string& hubName, const UserId& ownerUuid);
    void addMember(const HubId& hubId, const UserId& userUuid,
                   const std::string& role = "member");
    void removeMember(const HubId& hubId, const UserId& userUuid);

    ChannelId createChannel(const HubId& hubId, const std::string& channelName,
                            const std::string& type);
    Message sendMessage(const ChannelId& channelId, const UserId& senderUuid,
                        const std::string& content);

    std::vector<Message> fetchMessages(const ChannelId& channelId, int limit);
    std::vector<Hub> getUserHubs(const UserId& userUuid);
    std::vector<Channel> getHubChannels(const HubId& hubId);
    std::optional<Channel> getChannel(const ChannelId& channelId);
    bool isHubMember(const HubId& hubId, const UserId& userUuid);
    std::optional<Role> getMembershipRole(const HubId& hubId, const UserId& userUuid);

    HubId ensurePersonalHubWithGeneral(const UserId& ownerUuid, const std::string& hubName);

    pqxx::connection& getConnection();

   private:
    pqxx::connection conn_;
};

#endif  // INFRA_PERSISTENCE_CHATDB_H

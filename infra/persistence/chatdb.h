#ifndef INFRA_PERSISTENCE_CHATDB_H
#define INFRA_PERSISTENCE_CHATDB_H
#include "domains/ids/Ids.h"

#include <optional>
#include <pqxx/pqxx>
#include <string>
#include <vector>

struct DbMessage {
    MessageId id{""};
    ChannelId channel_id{""};
    UserId sender_id{""};
    std::string content;
    std::string created_at;
};

struct HubInfo {
    HubId id{""};
    std::string name;
};

struct ChannelInfo {
    ChannelId id{""};
    HubId hub_id{""};
    std::string name;
    std::string type;  // "text" or "voice"
};

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
    void sendMessage(const ChannelId& channelId, const UserId& senderUuid,
                     const std::string& content);

    std::vector<DbMessage> fetchMessages(const ChannelId& channelId, int limit);
    std::vector<HubInfo> getUserHubs(const UserId& userUuid);
    std::vector<ChannelInfo> getHubChannels(const HubId& hubId);

    HubId ensurePersonalHubWithGeneral(const UserId& ownerUuid, const std::string& hubName);

    pqxx::connection& getConnection();

   private:
    pqxx::connection conn_;
};

#endif  // INFRA_PERSISTENCE_CHATDB_H

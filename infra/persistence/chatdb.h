#ifndef INFRA_PERSISTENCE_CHATDB_H
#define INFRA_PERSISTENCE_CHATDB_H
#include "domains/ids/Ids.h"

#include <optional>
#include <pqxx/pqxx>
#include <string>
#include <vector>

struct DbMessage {
    std::string id;
    std::string channel_id;
    std::string sender_id;
    std::string content;
    std::string created_at;
};

struct HubInfo {
    std::string id;
    std::string name;
};

struct ChannelInfo {
    std::string id;
    std::string hub_id;
    std::string name;
    std::string type;  // "text" or "voice"
};

class ChatDB {
   public:
    explicit ChatDB(const std::string& conninfo);

    // CRUD
    std::string createHub(const std::string& hubName, const std::string& ownerUuid);
    void addMember(const std::string& hubId, const std::string& userUuid,
                   const std::string& role = "member");
    void removeMember(const std::string& hubId, const std::string& userUuid);

    std::string createChannel(const std::string& hubId, const std::string& channelName,
                              const std::string& type);
    void sendMessage(const std::string& channelId, const std::string& senderUuid,
                     const std::string& content);

    std::vector<DbMessage> fetchMessages(const std::string& channelId, int limit);
    std::vector<HubInfo> getUserHubs(const UserId& userUuid);
    std::vector<ChannelInfo> getHubChannels(const std::string& hubId);

    std::string ensurePersonalHubWithGeneral(const std::string& ownerUuid,
                                             const std::string& hubName);

    pqxx::connection& getConnection();

   private:
    pqxx::connection conn_;
};

#endif  // INFRA_PERSISTENCE_CHATDB_H

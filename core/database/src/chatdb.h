#ifndef CHATDB_H
#define CHATDB_H

#include <pqxx/pqxx>
#include <string>
#include <vector>
#include <optional>

struct DbMessage {
    int id;
    int channel_id;
    int sender_id;
    std::string content;
    std::string sent_at;
};

struct HubInfo {
    int id;
    std::string name;
};

struct ChannelInfo {
    int id;
    int hub_id;
    std::string name;
    std::string type;
};

class ChatDB {
public:
    explicit ChatDB(const std::string& conninfo);

    int createHub(const std::string& hubName, int ownerId);
    void addMember(int hubId, int userId);
    void removeMember(int hubId, int userId);
    int createChannel(int hubId, const std::string& channelName, const std::string& type);
    void sendMessage(int channelId, int senderId, const std::string& content);
    std::vector<DbMessage> fetchMessages(int channelId, int limit = 50);

    // Provide access to raw connection for advanced queries or tests
    pqxx::connection& getConnection();

    // New: Users
    std::optional<int> findUserIdByUsername(const std::string& username);
    std::optional<std::string> findPasswordHashByUsername(const std::string& username);
    int createUser(const std::string& username, const std::string& passwordHash, const std::string& email);

    // New: Hubs and channels for a user
    std::vector<HubInfo> getUserHubs(int userId);
    std::vector<ChannelInfo> getHubChannels(int hubId);

    // New: Ensure defaults
    int ensurePersonalHubWithGeneral(int userId, const std::string& hubName = "Personal Hub");

private:
    pqxx::connection conn_;
};

#endif // CHATDB_H

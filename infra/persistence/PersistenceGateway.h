#ifndef INFRA_PERSISTENCE_PERSISTENCE_GATEWAY_H
#define INFRA_PERSISTENCE_PERSISTENCE_GATEWAY_H
#include "infra/persistence/ConnectionPool.h"
#include "infra/persistence/DatabaseExecutor.h"
#include "infra/persistence/repositories/HubRepository.h"
#include "infra/persistence/repositories/MessageRepository.h"
#include "infra/persistence/repositories/UserRepository.h"
#include "infra/persistence/repositories/VoiceStateRepository.h"

#include <string>

class PersistenceGateway {
   public:
    explicit PersistenceGateway(const std::string& conninfo, std::size_t read_pool_size = 4,
                                std::size_t write_pool_size = 4);

    HubRepository& hubs() { return hub_repo_; }
    MessageRepository& messages() { return message_repo_; }
    UserRepository& users() { return user_repo_; }
    VoiceStateRepository& voice_state() { return voice_state_repo_; }

    const HubRepository& hubs() const { return hub_repo_; }
    const MessageRepository& messages() const { return message_repo_; }
    const UserRepository& users() const { return user_repo_; }
    const VoiceStateRepository& voice_state() const { return voice_state_repo_; }

   private:
    ConnectionPool read_pool_;
    ConnectionPool write_pool_;
    ReadRepositoryMux read_repo_mux_;
    WriteRepositoryMux write_repo_mux_;
    DatabaseExecutor db_executor_;
    HubRepository hub_repo_;
    MessageRepository message_repo_;
    UserRepository user_repo_;
    VoiceStateRepository voice_state_repo_;
};

#endif  // INFRA_PERSISTENCE_PERSISTENCE_GATEWAY_H

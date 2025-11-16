#ifndef INFRA_PERSISTENCE_PERSISTENCE_GATEWAY_H
#define INFRA_PERSISTENCE_PERSISTENCE_GATEWAY_H
#include "app/services/PublicIdService.h"
#include "infra/persistence/ConnectionPool.h"
#include "infra/persistence/RepositoryMux.h"
#include "infra/persistence/repositories/ChannelRepository.h"
#include "infra/persistence/repositories/HubRepository.h"
#include "infra/persistence/repositories/UserRepository.h"

#include <string>

class PersistenceGateway {
   public:
    explicit PersistenceGateway(const std::string& conninfo, std::size_t pool_size = 4);

    HubRepository& hubs() { return hub_repo_; }
    ChannelRepository& channels() { return channel_repo_; }
    UserRepository& users() { return user_repo_; }

    const HubRepository& hubs() const { return hub_repo_; }
    const ChannelRepository& channels() const { return channel_repo_; }
    const UserRepository& users() const { return user_repo_; }

    app::services::PublicIdService& ids() { return ids_; }
    const app::services::PublicIdService& ids() const { return ids_; }

   private:
    ConnectionPool pool_;
    RepositoryMux repo_mux_;
    HubRepository hub_repo_;
    ChannelRepository channel_repo_;
    UserRepository user_repo_;
    app::services::PublicIdService ids_;
};

#endif  // INFRA_PERSISTENCE_PERSISTENCE_GATEWAY_H

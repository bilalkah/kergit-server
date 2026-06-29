#ifndef INFRA_PERSISTENCE_PERSISTENCE_GATEWAY_H
#define INFRA_PERSISTENCE_PERSISTENCE_GATEWAY_H
#include "core/ServerConfig.h"
#include "infra/persistence/ConnectionPool.h"
#include "infra/persistence/TransactionRunner.h"
#include "infra/persistence/repositories/AuditRepository.h"
#include "infra/persistence/repositories/HubRepository.h"
#include "infra/persistence/repositories/MessageRepository.h"
#include "infra/persistence/repositories/UserRepository.h"

#include <string>

class PersistenceGateway {
   public:
    explicit PersistenceGateway(const core::DataBaseConfig& config);

    HubRepository& hubs() { return hub_repo_; }
    MessageRepository& messages() { return message_repo_; }
    UserRepository& users() { return user_repo_; }
    AuditRepository& audit() { return audit_repo_; }

    const HubRepository& hubs() const { return hub_repo_; }
    const MessageRepository& messages() const { return message_repo_; }
    const UserRepository& users() const { return user_repo_; }
    const AuditRepository& audit() const { return audit_repo_; }

   private:
    // Separate read/write pools act as a bulkhead: a flood of slow reads cannot starve writes.
    ConnectionPool read_pool_;
    ConnectionPool write_pool_;
    // The single owner of transaction execution + transient-failure recovery for every repository.
    TransactionRunner db_;
    HubRepository hub_repo_;
    MessageRepository message_repo_;
    UserRepository user_repo_;
    AuditRepository audit_repo_;
};

#endif  // INFRA_PERSISTENCE_PERSISTENCE_GATEWAY_H

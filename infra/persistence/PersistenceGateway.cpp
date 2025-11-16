#include "infra/persistence/PersistenceGateway.h"

PersistenceGateway::PersistenceGateway(const std::string& conninfo, std::size_t pool_size)
    : pool_(conninfo, pool_size),
      repo_mux_(pool_),
      hub_repo_(repo_mux_),
      channel_repo_(repo_mux_),
      user_repo_(repo_mux_) {}

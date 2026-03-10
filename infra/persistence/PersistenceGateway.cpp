#include "infra/persistence/PersistenceGateway.h"

PersistenceGateway::PersistenceGateway(const std::string& conninfo, std::size_t read_pool_size,
                                       std::size_t write_pool_size)
    : read_pool_(conninfo, read_pool_size),
      write_pool_(conninfo, write_pool_size),
      read_repo_mux_(read_pool_),
      write_repo_mux_(write_pool_),
      db_executor_(read_repo_mux_, write_repo_mux_),
      hub_repo_(db_executor_),
      channel_repo_(db_executor_),
      user_repo_(db_executor_),
      voice_state_repo_(db_executor_) {}

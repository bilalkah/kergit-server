#include "infra/persistence/PersistenceGateway.h"

#include "utils/Logger.h"

PersistenceGateway::PersistenceGateway(const core::DataBaseConfig& config)
    : read_pool_(config.to_connection_string(), config.read_pool_size),
      write_pool_(config.to_connection_string(), config.write_pool_size),
      read_repo_mux_(read_pool_),
      write_repo_mux_(write_pool_),
      db_executor_(read_repo_mux_, write_repo_mux_),
      hub_repo_(db_executor_),
      message_repo_(db_executor_),
      user_repo_(db_executor_),
      audit_repo_(db_executor_) {
    utils::log_line(
        config.ssl ? utils::LogLevel::INFO : utils::LogLevel::WARN,
        "PersistenceGateway: SSL mode: " + std::string(config.ssl ? "verify-full" : "OFF") + " (" +
            config.host + ":" + std::to_string(config.port) + ")");
}

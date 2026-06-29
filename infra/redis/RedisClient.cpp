#include "infra/redis/RedisClient.h"

#include "utils/Logger.h"

#include <sw/redis++/redis++.h>

namespace infra::redis {

namespace {
void log_redis_failure(const char* op, const std::exception& e) {
    utils::log_line(
        utils::LogLevel::WARN,
        std::string("RedisClient: ") + op + " failed (redis unavailable/restarting?): " + e.what());
}
}  // namespace

RedisClient::RedisClient(const std::string& host, uint16_t port) {
    sw::redis::ConnectionOptions opts;
    opts.host = host;
    opts.port = static_cast<int>(port);
    opts.socket_timeout = std::chrono::milliseconds(200);
    opts.connect_timeout = std::chrono::milliseconds(1000);
    // Constructing Redis does NOT open a socket: the underlying connection pool connects lazily
    // on first use and transparently reconnects after a drop. So Redis being down at startup
    // never aborts the server, and a mid-flight Redis restart heals on the next call. Each
    // operation below still guards against the throw window while Redis is unreachable.
    redis_ = std::make_unique<sw::redis::Redis>(opts);
}

RedisClient::~RedisClient() = default;

void RedisClient::setex(const std::string& key, std::chrono::seconds ttl,
                        const std::string& value) {
    try {
        redis_->setex(key, ttl, value);
    } catch (const std::exception& e) {
        // Best-effort ephemeral write; dropping it during an outage is acceptable.
        log_redis_failure("setex", e);
    }
}

bool RedisClient::setnxex(const std::string& key, std::chrono::seconds ttl,
                          const std::string& value) {
    try {
        if (!redis_->setnx(key, value)) {
            return false;
        }
        redis_->expire(key, ttl.count());
        return true;
    } catch (const std::exception& e) {
        // Treat as "could not acquire" so callers fall back to the safe path during an outage.
        log_redis_failure("setnxex", e);
        return false;
    }
}

std::optional<std::string> RedisClient::get(const std::string& key) {
    try {
        auto val = redis_->get(key);
        if (val) return *val;
        return std::nullopt;
    } catch (const std::exception& e) {
        log_redis_failure("get", e);
        return std::nullopt;
    }
}

void RedisClient::del(const std::string& key) {
    try {
        redis_->del(key);
    } catch (const std::exception& e) {
        log_redis_failure("del", e);
    }
}

std::optional<std::chrono::seconds> RedisClient::ttl(const std::string& key) {
    try {
        auto result = redis_->ttl(key);
        if (result < 0) return std::nullopt;  // -2: key doesn't exist, -1: no TTL
        return std::chrono::seconds(result);
    } catch (const std::exception& e) {
        log_redis_failure("ttl", e);
        return std::nullopt;
    }
}

}  // namespace infra::redis

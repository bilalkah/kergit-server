#include "infra/redis/RedisClient.h"

#include <sw/redis++/redis++.h>

namespace infra::redis {

RedisClient::RedisClient(const std::string& host, uint16_t port) {
    sw::redis::ConnectionOptions opts;
    opts.host = host;
    opts.port = static_cast<int>(port);
    opts.socket_timeout = std::chrono::milliseconds(200);
    opts.connect_timeout = std::chrono::milliseconds(1000);
    redis_ = std::make_unique<sw::redis::Redis>(opts);
}

RedisClient::~RedisClient() = default;

void RedisClient::setex(const std::string& key, std::chrono::seconds ttl,
                        const std::string& value) {
    redis_->setex(key, ttl, value);
}

bool RedisClient::setnxex(const std::string& key, std::chrono::seconds ttl,
                          const std::string& value) {
    if (!redis_->setnx(key, value)) {
        return false;
    }
    redis_->expire(key, ttl.count());
    return true;
}

std::optional<std::string> RedisClient::get(const std::string& key) {
    auto val = redis_->get(key);
    if (val) return *val;
    return std::nullopt;
}

void RedisClient::del(const std::string& key) { redis_->del(key); }

std::optional<std::chrono::seconds> RedisClient::ttl(const std::string& key) {
    auto result = redis_->ttl(key);
    if (result < 0) return std::nullopt;  // -2: key doesn't exist, -1: no TTL
    return std::chrono::seconds(result);
}

}  // namespace infra::redis

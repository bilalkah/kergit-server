#ifndef INFRA_REDIS_REDISCLIENT_H
#define INFRA_REDIS_REDISCLIENT_H

#include <chrono>
#include <memory>
#include <optional>
#include <string>

namespace sw::redis {
class Redis;
}

namespace infra::redis {

class RedisClient {
   public:
    RedisClient(const std::string& host, uint16_t port);
    ~RedisClient();

    RedisClient(const RedisClient&) = delete;
    RedisClient& operator=(const RedisClient&) = delete;

    void setex(const std::string& key, std::chrono::seconds ttl, const std::string& value);
    bool setnxex(const std::string& key, std::chrono::seconds ttl, const std::string& value);
    std::optional<std::string> get(const std::string& key);
    void del(const std::string& key);
    std::optional<std::chrono::seconds> ttl(const std::string& key);

   private:
    std::unique_ptr<sw::redis::Redis> redis_;
};

}  // namespace infra::redis

#endif  // INFRA_REDIS_REDISCLIENT_H

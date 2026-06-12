#ifndef APP_SERVICES_USER_USERCACHE_H
#define APP_SERVICES_USER_USERCACHE_H

#include "core/cache/OnMemoryCache.h"
#include "domains/User.h"

#include <expected>
#include <memory>
namespace app::services {

enum class UserCacheError { NotFound, CorruptedEntry, BackendFailure };

class IUserCache {
   public:
    virtual ~IUserCache() = default;

    virtual std::expected<User, UserCacheError> get(UserId id) = 0;

    virtual void put(const User& user) = 0;
    virtual void invalidate(UserId id) = 0;
};

class UserCache final : public IUserCache {
   public:
    explicit UserCache();
    ~UserCache() override = default;

    std::expected<User, UserCacheError> get(UserId id) override;

    void put(const User& user) override;

    void invalidate(UserId id) override;

   private:
    std::unique_ptr<core::cache::ICache> cache_;
};

}  // namespace app::services

#endif  // APP_SERVICES_USER_USERCACHE_H

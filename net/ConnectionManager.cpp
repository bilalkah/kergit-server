#include "net/ConnectionManager.h"

namespace net {
void ConnectionManager::attach(const ConnId& id, IWebSocket* ws) {
    std::lock_guard<std::mutex> lk(mu_);
    by_id_[id] = ws;
}
void ConnectionManager::detach(const ConnId& id) {
    std::lock_guard<std::mutex> lk(mu_);
    by_id_.erase(id);
}
IWebSocket* ConnectionManager::get(const ConnId& id) const {
    std::lock_guard<std::mutex> lk(mu_);
    auto it = by_id_.find(id);
    return it == by_id_.end() ? nullptr : it->second;
}
}  // namespace net

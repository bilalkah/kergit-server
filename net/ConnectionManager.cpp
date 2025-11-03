#include "net/ConnectionManager.h"

namespace net {

void ConnectionManager::attach(const ConnId& conn_id, UwsSocket* ws) {
    std::lock_guard<std::mutex> lk(mu_);
    map_[conn_id] = ws;
}

void ConnectionManager::detach(const ConnId& conn_id) {
    std::lock_guard<std::mutex> lk(mu_);
    map_.erase(conn_id);
}

UwsSocket* ConnectionManager::get(const ConnId& conn_id) const {
    std::lock_guard<std::mutex> lk(mu_);
    auto it = map_.find(conn_id);
    return it == map_.end() ? nullptr : it->second;
}

std::vector<UwsSocket*> ConnectionManager::get_all() const {
    std::lock_guard<std::mutex> lk(mu_);
    std::vector<UwsSocket*> result;
    result.reserve(map_.size());
    for (const auto& pair : map_) {
        result.push_back(pair.second);
    }
    return result;
}

void ConnectionManager::for_each(std::function<void(UwsSocket*)> func) const {
    std::lock_guard<std::mutex> lk(mu_);
    for (const auto& pair : map_) {
        func(pair.second);
    }
}

}  // namespace net

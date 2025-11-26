#include "app/pubsub/PubSub.h"

#include <algorithm>

namespace app {

void PubSub::subscribe(const ConnId& conn, const std::string& topic) {
    std::lock_guard<std::mutex> lock(mu_);
    topic_to_conns_[topic].insert(conn);
    conn_to_topics_[conn].insert(topic);
}

void PubSub::unsubscribe(const ConnId& conn, const std::string& topic) {
    std::lock_guard<std::mutex> lock(mu_);
    topic_to_conns_[topic].erase(conn);
    conn_to_topics_[conn].erase(topic);
    if (conn_to_topics_[conn].empty()) {
        conn_to_topics_.erase(conn);
    }
}

void PubSub::unsubscribe_all(const ConnId& conn) {
    std::lock_guard<std::mutex> lock(mu_);
    if (conn_to_topics_.find(conn) != conn_to_topics_.end()) {
        for (const auto& topic : conn_to_topics_[conn]) {
            topic_to_conns_[topic].erase(conn);
        }
        conn_to_topics_.erase(conn);
    }
}

std::unordered_set<ConnId> PubSub::subscribers(const std::string& topic) const {
    std::lock_guard<std::mutex> lock(mu_);
    if (topic_to_conns_.find(topic) != topic_to_conns_.end()) {
        return topic_to_conns_.at(topic);
    }
    return {};
}

}  // namespace app

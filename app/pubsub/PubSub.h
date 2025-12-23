#ifndef APP_PUBSUB_PUBSUB_H_
#define APP_PUBSUB_PUBSUB_H_

#include "domains/ids/Ids.h"

#include <mutex>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace app {

class PubSub {
   public:
    void subscribe(const ConnId& conn, const std::string& topic);
    void unsubscribe(const ConnId& conn, const std::string& topic);
    void unsubscribe_all(const ConnId& conn);
    void drop_topic(const std::string& topic);
    std::unordered_set<ConnId> subscribers(const std::string& topic) const;

   private:
    std::unordered_map<std::string, std::unordered_set<ConnId>> topic_to_conns_;
    std::unordered_map<ConnId, std::unordered_set<std::string>> conn_to_topics_;
    mutable std::mutex mu_;
};

}  // namespace app
#endif  // APP_PUBSUB_PUBSUB_H_

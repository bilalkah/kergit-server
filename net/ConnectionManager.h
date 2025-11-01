#ifndef NET_CONNECTION_MANAGER_H
#define NET_CONNECTION_MANAGER_H

#include "core/IWebSocket.h"
#include "domains/ids/Ids.h"

#include <mutex>
#include <unordered_map>

namespace net {

class ConnectionManager {
   public:
    void attach(const ConnId& conn_id, IWebSocket* ws);
    void detach(const ConnId& conn_id);
    IWebSocket* get(const ConnId& conn_id) const;

   private:
    mutable std::mutex mu_;
    std::unordered_map<ConnId, IWebSocket*> by_id_;
};

}  // namespace net

#endif  // NET_CONNECTION_MANAGER_H

#ifndef NET_CONNECTION_MANAGER_H
#define NET_CONNECTION_MANAGER_H

#include "core/Types.h"
#include "domains/ids/Ids.h"

#include <mutex>
#include <string>
#include <unordered_map>

namespace net {

class ConnectionManager {
   public:
    void attach(const ConnId& conn_id, UwsSocket* ws);
    void detach(const ConnId& conn_id);
    UwsSocket* get(const ConnId& conn_id) const;
    std::vector<UwsSocket*> get_all() const;
    void for_each(std::function<void(UwsSocket*)> func) const;

   private:
    mutable std::mutex mu_;
    std::unordered_map<ConnId, UwsSocket*> map_;
};

}  // namespace net

#endif  // NET_CONNECTION_MANAGER_H

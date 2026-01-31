#ifndef NET_ROUTER_H
#define NET_ROUTER_H

#include "domains/ids/Ids.h"
#include "net/NetworkStack.h"
#include "net/outbound/OutgoingQueue.h"
#include "utils/Loggable.h"

namespace net {

class NetworkRouter : public utils::Loggable, public outbound::IOutboundSink {
   public:
    ~NetworkRouter() override = default;

    void register_stack(std::unique_ptr<NetworkStack> stack);

    // IOutboundSink implementation
    outbound::PushResult push(const outbound::OutgoingMessage& msg) override;
    outbound::PushResult push(outbound::OutgoingMessage&& msg) override;

    void stop_all();
    void start_all();

   private:
    std::unordered_map<NetStackId, std::vector<GlobalConnId>> group_outgoing_msg(
        const outbound::OutgoingMessage& msg);
    /**
     * Network stacks by id
     */
    std::unordered_map<NetStackId, std::unique_ptr<NetworkStack>> net_stacks_by_id_;
};

}  // namespace net

#endif  // NET_ROUTER_H

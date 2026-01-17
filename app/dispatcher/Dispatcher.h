#ifndef APP_DISPATCHER_H
#define APP_DISPATCHER_H

#include "app/commands/ICommand.h"
#include "app/dispatcher/CommandContext.h"
#include "domains/ids/Ids.h"
#include "proto/envelope.pb.h"
#include "utils/Loggable.h"

#include <functional>
#include <nlohmann/json.hpp>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>

namespace app {

class Dispatcher : public utils::Loggable {
   public:
    std::vector<net::outbound::OutgoingMessage> dispatch(const std::string& type,
                                                         CommandContext& ctx,
                                                         const queue::Event& evt);
    std::vector<net::outbound::OutgoingMessage> dispatch(const sercom::protocol::Envelope_Type type,
                                                         CommandContext& ctx,
                                                         const queue::Event& evt);

    std::unordered_set<std::string> registered_commands() const;
    void register_all();

   private:
    std::unordered_map<sercom::protocol::Envelope_Type, std::unique_ptr<ICommand>> map_proto_;
    std::unordered_map<std::string, std::unique_ptr<ICommand>> map_str_;
};

}  // namespace app

#endif  // APP_DISPATCHER_H

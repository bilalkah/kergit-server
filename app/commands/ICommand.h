#ifndef APP_ICOMMAND_H
#define APP_ICOMMAND_H

#include "app/queue/OutgoingQueue.h"
#include "domains/ids/Ids.h"
#include "net/PerSocketData.h"

#include <chrono>
#include <nlohmann/json.hpp>
#include <string>

namespace app {

namespace {
using namespace std::chrono;
}
struct Input {
    nlohmann::json data;
    system_clock::time_point received_at;
};

struct Output {
    bool success;
    std::string error_code;
    std::string error_message;

    std::vector<OutgoingMessage> messages;
    system_clock::time_point sent_at;
};

struct CommandContext {
    ConnId conn_id;
    UserId user_id;
    HubId current_hub_id;
    ChannelId current_channel_id;
    bool authenticated{false};

    Input input;
    Output output;
};

class ICommand {
   public:
    virtual ~ICommand() = default;
    virtual void execute(CommandContext&) = 0;
};

}  // namespace app

#endif  // APP_ICOMMAND_H

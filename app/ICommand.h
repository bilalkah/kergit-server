#ifndef APP_ICOMMAND_H
#define APP_ICOMMAND_H

#include "domains/ids/Ids.h"

#include <chrono>
#include <nlohmann/json.hpp>
#include <string>

struct CommandContext {
    ConnId conn_id{""};
    UserId user_id{""};
    HubId current_hub{""};
    ChannelId current_chan{""};
    std::string remote_ip{""};
    std::chrono::system_clock::time_point received_at;
};

class ICommand {
   public:
    virtual ~ICommand() = default;
    virtual nlohmann::json execute(const CommandContext&, const nlohmann::json&) = 0;
};

#endif  // APP_ICOMMAND_H

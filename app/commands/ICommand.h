#ifndef APP_ICOMMAND_H
#define APP_ICOMMAND_H

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
    nlohmann::json data;
    system_clock::time_point sent_at;
};

struct CommandContext {
    CommandContext(net::PerSocketData& psd_ref) : psd(psd_ref) {}
    net::PerSocketData& psd;
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

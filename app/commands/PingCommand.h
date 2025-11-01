#ifndef APP_PINGCOMMAND_H
#define APP_PINGCOMMAND_H

#include "app/ICommand.h"

class PingCommand : public ICommand {
   public:
    nlohmann::json execute(const CommandContext&, const nlohmann::json& in) override {
        return {{"type", "pong"}, {"ts", /* your time util */ 0}};
    }
};

#endif  // APP_PINGCOMMAND_H

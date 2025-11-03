#ifndef APP_PINGCOMMAND_H
#define APP_PINGCOMMAND_H

#include "app/commands/ICommand.h"

namespace app {

class PingCommand : public ICommand {
   public:
    void execute(CommandContext&) override { return; }
};

}  // namespace app

#endif  // APP_PINGCOMMAND_H

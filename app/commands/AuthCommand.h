#ifndef APP_COMMANDS_AUTHCOMMAND_H
#define APP_COMMANDS_AUTHCOMMAND_H

#include "app/commands/ICommand.h"
#include "app/services/AuthService.h"

namespace app {

class AuthCommand : public ICommand {
   public:
    AuthCommand();
    void execute(CommandContext&);

   private:
    void fill_psd(net::PerSocketData& psd, const infra::security::token::UserClaims& claims);
    app::services::AuthService auth_service_;
};

}  // namespace app

#endif  // APP_COMMANDS_AUTHCOMMAND_H

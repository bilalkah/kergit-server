#ifndef APP_SERVICES_AUTHSERVICE_H
#define APP_SERVICES_AUTHSERVICE_H

#include "infra/security/token/SupabaseJwtVerifier.h"

namespace app::services {

struct AuthServiceResult {
    bool success;
    infra::security::token::UserClaims claims;
    std::string error_message;
};

class AuthService {
   public:
    AuthService();

    AuthServiceResult authenticate(const std::string& token);

   private:
    infra::security::token::SupabaseJWTVerifier token_verifier_;
};

}  // namespace app::services

#endif  // APP_SERVICES_AUTHSERVICE_H

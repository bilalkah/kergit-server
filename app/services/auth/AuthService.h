#ifndef APP_SERVICES_AUTHSERVICE_H
#define APP_SERVICES_AUTHSERVICE_H

#include "infra/security/token/SupabaseVerifier.h"
#include "utils/Loggable.h"

#include <expected>
#include <string_view>

namespace app::services {

enum class AuthError {
    InvalidToken,
    ExpiredToken,
    Other,
};

using AuthResult = std::expected<infra::security::token::UserClaims, AuthError>;

class AuthService : utils::Loggable {
   public:
    AuthService();

    AuthResult authenticate(const std::string_view token);

   private:
    infra::security::token::SupabaseVerifier token_verifier_;
};

}  // namespace app::services

#endif  // APP_SERVICES_AUTHSERVICE_H

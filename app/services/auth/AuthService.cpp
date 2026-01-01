#include "app/services/auth/AuthService.h"

using namespace infra::security::token;

namespace app::services {

AuthService::AuthService()
    : token_verifier_([this] {
          auto exp = SupabaseJWTVerifier::create();
          if (!exp) {
              log(utils::LogLevel::ERROR, "Failed to create SupabaseJWTVerifier in AuthService: ",
                  static_cast<int>(exp.error()));
              std::exit(EXIT_FAILURE);
          }
          return std::move(exp.value());
      }()) {}

AuthResult AuthService::authenticate(const std::string_view token) {
    auto result = token_verifier_.verify_token(std::string{token});
    if (!result) {
        switch (result.error()) {
            case JwtVerifyError::InvalidSignature:
                return std::unexpected(AuthError::InvalidToken);
            case JwtVerifyError::TokenExpired:
                return std::unexpected(AuthError::ExpiredToken);
            default:
                return std::unexpected(AuthError::Other);
        }
    }

    return result.value();
}

}  // namespace app::services

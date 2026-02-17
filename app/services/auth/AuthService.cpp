#include "app/services/auth/AuthService.h"

using namespace infra::security::token;

namespace app::services {

AuthService::AuthService()
    : token_verifier_([this] {
          auto exp = SupabaseVerifier::create();
          if (!exp) {
              throw std::runtime_error("Failed to initialize SupabaseVerifier, err: " +
                                       std::to_string(static_cast<int>(exp.error())));
          }
          return std::move(exp.value());
      }()) {}

AuthResult AuthService::authenticate(const std::string_view token) {
    auto result = token_verifier_.verify_token(token);

    if (!result) {
        AuthError err;
        switch (result.error()) {
            case JwtVerifyError::InvalidSignature:
                err = AuthError::InvalidToken;
                break;
            case JwtVerifyError::TokenExpired:
                err = AuthError::ExpiredToken;
                break;
            default:
                err = AuthError::Other;
                break;
        }
        return std::unexpected(err);
    }

    return result.value();
}

}  // namespace app::services

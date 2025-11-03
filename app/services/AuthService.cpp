#include "app/services/AuthService.h"

using namespace infra::security::token;

namespace app::services {

AuthService::AuthService(){};
AuthServiceResult AuthService::authenticate(const std::string& token) {
    AuthServiceResult result;
    const auto& user_claims = token_verifier_.verify_token(token);

    if (!user_claims.has_value()) {
        result.success = false;
        result.error_message = "Invalid token";
        return result;
    }

    result.success = true;
    result.claims = user_claims.value();

    return result;
}

}  // namespace app::services

#pragma once

#include "infra/security/token/ITokenVerifier.h"

#include <string_view>

namespace infra::security::token::jwt_scan {

bool parse_header(std::string_view json, std::string_view& alg, std::string_view& kid);

bool parse_payload(std::string_view json, UserClaims& out);

}  // namespace infra::security::token::jwt_scan

#ifndef INFRA_SECURITY_TOKEN_JWT_TYPES_H
#define INFRA_SECURITY_TOKEN_JWT_TYPES_H

#include <string>

namespace infra::security::token {

struct SupabaseJWK {
    std::string kty;  // key type
    std::string alg;  // algorithm
    std::string kid;  // key id
    std::string x;    // x coordinate (for EC keys)
    std::string y;    // y coordinate (for EC keys)
    std::string crv;  // curve (for EC keys)
    std::string n;    // modulus (for RSA keys)
    std::string e;    // exponent (for RSA keys)
};

}  // namespace infra::security::token

#endif  // INFRA_SECURITY_TOKEN_JWT_TYPES_H

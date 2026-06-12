#ifndef CORE_PUBLICENDPOINTCONFIG_H
#define CORE_PUBLICENDPOINTCONFIG_H

#include <string>
#include <string_view>

namespace core {

class PublicEndpointConfig {
   public:
    static PublicEndpointConfig from_env();
    static PublicEndpointConfig from_origins(std::string app_origin, std::string supabase_origin);

    const std::string& app_origin() const { return app_origin_; }
    const std::string& supabase_origin() const { return supabase_origin_; }

    std::string websocket_origin() const;
    std::string invite_base_url() const;
    std::string livekit_node_url(std::string_view node_id) const;
    std::string supabase_issuer() const;

   private:
    PublicEndpointConfig(std::string app_origin, std::string supabase_origin);

    static void validate_origin(std::string_view key, std::string_view origin);
    static std::string append_path(std::string_view origin, std::string_view path);

    const std::string app_origin_;
    const std::string supabase_origin_;
};

}  // namespace core

#endif  // CORE_PUBLICENDPOINTCONFIG_H

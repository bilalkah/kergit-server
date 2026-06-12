#ifndef APP_SERVICES_HUB_ROLEPOLICY_H
#define APP_SERVICES_HUB_ROLEPOLICY_H

#include "domains/Hub.h"

namespace app::services::hub {

inline bool canInvite(const Role actor_role) {
    return actor_role == Role::OWNER || actor_role == Role::ADMIN;
}

inline bool canKickHubMember(const Role actor_role, const Role target_role, const UserId& actor_id,
                             const UserId& target_id) {
    if (actor_id == target_id) return false;
    if (target_role == Role::OWNER) return false;

    if (actor_role == Role::OWNER) {
        return target_role == Role::ADMIN || target_role == Role::USER;
    }
    if (actor_role == Role::ADMIN) {
        return target_role == Role::USER;
    }
    return false;
}

inline bool canKickVoiceParticipant(const Role actor_role, const Role target_role,
                                    const UserId& actor_id, const UserId& target_id) {
    return canKickHubMember(actor_role, target_role, actor_id, target_id);
}

inline bool canChangeMemberRole(const Role actor_role, const Role target_role,
                                const Role desired_role, const UserId& actor_id,
                                const UserId& target_id) {
    if (actor_role != Role::OWNER) return false;
    if (actor_id == target_id) return false;
    if (target_role == Role::OWNER) return false;
    if (desired_role == Role::OWNER) return false;
    if (desired_role != Role::ADMIN && desired_role != Role::USER) return false;
    if (target_role != Role::ADMIN && target_role != Role::USER) return false;
    if (target_role == desired_role) return false;
    return true;
}

}  // namespace app::services::hub

#endif  // APP_SERVICES_HUB_ROLEPOLICY_H

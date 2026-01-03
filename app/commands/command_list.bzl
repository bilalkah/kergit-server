"Command list for app/BUILD"

COMMAND_LIST = [
    "//app/commands/auth:auth_command",
    "//app/commands/system:disconnection_command",
    "//app/commands:join_channel_command",
    "//app/commands:send_message_command",
    "//app/commands:create_channel_command",
    "//app/commands:delete_channel_command",
    "//app/commands:rename_channel_command",
    # "//app/commands:create_hub_command",
    # "//app/commands:rename_hub_command",
    # "//app/commands:delete_hub_command",
    # "//app/commands:get_hub_invite_command",
    # "//app/commands:join_hub_by_invite_command",
    # "//app/commands:leave_hub_command",
    # "//app/commands:update_member_role_command",
    # "//app/commands:update_profile_command",
    # "//app/commands/system:disconnect_command",
]

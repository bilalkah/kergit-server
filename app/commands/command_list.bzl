"Command list for app/BUILD"

COMMAND_LIST = [
    "//app/commands/session:bootstrap_command",
    "//app/commands/session:authenticate_command",
    "//app/commands/session:request_state_sync_command",
    "//app/commands/session:disconnection_command",
    "//app/commands/activity:select_active_channel_command",
    "//app/commands/activity:typing_command",
    "//app/commands/activity:join_voice_channel_command",
    "//app/commands/activity:voice_channel_activity_command",
    "//app/commands/message:send_message_command",
    "//app/commands/message:fetch_messages_before_command",
    "//app/commands/hub:create_hub_command",
    "//app/commands/hub:join_hub_command",
    "//app/commands/hub:create_hub_join_code_command",
    "//app/commands/hub:leave_hub_command",
    "//app/commands/hub:remove_hub_command",
    "//app/commands/hub:update_hub_command",
    "//app/commands/user:update_user_command",
    "//app/commands/channel:create_channel_command",
    "//app/commands/channel:rename_channel_command",
    "//app/commands/channel:remove_channel_command",
]

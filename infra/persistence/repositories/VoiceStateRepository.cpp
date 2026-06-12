#include "infra/persistence/repositories/VoiceStateRepository.h"

void VoiceStateRepository::upsert(const UserId& user, const ChannelId& channel, bool muted,
                                  bool deafened) {
    db_.write("VoiceStateRepository.upsert", [&](pqxx::work& txn) {
        txn.exec(
            "INSERT INTO public.voice_state (user_id, channel_id, muted, deafened, joined_at, "
            "last_seen) "
            "VALUES ($1::uuid, $2::uuid, $3, $4, now(), now()) "
            "ON CONFLICT (user_id) DO UPDATE SET "
            "channel_id = EXCLUDED.channel_id, muted = EXCLUDED.muted, deafened = "
            "EXCLUDED.deafened, "
            "joined_at = now(), last_seen = now()",
            pqxx::params{user.value, channel.value, muted, deafened});
        return 0;
    });
}

void VoiceStateRepository::update_mute_state(const UserId& user, bool muted, bool deafened) {
    db_.write("VoiceStateRepository.update_mute_state", [&](pqxx::work& txn) {
        txn.exec(
            "UPDATE public.voice_state SET muted = $2, deafened = $3, last_seen = now() "
            "WHERE user_id = $1::uuid",
            pqxx::params{user.value, muted, deafened});
        return 0;
    });
}

void VoiceStateRepository::remove(const UserId& user) {
    db_.write("VoiceStateRepository.remove", [&](pqxx::work& txn) {
        txn.exec("DELETE FROM public.voice_state WHERE user_id = $1::uuid",
                 pqxx::params{user.value});
        return 0;
    });
}

void VoiceStateRepository::remove_channel(const ChannelId& channel) {
    db_.write("VoiceStateRepository.remove_channel", [&](pqxx::work& txn) {
        txn.exec("DELETE FROM public.voice_state WHERE channel_id = $1::uuid",
                 pqxx::params{channel.value});
        return 0;
    });
}

std::vector<VoiceStateRepository::VoiceStateRow> VoiceStateRepository::load_all() {
    return db_.read("VoiceStateRepository.load_all", [&](pqxx::work& txn) {
        auto res = txn.exec(
            "SELECT user_id::text, channel_id::text, muted, deafened FROM public.voice_state");

        std::vector<VoiceStateRow> rows;
        rows.reserve(res.size());
        for (const auto& row : res) {
            rows.push_back(VoiceStateRow{
                .user_id = UserId{row[0].as<std::string>()},
                .channel_id = ChannelId{row[1].as<std::string>()},
                .muted = row[2].as<bool>(),
                .deafened = row[3].as<bool>(),
            });
        }
        return rows;
    });
}

void VoiceStateRepository::clear_all() {
    db_.write("VoiceStateRepository.clear_all", [&](pqxx::work& txn) {
        txn.exec("DELETE FROM public.voice_state");
        return 0;
    });
}

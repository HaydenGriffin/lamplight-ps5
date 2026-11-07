// Lamplight for PS5 - Now Playing: the room's player, its controls and its volume.
// Copyright (C) 2026 Hayden Griffin
// SPDX-License-Identifier: GPL-3.0-or-later
#include "lamplight/application.hpp"
#include <algorithm>
#include <cmath>

namespace lamplight
{
using hui::Action;
using hui::Direction;
namespace
{
// Volume steps send at most this often while Up or Down repeats; the last one always goes.
constexpr std::int64_t kVolumeEveryMs = 300;
} // namespace

std::string Application::music_speaker() const
{
    if (!current_room().media.empty())
        return current_room().media;
    for (const auto &room : config_.rooms)
        if (!room.media.empty())
            return room.media;
    return "";
}
std::string Application::music_player() const
{
    return music_session(config_.speakers, snapshot_.states, music_speaker()).target;
}
Entity Application::music_state() const
{
    return music_session(config_.speakers, snapshot_.states, music_speaker()).state;
}
double Application::music_position(const Entity &player) const
{
    const double position = number(field(player.attributes, "media_position"));
    if (std::isnan(position))
        return 0;
    double at = position;
    const auto updated =
        parse_iso_ms(text(player.attributes, "media_position_updated_at"), clock_.zone);
    if (player.state == "playing" && updated != kNoTime && now() > updated)
        at += static_cast<double>(now() - updated) / 1000.0;
    const double duration = number(field(player.attributes, "media_duration"));
    if (!std::isnan(duration) && duration > 0)
        at = std::min(at, duration);
    return std::max(0.0, at);
}
std::string Application::cover_url() const
{
    if (page_ != Page::music)
        return "";
    const auto player = music_state();
    if (player.state != "playing" && player.state != "paused")
        return "";
    return ha_url(text(player.attributes, "entity_picture"));
}
void Application::update_music(const hui::InputFrame &input)
{
    const auto id = music_player();
    const auto player = music_state();
    if (input.is_pressed(Action::north) && open_sheet(id, player.name(id), "music"))
        return;
    if (input.is_pressed(Action::west) && open_speakers_sheet())
        return;
    if (input.nav == Direction::left || input.nav == Direction::right)
        music_focus_ = std::clamp(music_focus_ + (input.nav == Direction::left ? -1 : 1), 0, 2);
    if (input.is_pressed(Action::confirm))
        press_music(music_focus_);
    // Up and Down: the volume, shown at once and sent as it settles.
    const double level = number(field(player.attributes, "volume_level"));
    if ((input.nav == Direction::up || input.nav == Direction::down) && !std::isnan(level) &&
        player.available())
    {
        const double from = music_volume_ >= 0 ? music_volume_ : level;
        music_volume_ = std::clamp(from + (input.nav == Direction::up ? 0.05 : -0.05), 0.0, 1.0);
        music_volume_ = std::round(music_volume_ * 20) / 20;
    }
    // The right stick glides it, as it does a tile's level; up is more.
    if (std::fabs(input.stick2_y) >= 0.2f && !std::isnan(level) && player.available())
    {
        const double from = music_volume_ >= 0 ? music_volume_ : level;
        music_volume_ = std::clamp(from - static_cast<double>(input.stick2_y) * 0.5 *
                                              static_cast<double>(frame_dt_),
                                   0.0, 1.0);
    }
    if (music_volume_ >= 0)
        music_volume_player_ = id;
}
// The volume being set goes to Home Assistant once per new level (at most every 300 ms), from any
// page, so leaving Now Playing doesn't lose the last step. It shows until Home Assistant agrees,
// or for three seconds after the last send.
void Application::send_volume()
{
    if (music_volume_ < 0)
        return;
    const auto &player = snapshot_.states.get(music_volume_player_);
    const double level = number(field(player.attributes, "volume_level"));
    const bool sent = std::fabs(music_volume_sent_level_ - music_volume_) < 0.001;
    if (sent)
    {
        if ((!std::isnan(level) && std::fabs(level - music_volume_) < 0.02) ||
            now() - music_volume_sent_ > 3000)
            music_volume_ = music_volume_sent_level_ = -1;
        return;
    }
    if (now() - music_volume_sent_ < kVolumeEveryMs)
        return;
    music_volume_sent_ = now();
    music_volume_sent_level_ = music_volume_;
    if (!dispatch(service("media_player", "volume_set", {{"volume_level", music_volume_}},
                          music_volume_player_),
                  Job::room))
    {
        music_volume_ = music_volume_sent_level_ = -1;
        toast("Not connected to Home Assistant", true, 5000);
    }
}
void Application::press_music(int control)
{
    const auto id = music_player();
    const auto player = music_state();
    // Previous and Next only while something is loaded; nothing while a press is on its way.
    const bool loaded = player.state == "playing" || player.state == "paused";
    if (!player.available() || room_busy_ == id || (control != 1 && !loaded))
        return;
    const char *name = control == 0   ? "media_previous_track"
                       : control == 2 ? "media_next_track"
                                      : "media_play_pause";
    if (!dispatch(service("media_player", name, Json::object(), id), Job::room))
    {
        toast("Not connected to Home Assistant", true, 5000);
        return;
    }
    room_busy_ = id;
    room_busy_until_ = now() + 8000;
}
} // namespace lamplight

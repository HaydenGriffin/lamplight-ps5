// Lamplight for PS5 - Which speakers play together, and the rows of the Speakers sheet.
// Copyright (C) 2026 Hayden Griffin
// SPDX-License-Identifier: GPL-3.0-or-later
#include "lamplight/speakers.hpp"
#include "lamplight/timers.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <utility>

namespace lamplight
{
namespace
{
bool active_state(const std::string &state)
{
    return state == "playing" || state == "paused" || state == "buffering";
}
std::vector<std::string> members(const Speakers &speakers, const StateTable &states)
{
    return speakers.group.empty()
               ? std::vector<std::string>{}
               : strings(field(states.get(speakers.group).attributes, "group_members"));
}
bool has(const std::vector<std::string> &list, const std::string &value)
{
    return std::find(list.begin(), list.end(), value) != list.end();
}
// A member playing the group's queue. Music Assistant's group can stay idle a while after a start
// or a move though its speakers already play, so such a member counts as the group playing.
const Entity *live_member(const Speakers &speakers, const StateTable &states)
{
    const auto queue = text(states.get(speakers.group).attributes, "active_queue");
    if (queue.empty())
        return nullptr;
    for (const auto &id : members(speakers, states))
    {
        const auto &member = states.get(id);
        if (active_state(member.state) && text(member.attributes, "active_queue") == queue)
            return &member;
    }
    return nullptr;
}
std::string plural_rooms(int count)
{
    return std::to_string(count) + (count == 1 ? " room" : " rooms");
}
std::string speaker_name(const Speakers &speakers, const StateTable &states,
                         const std::string &player)
{
    if (player == speakers.group)
        return speakers.group_name;
    for (const auto &s : speakers.rooms)
        if (s.player == player && !s.name.empty())
            return s.name;
    return states.get(player).name(player);
}
Control volume_row(const std::string &player, const std::string &name, const Entity &state)
{
    Control row;
    row.kind = Control::Kind::slider;
    row.entity = player;
    row.id = "volume";
    row.label = name;
    row.step = 5;
    const double level = number(field(state.attributes, "volume_level"));
    row.value = std::isnan(level) ? 0 : std::round(level * 100);
    row.disabled = !state.available() || std::isnan(level);
    row.disabled_words = state.available() ? "No volume" : "Offline";
    return row;
}
Request transfer(const std::string &from, const std::string &to)
{
    return service("music_assistant", "transfer_queue",
                   {{"source_player", from}, {"auto_play", true}}, to);
}
// Why the music can't move between this room and the group now ("" when it can).
std::string why_not_together(const Speakers &speakers, const StateTable &states,
                             const std::string &here, const std::string &name)
{
    if (!states.get(speakers.group).available())
        return speakers.group_name + " is offline";
    if (!states.get(here).available())
        return name + " speaker is offline";
    if (!has(members(speakers, states), here))
        return name + " isn't in " + speakers.group_name + " \xC2\xB7 add it below";
    const bool together = in_session(speakers, states, here);
    if (!together && !active_state(states.get(here).state))
        return "Start some music first";
    return "";
}
void add_where(std::vector<Control> &rows, const Speakers &speakers, const StateTable &states,
               const std::string &here, const std::string &name, bool moving)
{
    if (speakers.group.empty())
        return;
    const bool together = in_session(speakers, states, here);
    Control row;
    row.kind = Control::Kind::buttons;
    row.id = "where";
    row.entity = here;
    row.label = "Where it plays";
    row.detail = moving ? "Moving\xE2\x80\xA6" : why_not_together(speakers, states, here, name);
    ControlOption option;
    option.label = together ? "Only " + name : "Play everywhere";
    option.value = "where";
    option.icon = together ? "speaker" : "speaker-multiple";
    option.disabled = !row.detail.empty();
    option.done = together ? "Playing only in " + name : "Playing in " + speakers.group_name;
    option.moves_to = together ? here : speakers.group;
    option.press = together ? transfer(speakers.group, here) : transfer(here, speakers.group);
    row.options.push_back(std::move(option));
    rows.push_back(std::move(row));
}
void add_elsewhere(std::vector<Control> &rows, const Speakers &speakers,
                   const std::vector<Speaker> &list, const StateTable &states,
                   const std::string &here, const std::string &name, bool moving)
{
    const bool here_together = in_session(speakers, states, here);
    std::string heading = "Playing elsewhere";
    auto add = [&](const std::string &source, const std::string &who, const Entity &state,
                   const std::string &verb, Request press, const std::string &done,
                   const std::string &moves_to, const std::string &icon)
    {
        Control row;
        row.kind = Control::Kind::buttons;
        row.id = "move";
        row.entity = source;
        const auto title = text(state.attributes, "media_title");
        row.label = title.empty() ? who : who + " \xC2\xB7 " + title;
        row.heading = std::exchange(heading, "");
        row.detail = moving ? "Moving\xE2\x80\xA6" : "";
        ControlOption option;
        option.value = "move";
        option.label = verb;
        option.icon = icon;
        option.disabled = moving || !states.get(here).available();
        option.done = done;
        option.moves_to = moves_to;
        option.press = std::move(press);
        row.options.push_back(std::move(option));
        rows.push_back(std::move(row));
    };
    // The group is joined, never moved here: that would silence every other room.
    if (!speakers.group.empty() && !here_together && group_active(speakers, states) &&
        has(members(speakers, states), here))
    {
        const auto *live = live_member(speakers, states);
        add(speakers.group, speakers.group_name, live ? *live : states.get(speakers.group), "Join",
            service("media_player", "join", {{"group_members", Json::array({here})}},
                    speakers.group),
            name + " joined " + speakers.group_name, "", "speaker-multiple");
    }
    for (const auto &s : list)
    {
        if (s.player == here || in_session(speakers, states, s.player))
            continue;
        const auto &state = states.get(s.player);
        if (!active_state(state.state))
            continue;
        const auto who = s.name.empty() ? state.name(s.player) : s.name;
        add(s.player, who, state, "Move here", transfer(s.player, here),
            "Moved the music from " + who + " to " + name, here, "arrow-collapse-down");
    }
}
// A timer's "H:MM:SS" duration in whole minutes, or -1.
int timer_minutes(const std::string &duration)
{
    int hours = 0, minutes = 0, seconds = 0;
    if (std::sscanf(duration.c_str(), "%d:%d:%d", &hours, &minutes, &seconds) != 3)
        return -1;
    return hours * 60 + minutes + (seconds >= 30 ? 1 : 0);
}
void add_sleep(std::vector<Control> &rows, const Speakers &speakers, const Speaker *speaker,
               const StateTable &states, const std::string &name, std::int64_t now,
               const Zone &zone)
{
    if (speakers.sleep_script.empty() || !speaker || speaker->sleep_timer.empty())
        return;
    const auto script = speakers.sleep_script.substr(speakers.sleep_script.find('.') + 1);
    const auto &timer = states.get(speaker->sleep_timer);
    const auto ends = parse_iso_ms(text(timer.attributes, "finishes_at"), zone);
    const bool running = timer.state == "active" && ends != kNoTime && ends > now;
    Control row;
    row.kind = Control::Kind::options;
    row.id = "sleep";
    row.entity = speaker->player;
    // Named, as the sheet can show music playing in another room too.
    row.label = "Sleep timer \xC2\xB7 " + name;
    if (running)
        row.detail = "Stops in " +
                     std::to_string(std::max<std::int64_t>(1, (ends - now + 59999) / 60000)) +
                     " min";
    auto add = [&](const std::string &label, int minutes, const std::string &done)
    {
        ControlOption option;
        option.value = std::to_string(minutes);
        option.label = label;
        option.done = done;
        option.press =
            service("script", script, {{"player", speaker->player}, {"minutes", minutes}});
        row.options.push_back(std::move(option));
    };
    add("Off", 0, "Sleep timer off");
    for (int minutes : speakers.sleep_minutes)
        add(std::to_string(minutes) + " min", minutes,
            "Music in " + name + " stops in " + std::to_string(minutes) + " minutes");
    // A running timer keeps the choice that started it ("0:15:00" is 15 min).
    const auto chosen = running ? std::to_string(timer_minutes(text(timer.attributes, "duration")))
                                : std::string("0");
    for (std::size_t i = 0; i < row.options.size(); ++i)
        if (row.options[i].value == chosen)
            row.selected = static_cast<int>(i);
    rows.push_back(std::move(row));
}
} // namespace

bool group_active(const Speakers &speakers, const StateTable &states)
{
    return !speakers.group.empty() && (active_state(states.get(speakers.group).state) ||
                                       live_member(speakers, states) != nullptr);
}
// The group's list holds every room, even idle, so the speaker must also be on the group's queue:
// after "Only Kitchen" the group can sit paused while the kitchen plays its own.
bool in_session(const Speakers &speakers, const StateTable &states, const std::string &player)
{
    if (player.empty() || player == speakers.group || !has(members(speakers, states), player) ||
        !group_active(speakers, states))
        return false;
    const auto queue = text(states.get(player).attributes, "active_queue");
    return queue.empty() || queue == text(states.get(speakers.group).attributes, "active_queue");
}
MusicSession music_session(const Speakers &speakers, const StateTable &states,
                           const std::string &player)
{
    if (speakers.group.empty() ||
        (player != speakers.group && !in_session(speakers, states, player)))
        return {player, false, states.get(player)};
    MusicSession session{speakers.group, true, states.get(speakers.group)};
    // The live member's state, title and cover; the group's own volume and members.
    if (const auto *live = live_member(speakers, states))
    {
        session.state.state = live->state;
        auto attributes = live->attributes;
        for (const char *key : {"volume_level", "group_members", "active_queue", "friendly_name"})
            if (const auto &own = field(session.state.attributes, key); !own.is_null())
                attributes[key] = own;
        session.state.attributes = std::move(attributes);
    }
    return session;
}
std::vector<Speaker> speaker_list(const Config &config)
{
    if (!config.speakers.rooms.empty())
        return config.speakers.rooms;
    std::vector<Speaker> list;
    for (const auto &room : config.rooms)
        if (!room.media.empty() && std::none_of(list.begin(), list.end(), [&](const Speaker &s)
                                                { return s.player == room.media; }))
            list.push_back({room.name, room.media, ""});
    return list;
}
std::string speakers_words(const Config &config, const StateTable &states)
{
    int together = 0, alone = 0;
    for (const auto &s : speaker_list(config))
    {
        if (in_session(config.speakers, states, s.player))
            ++together;
        else if (states.get(s.player).state == "playing")
            ++alone;
    }
    if (together)
        return config.speakers.group_name + " \xC2\xB7 " + plural_rooms(together) +
               (alone ? " + " + std::to_string(alone) + (alone == 1 ? " other" : " others") : "");
    return alone ? "Playing in " + plural_rooms(alone) : "Nothing playing";
}
std::vector<Control> speaker_rows(const Config &config, const StateTable &states,
                                  const std::string &here, bool moving, std::int64_t now,
                                  const Zone &zone)
{
    const auto &speakers = config.speakers;
    const auto list = speaker_list(config);
    const auto name = speaker_name(speakers, states, here);
    const auto own =
        std::find_if(list.begin(), list.end(), [&](const Speaker &s) { return s.player == here; });
    std::vector<Control> rows;
    add_where(rows, speakers, states, here, name, moving);
    add_elsewhere(rows, speakers, list, states, here, name, moving);
    add_sleep(rows, speakers, own == list.end() ? nullptr : &*own, states, name, now, zone);
    const auto first_volume = rows.size();
    const auto group_list = members(speakers, states);
    // Together, one slider moves them all and keeps their balance.
    if (group_active(speakers, states))
    {
        const auto session = music_session(speakers, states, speakers.group);
        rows.push_back(volume_row(speakers.group, speakers.group_name, session.state));
    }
    for (const auto &s : list)
    {
        const auto &state = states.get(s.player);
        const auto who = s.name.empty() ? state.name(s.player) : s.name;
        auto row = volume_row(s.player, who, state);
        if (!speakers.group.empty() && state.available() && states.get(speakers.group).available())
        {
            const bool joined = has(group_list, s.player);
            row.detail = joined ? "Joins " + speakers.group_name : "";
            row.press_words = (joined ? "Leave " : "Join ") + speakers.group_name;
            row.press_done = who + (joined ? " left " : " joined ") + speakers.group_name;
            row.press = joined
                            ? service("media_player", "unjoin", Json::object(), s.player)
                            : service("media_player", "join",
                                      {{"group_members", Json::array({s.player})}}, speakers.group);
        }
        rows.push_back(std::move(row));
    }
    if (rows.size() > first_volume)
        rows[first_volume].heading = "Volume";
    return rows;
}
} // namespace lamplight

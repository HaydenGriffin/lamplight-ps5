// Lamplight for PS5 - Settings: the Home Assistant connection and what each page shows.
// Copyright (C) 2026 Hayden Griffin
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "lamplight/json.hpp"

namespace lamplight
{
// A chosen entity with the name and icon Lamplight shows for it (empty: Home Assistant's name
// and the page's default icon). The same shape as the cards' calendars and appliance_timers.
struct Choice
{
    std::string entity, name, icon;
};
struct HelpExample
{
    std::string say, run, agent, note;
    bool confirm = false, tap = true;
};
struct HelpCategory
{
    std::string name, icon, hint, tip;
    std::vector<HelpExample> examples;
};
// The room page: the room's main device (a media player, shown as the hero), its things as tiles,
// and its named actions (scripts and scenes) as pills. Config file only, as Help's categories.
struct RoomThing
{
    std::string entity, name, icon;
    bool confirm = false; // the two-press arm (DESIGN.md, Actions 6)
};
struct RoomPage
{
    std::string name, icon, media;
    std::vector<RoomThing> tiles, actions;
    bool empty() const
    {
        return media.empty() && tiles.empty() && actions.empty();
    }
};
struct Config
{
    std::string host, token;
    int port = 8123;
    std::string weather;
    std::vector<Choice> calendars;
    std::string view_assist; // the View Assist device's sensor, for timers and answers
    std::vector<int> quick_minutes{5, 10, 15, 20};
    std::vector<Choice> appliances;
    std::string wifi_ssid, wifi_password, wifi_security = "WPA";
    std::string agent = "conversation.home_assistant", wake_phrase = "Okay Nabu", room;
    std::string read_only; // an on/off entity: while on, Help's examples run nothing
    // Help's categories; empty means the built-in set (default_categories()).
    std::vector<HelpCategory> categories;
    RoomPage room_page;
    bool reduced_motion = false, vibration = false;
    bool configured() const;
    Json json() const;
    static Config parse(const Json &j);
    const std::vector<HelpCategory> &help() const;
};
const std::vector<HelpCategory> &default_categories();
// "{room}" filled with the room's name; without one, ", in the {room}" goes and "{room}" is "here".
std::string fill_room(const std::string &text, const std::string &room);
bool valid_ipv4(const std::string &host);
} // namespace lamplight

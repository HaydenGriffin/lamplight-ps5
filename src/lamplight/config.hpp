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
// A service call the settings name, made exactly as they write it: nothing else about it can vary.
struct ServiceCall
{
    std::string service; // "script.heating_set_room"
    Json data = Json::object();
    std::string entity;    // its target, if it takes one
    bool response = false; // it answers, and its "message" is what to say
    bool empty() const
    {
        return service.empty();
    }
};
// A service call with a name on a button: a thermostat's quick warm-up, or Everything off.
struct NamedCall
{
    std::string name, icon;
    ServiceCall call;
    bool confirm = false;     // the two-press arm (DESIGN.md, Actions 6)
    bool undo_lights = false; // the lamps are noted first, and Undo puts them back
};
// A room page: the room's main device (a media player, shown as the hero), its things as tiles,
// and its named actions (scripts and scenes) as pills. From lamplight.conf or Home Assistant's
// settings entity, as Help's categories.
struct RoomThing
{
    std::string entity, name, icon;
    bool confirm = false; // the two-press arm (DESIGN.md, Actions 6)
    // What its chart shows instead of its own reading (a room sensor beside a radiator's head).
    std::string sensor = {};
    std::vector<NamedCall> boosts = {}; // quick buttons at the top of its sheet
};
// One of the room's lighting choices, as the tablets' scenes: its main light to a level, its main
// light off, or a service of its own.
struct RoomScene
{
    std::string name, icon;
    int brightness = 0, kelvin = 0;
    bool off = false;
    ServiceCall call;
    std::string entity; // on while this scene holds (input_boolean.cinema_mode)
};
struct RoomPage
{
    std::string name, icon, media;
    std::vector<RoomThing> tiles, actions;
    // The room's own picture behind its page: a Home Assistant path ("/local/room.jpg"), or an
    // entity whose "background" attribute holds one, as a View Assist device's does.
    std::string photo;
    std::string light; // the room's main light, which its scenes set (else its first light tile)
    std::vector<RoomScene> scenes;
    bool empty() const
    {
        return media.empty() && tiles.empty() && actions.empty();
    }
};
// The doorbell: when trigger (a binary sensor turning on, or an event entity firing) goes, the
// camera fills the screen.
struct Doorbell
{
    std::string trigger, camera, name;
    bool empty() const
    {
        return trigger.empty() || camera.empty();
    }
};
// The house's notices: an entity whose "items" attribute lists them, and an event that dismisses
// one ({id, key}; "" when they can't be dismissed from here).
struct NoticeSource
{
    std::string entity, dismiss_event;
};
// One of the house's modes: a switch, with a service each way when more than the switch has to go.
struct HouseMode
{
    std::string entity, name, icon, about;
    ServiceCall on, off;
    bool confirm_off = false; // ending it asks twice (guests leaving ends a lot at once)
};
// What L2 opens from any page: the whole-house actions and the modes. undo_lights is the group
// whose lamps an action's Undo puts back.
struct HouseControls
{
    std::vector<NamedCall> actions;
    std::vector<HouseMode> modes;
    std::string undo_lights;
    bool empty() const
    {
        return actions.empty() && modes.empty();
    }
};
struct Speaker
{
    std::string name, player, sleep_timer;
};
// The speakers the Speakers sheet shows (without rooms here, each room page's own), and the group
// they play together in. sleep_script takes {player, minutes}; 0 minutes cancels.
struct Speakers
{
    std::string group, group_name = "Everywhere";
    std::vector<Speaker> rooms;
    std::string sleep_script;
    std::vector<int> sleep_minutes{15, 30, 60};
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
    std::vector<RoomPage> rooms;
    HouseControls house;
    Speakers speakers;
    // "12h", "24h", or "auto" (12-hour where Home Assistant's country usually writes it so).
    std::string clock = "auto";
    Doorbell doorbell;
    // A media-source folder (media-source://...) whose pictures the slideshow and the resting
    // clock show, a new one every photo_seconds.
    std::string photos;
    int photo_seconds = 60;
    // Behind the clock and the pages: "sky", "photos" (the folder's pictures, changing), "room"
    // (the View Assist device's background attribute, as the tablets), a Home Assistant path, or
    // an entity with a "background" attribute.
    std::string background = "sky";
    // Without photos, the resting clock draws the sky's mood: stars and the moon at night, clouds
    // by day, rain and snow as the forecast has them.
    bool mood = true;
    NoticeSource notices;
    // The Assist pipeline push-to-talk uses ("" for Home Assistant's preferred one).
    std::string pipeline;
    // An entity whose "lamplight" attribute holds settings, applied over these ("" for none).
    std::string settings_entity = "sensor.lamplight_ps5";
    bool reduced_motion = false, vibration = false;
    bool configured() const;
    Json json() const;
    static Config parse(const Json &j);
    const std::vector<HelpCategory> &help() const;
};
// These settings with another's keys over them, as lamplight.conf and Home Assistant's settings
// entity give them; with connection false, the address, token and settings entity are kept.
Config merged(const Config &base, const Json &over, bool connection);
const std::vector<HelpCategory> &default_categories();
// "{room}" filled with the room's name; without one, ", in the {room}" goes and "{room}" is "here".
std::string fill_room(const std::string &text, const std::string &room);
bool valid_ipv4(const std::string &host);
} // namespace lamplight

// Lamplight for PS5 - What can be set on a thing, and the service call that sets it.
// Copyright (C) 2026 Hayden Griffin
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "lamplight/client.hpp"
#include <cmath>
#include <cstdint>
#include <optional>

namespace lamplight
{
struct ControlOption
{
    std::string value, label;
    std::uint32_t colour = 0; // a swatch's colour, 0xRRGGBB
    std::string icon = {};
    bool asks = false;                 // a second press first (Unlock, Everything off)
    bool disabled = false;             // shown, and its row's detail says why it can't run
    std::optional<Request> press = {}; // what it runs, where that isn't worked out from the row
    std::string done = {};             // said once it has gone
    std::string moves_to = {};         // the music is moving to this speaker: watch that it starts
    bool undo_lights = false;          // the lamps are noted first, and Undo puts them back
    bool closes = false;               // it ends the sheet, so its Undo can be pressed
};
// One row of a thing's sheet.
struct Control
{
    enum class Kind
    {
        power,    // on or off
        slider,   // a level
        options,  // one of several, which stays chosen (a mode)
        swatches, // the same, as colours
        buttons,  // commands: nothing stays chosen (Open, Stop, Close)
        chart     // the last 24 hours of a reading: only to look at, so never focused
    };
    Kind kind = Kind::power;
    std::string entity, id, label;
    double min = 0, max = 100, step = 5, value = 0;
    double other_end = 0; // a thermostat's range is set whole: the end this row isn't
    std::vector<ControlOption> options;
    int selected = -1;
    bool on = false;
    std::string heading; // a small title above this row, where a new group of rows starts
    std::string detail;  // a line of its own: what a mode does, why it can't run, how long is left
    std::string icon;    // a mode's disc
    bool asks = false;   // a switch that wants a second press this way (ending Guests)
    // What Cross does, where that isn't worked out from the row: a mode's scripts, or a slider
    // that also does something (a speaker joining the group).
    std::optional<Request> press;
    std::string press_words, press_done;
    bool disabled = false; // a slider that can't move, and the word it shows for why
    std::string disabled_words;
    std::string attribute, unit;  // a chart: what it reads (else the state), and in what
    double target = std::nan(""); // a chart: the level it is set to, as a line across
    std::string key() const
    {
        return entity + "/" + id;
    }
};
// What the thing offers, from what its attributes say it supports. Empty for a thing that only
// reports (a sensor).
std::vector<Control> controls_for(const std::string &entity, const Entity &state);
// One lamp as a row among others: its level, or on and off if it doesn't dim.
Control lamp_control(const std::string &entity, const Entity &state, const std::string &name);
// The level a tile's right stick sets (brightness, target temperature, position, speed, volume).
std::optional<Control> primary_slider(const std::string &entity, const Entity &state);
// The lamps a light group holds, from its entity_id attribute (none for a single lamp).
std::vector<std::string> group_members(const std::string &entity, const Entity &state);
std::optional<Request> slider_request(const Control &control, double value);
std::optional<Request> option_request(const Control &control, int index);
Request power_request(const Control &control);
// The value on the control's own steps, within its range.
double snapped(const Control &control, double value);
// "55%", "Off", "21.5°", "Warm white, 2700 K": a level being set, on the control's steps.
std::string value_words(const Control &control, double value);
// The same for the level a thing reports, as it is ("73%", not the step's "75%").
std::string reading_words(const Control &control, double value);
// The services a sheet or a press may call on a room's things, by domain.
bool control_service(const std::string &domain, const std::string &service);
// The room's main light: the one it names, or its first light.
std::string room_light(const RoomPage &room);
// The room's scenes as a row (none without any), with the one in force chosen. Its options carry
// their own calls.
std::optional<Control> scene_row(const RoomPage &room, const StateTable &states);
// A thing's quick buttons as a row.
std::optional<Control> boost_row(const std::string &entity, const std::vector<NamedCall> &boosts);
// The sheet L2 opens: the whole-house actions, then each mode as a switch.
std::vector<Control> house_rows(const HouseControls &house, const StateTable &states);
// "Cinema and Away on" ("" for none).
std::string modes_words(const HouseControls &house, const StateTable &states);
} // namespace lamplight

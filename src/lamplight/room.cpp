// Lamplight for PS5 - The room page's things: what pressing one does, and how it reads.
// Copyright (C) 2026 Hayden Griffin
// SPDX-License-Identifier: GPL-3.0-or-later
#include "lamplight/room.hpp"
#include "lamplight/weather.hpp"
#include <cmath>

namespace lamplight
{
std::optional<Request> press_action(const std::string &entity)
{
    const auto domain = domain_of(entity);
    if (domain == "light" || domain == "switch" || domain == "fan" || domain == "input_boolean")
        return service(domain, "toggle", Json::object(), entity);
    if (domain == "scene" || domain == "script")
        return service(domain, "turn_on", Json::object(), entity);
    if (domain == "media_player")
        return service(domain, "media_play_pause", Json::object(), entity);
    if (domain == "cover")
        return service(domain, "toggle", Json::object(), entity);
    return std::nullopt;
}
bool is_on(const std::string &entity, const Entity &state)
{
    const auto domain = domain_of(entity);
    if (domain == "media_player")
        return state.state == "playing";
    if (domain == "cover")
        return state.state == "open" || state.state == "opening";
    if (domain == "climate")
        return text(state.attributes, "hvac_action") == "heating";
    if (domain == "sensor")
        return false;
    return state.state == "on";
}
std::string state_words(const std::string &entity, const Entity &state)
{
    const auto domain = domain_of(entity);
    if (!state.available())
        return "Unavailable";
    if (domain == "light" && state.state == "on" &&
        has_number(field(state.attributes, "brightness")))
        return std::to_string(
                   std::lround(number(field(state.attributes, "brightness")) * 100 / 255)) +
               "%";
    if (domain == "climate")
    {
        const double now = number(field(state.attributes, "current_temperature"));
        return std::isfinite(now) ? degrees(now) : "Heating off";
    }
    if (domain == "media_player")
        return state.state == "playing"  ? "Playing"
               : state.state == "paused" ? "Paused"
                                         : "Stopped";
    if (domain == "cover")
        return state.state == "open" ? "Open" : state.state == "closed" ? "Closed" : "Moving";
    if (domain == "script")
        return state.state == "on" ? "Running" : "Ready";
    if (domain == "scene")
        return "Scene";
    if (domain == "light" || domain == "switch" || domain == "fan" || domain == "input_boolean" ||
        domain == "binary_sensor")
        return state.state == "on" ? "On" : "Off";
    // A sensor (or anything else) says its own state, with its unit.
    const auto unit = text(state.attributes, "unit_of_measurement");
    auto words = state.state;
    if (!words.empty() && words[0] >= 'a' && words[0] <= 'z')
        words[0] = static_cast<char>(words[0] - 'a' + 'A');
    return unit.empty() ? words : words + (unit == "%" || unit == "\xC2\xB0" ? "" : " ") + unit;
}
const char *default_icon(const std::string &entity)
{
    const auto domain = domain_of(entity);
    if (domain == "light")
        return "lightbulb-outline";
    if (domain == "media_player")
        return "speaker";
    if (domain == "climate")
        return "radiator";
    if (domain == "cover")
        return "blinds";
    if (domain == "fan")
        return "fan";
    if (domain == "script" || domain == "scene")
        return "play";
    return "power";
}
} // namespace lamplight

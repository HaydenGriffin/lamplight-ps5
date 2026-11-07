// Lamplight for PS5 - The room page's things: what pressing one does, and how it reads.
// Copyright (C) 2026 Hayden Griffin
// SPDX-License-Identifier: GPL-3.0-or-later
#include "lamplight/room.hpp"
#include "lamplight/weather.hpp"
#include <cmath>
#include <cstdio>
#include <cstdlib>

namespace lamplight
{
namespace
{
// A sensor's state as a number, or NaN for words ("unknown", "on").
double reading_of(const Entity &state)
{
    char *end = nullptr;
    const double value = std::strtod(state.state.c_str(), &end);
    return !state.state.empty() && end && *end == '\0' ? value : std::nan("");
}
// To a tenth at most: "16.5", "75".
std::string tenths(double value)
{
    const double rounded = std::round(value * 10) / 10;
    char digits[32];
    std::snprintf(digits, sizeof digits, rounded == std::floor(rounded) ? "%.0f" : "%.1f", rounded);
    return digits;
}
} // namespace

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
    if (domain == "vacuum")
        return state.state == "cleaning";
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
        const double target = number(field(state.attributes, "temperature"));
        if (!std::isfinite(now))
            return "Heating off";
        return std::isfinite(target) && state.state != "off"
                   ? degrees(now) + " \xC2\xB7 Set " + degrees(target)
                   : degrees(now);
    }
    if (domain == "fan" && state.state == "on" && has_number(field(state.attributes, "percentage")))
        return std::to_string(std::lround(number(field(state.attributes, "percentage")))) + "%";
    if (domain == "media_player")
        return state.state == "playing"  ? "Playing"
               : state.state == "paused" ? "Paused"
                                         : "Stopped";
    if (domain == "cover")
    {
        // Partly open says how far.
        const double at = number(field(state.attributes, "current_position"));
        if (state.state == "open" && std::isfinite(at) && at < 100)
            return std::to_string(std::lround(at)) + "% open";
        return state.state == "open" ? "Open" : state.state == "closed" ? "Closed" : "Moving";
    }
    if (domain == "script")
        return state.state == "on" ? "Running" : "Ready";
    if (domain == "scene")
        return "Scene";
    if (domain == "light" || domain == "switch" || domain == "fan" || domain == "input_boolean" ||
        domain == "binary_sensor")
        return state.state == "on" ? "On" : "Off";
    // A sensor (or anything else) says its own state, with its unit. A temperature or a percentage
    // reads to a tenth at most: "16.5 °C", not the 16.53 a sensor reports.
    const auto unit = text(state.attributes, "unit_of_measurement");
    auto words = state.state;
    if (const double reading = reading_of(state);
        std::isfinite(reading) && (unit == "%" || unit.starts_with("\xC2\xB0")))
        words = tenths(reading);
    if (!words.empty() && words[0] >= 'a' && words[0] <= 'z')
        words[0] = static_cast<char>(words[0] - 'a' + 'A');
    return unit.empty() ? words : words + (unit == "%" || unit == "\xC2\xB0" ? "" : " ") + unit;
}
bool plan_runs(const std::string &entity, const std::vector<NamedCall> &boosts)
{
    return domain_of(entity) == "climate" && !boosts.empty();
}
std::string thing_words(const std::string &entity, const Entity &state, const Entity &sensor,
                        bool plan)
{
    if (domain_of(entity) != "climate" || !state.available())
        return state_words(entity, state);
    const double room = sensor.available() ? reading_of(sensor) : std::nan("");
    if (!std::isfinite(room))
    {
        const double head = number(field(state.attributes, "current_temperature"));
        return plan && std::isfinite(head) ? degrees(head) : state_words(entity, state);
    }
    const double target = number(field(state.attributes, "temperature"));
    const auto words = tenths(room) + "\xC2\xB0";
    return !plan && std::isfinite(target) && state.state != "off"
               ? words + " \xC2\xB7 Set " + degrees(target)
               : words;
}
bool can_glow(const std::string &entity)
{
    const auto domain = domain_of(entity);
    return domain != "script" && domain != "scene" && domain != "sensor" && domain != "weather" &&
           domain != "number" && domain != "input_number" && domain != "select" &&
           domain != "input_select" && domain != "lock" && domain != "camera";
}
const char *default_icon(const std::string &entity, const Entity &state)
{
    const auto domain = domain_of(entity);
    if (domain == "sensor" || domain == "binary_sensor")
    {
        const auto kind = text(state.attributes, "device_class");
        return kind == "temperature" ? "thermometer"
               : kind == "humidity"  ? "water-percent"
               : kind == "door"      ? "door-open"
                                     : "information-outline";
    }
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
    if (domain == "lock")
        return "key-variant";
    if (domain == "vacuum")
        return "robot-outline";
    if (domain == "camera")
        return "cctv";
    if (domain == "humidifier")
        return "water-percent";
    if (domain == "number" || domain == "input_number" || domain == "select" ||
        domain == "input_select")
        return "cog-outline";
    return "power";
}
} // namespace lamplight

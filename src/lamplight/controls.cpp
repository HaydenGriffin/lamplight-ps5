// Lamplight for PS5 - What can be set on a thing, and the service call that sets it.
// Copyright (C) 2026 Hayden Griffin
// SPDX-License-Identifier: GPL-3.0-or-later
#include "lamplight/controls.hpp"
#include "lamplight/timers.hpp"
#include "lamplight/weather.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <utility>

namespace lamplight
{
namespace
{
constexpr std::size_t kMaxLamps = 12;
// Home Assistant's ClimateEntityFeature.
constexpr int kClimateTarget = 1, kClimateRange = 2;
struct Swatch
{
    int hue, saturation;
    const char *name;
    std::uint32_t colour;
};
constexpr std::array<Swatch, 9> kSwatches{{{0, 0, "White", 0xfff4e5},
                                           {0, 100, "Red", 0xff453a},
                                           {30, 100, "Orange", 0xff9f0a},
                                           {52, 100, "Yellow", 0xffd60a},
                                           {120, 100, "Green", 0x30d158},
                                           {175, 100, "Teal", 0x40c8e0},
                                           {230, 100, "Blue", 0x0a84ff},
                                           {275, 100, "Purple", 0xbf5af2},
                                           {325, 100, "Pink", 0xff375f}}};
// "fan_only" as "Fan only".
std::string mode_words(std::string value)
{
    std::replace(value.begin(), value.end(), '_', ' ');
    return capitalised(std::move(value));
}
double number_or(const Json &attributes, const char *key, double fallback)
{
    const double value = number(field(attributes, key));
    return std::isnan(value) ? fallback : value;
}
// Home Assistant refuses a command the thing doesn't support (Stop on most garage doors). With no
// list of features, everything is offered.
bool supports(const Json &attributes, int feature)
{
    return (static_cast<int>(number_or(attributes, "supported_features", -1)) & feature) != 0;
}
bool has(const std::vector<std::string> &list, const std::string &value)
{
    return std::find(list.begin(), list.end(), value) != list.end();
}
Control toggle_row(const std::string &entity, const char *id, const std::string &label, bool on)
{
    Control c;
    c.kind = Control::Kind::power;
    c.entity = entity;
    c.id = id;
    c.label = label;
    c.on = on;
    return c;
}
Control power_row(const std::string &entity, const std::string &label, bool on)
{
    return toggle_row(entity, "power", label, on);
}
Control slider_row(const std::string &entity, const char *id, const std::string &label, double min,
                   double max, double step, double value)
{
    Control c;
    c.kind = Control::Kind::slider;
    c.entity = entity;
    c.id = id;
    c.label = label;
    c.min = min;
    c.max = max;
    c.step = step > 0 ? step : 1;
    c.value = std::clamp(value, min, std::max(min, max));
    return c;
}
// A list of a thing's own words (modes, presets, sources), with the current one chosen. Nothing
// when it has no list.
void add_choices(std::vector<Control> &rows, const std::string &entity, const char *id,
                 const std::string &label, const std::vector<std::string> &values,
                 const std::string &current)
{
    if (values.empty())
        return;
    Control c;
    c.kind = Control::Kind::options;
    c.entity = entity;
    c.id = id;
    c.label = label;
    for (const auto &v : values)
    {
        if (v == current)
            c.selected = static_cast<int>(c.options.size());
        c.options.push_back({v, mode_words(v)});
    }
    rows.push_back(std::move(c));
}
void add_buttons(std::vector<Control> &rows, const std::string &entity, const char *id,
                 std::vector<ControlOption> buttons)
{
    if (buttons.empty())
        return;
    Control c;
    c.kind = Control::Kind::buttons;
    c.entity = entity;
    c.id = id;
    c.label = "";
    c.options = std::move(buttons);
    rows.push_back(std::move(c));
}
Control chart_row(const std::string &entity, const char *attribute, const std::string &unit,
                  double target)
{
    Control c;
    c.kind = Control::Kind::chart;
    c.entity = entity;
    c.id = "history";
    c.label = "Last 24 hours";
    c.attribute = attribute;
    c.unit = unit;
    c.target = target;
    return c;
}
bool dims(const Entity &state)
{
    const auto modes = strings(field(state.attributes, "supported_color_modes"));
    if (modes.empty())
        return has_number(field(state.attributes, "brightness"));
    return std::any_of(modes.begin(), modes.end(), [](const auto &m) { return m != "onoff"; });
}
double brightness_percent(const Entity &state)
{
    if (state.state != "on")
        return 0;
    const double raw = number(field(state.attributes, "brightness"));
    return std::isnan(raw) ? 100 : std::round(raw * 100 / 255);
}
int nearest_swatch(const Entity &state)
{
    const auto &hs = field(state.attributes, "hs_color");
    if (!hs.is_array() || hs.size() < 2 || !hs[0].is_number() || !hs[1].is_number())
        return -1;
    const auto mode = text(state.attributes, "color_mode");
    if (mode == "color_temp" || mode == "white")
        return -1;
    const double hue = hs[0].get<double>(), saturation = hs[1].get<double>();
    if (saturation < 25)
        return 0;
    int best = -1;
    double nearest = 400;
    for (std::size_t i = 1; i < kSwatches.size(); ++i)
    {
        const double apart = std::fabs(hue - kSwatches[i].hue);
        const double around = std::min(apart, 360 - apart);
        if (around < nearest)
        {
            nearest = around;
            best = static_cast<int>(i);
        }
    }
    return best;
}
void add_light(std::vector<Control> &rows, const std::string &entity, const Entity &state)
{
    const auto &a = state.attributes;
    const auto modes = strings(field(a, "supported_color_modes"));
    rows.push_back(power_row(entity, "Power", state.state == "on"));
    if (dims(state))
        rows.push_back(
            slider_row(entity, "brightness", "Brightness", 0, 100, 5, brightness_percent(state)));
    if (has(modes, "color_temp"))
    {
        const double low = number_or(a, "min_color_temp_kelvin", 2000);
        const double high = number_or(a, "max_color_temp_kelvin", 6500);
        rows.push_back(slider_row(entity, "warmth", "Warmth", low, high, 100,
                                  number_or(a, "color_temp_kelvin", (low + high) / 2)));
    }
    const bool colour = std::any_of(modes.begin(), modes.end(), [](const auto &m)
                                    { return m == "hs" || m == "xy" || m.starts_with("rgb"); });
    if (colour)
    {
        Control c;
        c.kind = Control::Kind::swatches;
        c.entity = entity;
        c.id = "colour";
        c.label = "Colour";
        for (const auto &s : kSwatches)
            c.options.push_back(
                {std::to_string(s.hue) + "," + std::to_string(s.saturation), s.name, s.colour});
        c.selected = nearest_swatch(state);
        rows.push_back(std::move(c));
    }
    add_choices(rows, entity, "effect", "Effect", strings(field(a, "effect_list")),
                text(a, "effect"));
}
void add_climate(std::vector<Control> &rows, const std::string &entity, const Entity &state)
{
    const auto &a = state.attributes;
    const double low = number_or(a, "min_temp", 7), high = number_or(a, "max_temp", 35);
    const double step = number_or(a, "target_temp_step", 0.5);
    const double current = number_or(a, "current_temperature", (low + high) / 2);
    const auto features = static_cast<int>(number_or(a, "supported_features", kClimateTarget));
    const bool range_now = has_number(field(a, "target_temp_low"));
    if (has_number(field(a, "current_temperature")))
        rows.push_back(chart_row(entity, "current_temperature", "\xC2\xB0",
                                 state.state == "off" || !has_number(field(a, "temperature"))
                                     ? std::nan("")
                                     : number(field(a, "temperature"))));
    // Home Assistant refuses a single target from a thermostat that only takes a range. One that
    // takes both uses whichever its mode has now.
    if ((features & kClimateRange) && (range_now || !(features & kClimateTarget)))
    {
        const double from = number_or(a, "target_temp_low", current);
        const double to = number_or(a, "target_temp_high", current);
        rows.push_back(slider_row(entity, "temp_low", "Heat to", low, high, step, from));
        rows.back().other_end = to;
        rows.push_back(slider_row(entity, "temp_high", "Cool to", low, high, step, to));
        rows.back().other_end = from;
    }
    else if (features & kClimateTarget)
        rows.push_back(slider_row(entity, "temperature", "Target temperature", low, high, step,
                                  number_or(a, "temperature", current)));
    add_choices(rows, entity, "hvac", "Mode", strings(field(a, "hvac_modes")), state.state);
    add_choices(rows, entity, "preset", "Preset", strings(field(a, "preset_modes")),
                text(a, "preset_mode"));
    add_choices(rows, entity, "fan_mode", "Fan", strings(field(a, "fan_modes")),
                text(a, "fan_mode"));
}
void add_cover(std::vector<Control> &rows, const std::string &entity, const Entity &state)
{
    const auto &a = state.attributes;
    // A position can be reported without being settable (Home Assistant would refuse it).
    if (has_number(field(a, "current_position")) && supports(a, 4))
        rows.push_back(slider_row(entity, "position", "Position", 0, 100, 5,
                                  number(field(a, "current_position"))));
    if (has_number(field(a, "current_tilt_position")) && supports(a, 128))
        rows.push_back(slider_row(entity, "tilt", "Tilt", 0, 100, 5,
                                  number(field(a, "current_tilt_position"))));
    std::vector<ControlOption> buttons;
    if (supports(a, 1))
        buttons.push_back({"open_cover", "Open"});
    if (supports(a, 8))
        buttons.push_back({"stop_cover", "Stop"});
    if (supports(a, 2))
        buttons.push_back({"close_cover", "Close"});
    add_buttons(rows, entity, "cover", std::move(buttons));
}
void add_fan(std::vector<Control> &rows, const std::string &entity, const Entity &state)
{
    const auto &a = state.attributes;
    rows.push_back(power_row(entity, "Power", state.state == "on"));
    if (has_number(field(a, "percentage")) || has_number(field(a, "percentage_step")))
        rows.push_back(slider_row(entity, "speed", "Speed", 0, 100,
                                  number_or(a, "percentage_step", 10),
                                  number_or(a, "percentage", 0)));
    add_choices(rows, entity, "preset", "Preset", strings(field(a, "preset_modes")),
                text(a, "preset_mode"));
    if (field(a, "oscillating").is_boolean())
        rows.push_back(
            toggle_row(entity, "oscillate", "Oscillate", field(a, "oscillating") == true));
}
void add_media_player(std::vector<Control> &rows, const std::string &entity, const Entity &state)
{
    const auto &a = state.attributes;
    std::vector<ControlOption> buttons;
    if (supports(a, 16))
        buttons.push_back({"media_previous_track", "Previous"});
    if (supports(a, 1 | 16384))
        buttons.push_back({"media_play_pause", state.state == "playing" ? "Pause" : "Play"});
    if (supports(a, 32))
        buttons.push_back({"media_next_track", "Next"});
    add_buttons(rows, entity, "transport", std::move(buttons));
    if (has_number(field(a, "volume_level")))
        rows.push_back(slider_row(entity, "volume", "Volume", 0, 100, 5,
                                  std::round(number(field(a, "volume_level")) * 100)));
    if (field(a, "is_volume_muted").is_boolean())
        rows.push_back(toggle_row(entity, "mute", "Mute", field(a, "is_volume_muted") == true));
    add_choices(rows, entity, "source", "Source", strings(field(a, "source_list")),
                text(a, "source"));
    add_choices(rows, entity, "sound", "Sound", strings(field(a, "sound_mode_list")),
                text(a, "sound_mode"));
    if (field(a, "shuffle").is_boolean())
        rows.push_back(toggle_row(entity, "shuffle", "Shuffle", field(a, "shuffle") == true));
    if (field(a, "repeat").is_string())
        add_choices(rows, entity, "repeat", "Repeat", {"off", "all", "one"}, text(a, "repeat"));
}
void add_humidifier(std::vector<Control> &rows, const std::string &entity, const Entity &state)
{
    const auto &a = state.attributes;
    rows.push_back(power_row(entity, "Power", state.state == "on"));
    const double low = number_or(a, "min_humidity", 0), high = number_or(a, "max_humidity", 100);
    rows.push_back(
        slider_row(entity, "humidity", "Target humidity", low, high, 5,
                   number_or(a, "humidity", number_or(a, "current_humidity", (low + high) / 2))));
    add_choices(rows, entity, "humidifier_mode", "Mode", strings(field(a, "available_modes")),
                text(a, "mode"));
}
void add_number(std::vector<Control> &rows, const std::string &entity, const Entity &state)
{
    const auto &a = state.attributes;
    const double value = number(Json(state.state));
    const double low = number_or(a, "min", 0), high = number_or(a, "max", 100);
    rows.push_back(slider_row(entity, "value", "Value", low, high, number_or(a, "step", 1),
                              std::isnan(value) ? low : value));
}
} // namespace

std::vector<Control> controls_for(const std::string &entity, const Entity &state)
{
    std::vector<Control> rows;
    const auto domain = domain_of(entity);
    if (domain == "light")
        add_light(rows, entity, state);
    else if (domain == "climate")
        add_climate(rows, entity, state);
    else if (domain == "cover")
        add_cover(rows, entity, state);
    else if (domain == "fan")
        add_fan(rows, entity, state);
    else if (domain == "media_player")
        add_media_player(rows, entity, state);
    else if (domain == "humidifier")
        add_humidifier(rows, entity, state);
    else if (domain == "number" || domain == "input_number")
        add_number(rows, entity, state);
    else if (domain == "select" || domain == "input_select")
        add_choices(rows, entity, "option", "Choose", strings(field(state.attributes, "options")),
                    state.state);
    else if (domain == "switch" || domain == "input_boolean")
        rows.push_back(power_row(entity, "Power", state.state == "on"));
    else if (domain == "lock")
    {
        ControlOption lock, unlock;
        lock.value = "lock";
        lock.label = "Lock";
        unlock.value = "unlock";
        unlock.label = "Unlock";
        unlock.asks = true;
        add_buttons(rows, entity, "lock", {lock, unlock});
    }
    else if (domain == "sensor" && has_number(Json(state.state)))
        rows.push_back(
            chart_row(entity, "", text(state.attributes, "unit_of_measurement"), std::nan("")));
    else if (domain == "vacuum")
    {
        const auto &a = state.attributes;
        std::vector<ControlOption> buttons;
        if (supports(a, 8192))
            buttons.push_back({"start", "Start"});
        if (supports(a, 4))
            buttons.push_back({"pause", "Pause"});
        if (supports(a, 16))
            buttons.push_back({"return_to_base", "Go home"});
        add_buttons(rows, entity, "vacuum", std::move(buttons));
    }
    return rows;
}
Control lamp_control(const std::string &entity, const Entity &state, const std::string &name)
{
    if (!dims(state))
        return power_row(entity, name, state.state == "on");
    return slider_row(entity, "brightness", name, 0, 100, 5, brightness_percent(state));
}
std::optional<Control> primary_slider(const std::string &entity, const Entity &state)
{
    for (auto &row : controls_for(entity, state))
        if (row.kind == Control::Kind::slider)
            return std::move(row);
    return std::nullopt;
}
std::vector<std::string> group_members(const std::string &entity, const Entity &state)
{
    std::vector<std::string> members;
    if (domain_of(entity) != "light")
        return members;
    for (auto &id : strings(field(state.attributes, "entity_id")))
        if (domain_of(id) == "light" && id != entity && members.size() < kMaxLamps)
            members.push_back(std::move(id));
    if (members.size() < 2)
        members.clear();
    return members;
}
double snapped(const Control &control, double value)
{
    const double steps = std::round((value - control.min) / control.step);
    const double top = std::max(control.min, control.max);
    const double at = std::clamp(control.min + steps * control.step, control.min, top);
    // The top is always reachable, though the steps may not land on it (33.33 three times).
    return top - at < control.step / 2 ? top : at;
}
std::optional<Request> slider_request(const Control &c, double raw)
{
    const double value = snapped(c, raw);
    const auto whole = std::lround(value);
    const auto domain = domain_of(c.entity);
    if (c.id == "brightness")
    {
        // The bottom of the slider is off, so one control does both.
        if (value <= 0)
            return service("light", "turn_off", Json::object(), c.entity);
        return service("light", "turn_on", {{"brightness_pct", whole}}, c.entity);
    }
    if (c.id == "warmth")
        return service("light", "turn_on", {{"color_temp_kelvin", whole}}, c.entity);
    if (c.id == "temperature")
        return service("climate", "set_temperature", {{"temperature", value}}, c.entity);
    if (c.id == "temp_low" || c.id == "temp_high")
    {
        // Both ends go together, the other pushed along rather than crossed.
        const bool is_low = c.id == "temp_low";
        const double from = is_low ? value : std::min(c.other_end, value);
        const double to = is_low ? std::max(c.other_end, value) : value;
        return service("climate", "set_temperature",
                       {{"target_temp_low", from}, {"target_temp_high", to}}, c.entity);
    }
    if (c.id == "position")
        return service("cover", "set_cover_position", {{"position", whole}}, c.entity);
    if (c.id == "tilt")
        return service("cover", "set_cover_tilt_position", {{"tilt_position", whole}}, c.entity);
    if (c.id == "speed")
    {
        if (value <= 0)
            return service("fan", "turn_off", Json::object(), c.entity);
        // Down, not to the nearest: Home Assistant reads a three-speed fan's 67 as the top speed.
        return service("fan", "set_percentage",
                       {{"percentage", static_cast<long>(std::floor(value + 1e-6))}}, c.entity);
    }
    if (c.id == "volume")
        return service("media_player", "volume_set", {{"volume_level", value / 100}}, c.entity);
    if (c.id == "humidity")
        return service("humidifier", "set_humidity", {{"humidity", whole}}, c.entity);
    if (c.id == "value")
        return service(domain, "set_value", {{"value", value}}, c.entity);
    return std::nullopt;
}
std::optional<Request> option_request(const Control &c, int index)
{
    if (index < 0 || index >= static_cast<int>(c.options.size()))
        return std::nullopt;
    const auto &value = c.options[static_cast<std::size_t>(index)].value;
    if (const auto &press = c.options[static_cast<std::size_t>(index)].press)
        return press;
    const auto domain = domain_of(c.entity);
    if (c.id == "colour")
    {
        const auto comma = value.find(',');
        if (comma == std::string::npos)
            return std::nullopt;
        const int hue = std::atoi(value.c_str()), saturation = std::atoi(value.c_str() + comma + 1);
        return service("light", "turn_on", {{"hs_color", Json::array({hue, saturation})}},
                       c.entity);
    }
    if (c.id == "effect")
        return service("light", "turn_on", {{"effect", value}}, c.entity);
    if (c.id == "hvac")
        return service("climate", "set_hvac_mode", {{"hvac_mode", value}}, c.entity);
    if (c.id == "preset")
        return service(domain, "set_preset_mode", {{"preset_mode", value}}, c.entity);
    if (c.id == "fan_mode")
        return service("climate", "set_fan_mode", {{"fan_mode", value}}, c.entity);
    if (c.id == "source")
        return service("media_player", "select_source", {{"source", value}}, c.entity);
    if (c.id == "sound")
        return service("media_player", "select_sound_mode", {{"sound_mode", value}}, c.entity);
    if (c.id == "repeat")
        return service("media_player", "repeat_set", {{"repeat", value}}, c.entity);
    if (c.id == "humidifier_mode")
        return service("humidifier", "set_mode", {{"mode", value}}, c.entity);
    if (c.id == "option")
        return service(domain, "select_option", {{"option", value}}, c.entity);
    // Buttons name their own service.
    if (c.id == "cover" || c.id == "transport" || c.id == "lock" || c.id == "vacuum")
        return service(domain, value, Json::object(), c.entity);
    return std::nullopt;
}
Request power_request(const Control &c)
{
    if (c.press)
        return *c.press;
    if (c.id == "mute")
        return service("media_player", "volume_mute", {{"is_volume_muted", !c.on}}, c.entity);
    if (c.id == "shuffle")
        return service("media_player", "shuffle_set", {{"shuffle", !c.on}}, c.entity);
    if (c.id == "oscillate")
        return service("fan", "oscillate", {{"oscillating", !c.on}}, c.entity);
    return service(domain_of(c.entity), "toggle", Json::object(), c.entity);
}
std::string value_words(const Control &c, double raw)
{
    return reading_words(c, snapped(c, raw));
}
std::string reading_words(const Control &c, double value)
{
    const auto whole = std::to_string(std::lround(value));
    if (c.id == "brightness" || c.id == "all")
        return value <= 0 ? "Off" : std::to_string(std::max(1L, std::lround(value))) + "%";
    if (c.id == "warmth")
        return std::string(value < 3300   ? "Warm white"
                           : value < 4800 ? "Neutral white"
                                          : "Cool white") +
               ", " + std::to_string(std::lround(value / 10) * 10) + " K";
    if (c.id == "temperature" || c.id == "temp_low" || c.id == "temp_high")
    {
        const double tenths = std::round(value * 10) / 10;
        char text[16];
        std::snprintf(text, sizeof text, tenths == std::floor(tenths) ? "%.0f" : "%.1f", tenths);
        return std::string(text) + "\xC2\xB0";
    }
    if (c.id == "position" || c.id == "tilt" || c.id == "speed" || c.id == "volume" ||
        c.id == "humidity")
        return whole + "%";
    char text[32];
    std::snprintf(text, sizeof text, "%g", value);
    return text;
}
bool control_service(const std::string &domain, const std::string &name)
{
    auto in = [&](std::initializer_list<const char *> names)
    { return std::any_of(names.begin(), names.end(), [&](const char *n) { return name == n; }); };
    if (domain == "light" || domain == "switch" || domain == "input_boolean")
        return in({"toggle", "turn_on", "turn_off"});
    if (domain == "scene" || domain == "script")
        return in({"turn_on"});
    if (domain == "fan")
        return in(
            {"toggle", "turn_on", "turn_off", "set_percentage", "set_preset_mode", "oscillate"});
    if (domain == "cover")
        return in({"toggle", "open_cover", "close_cover", "stop_cover", "set_cover_position",
                   "set_cover_tilt_position"});
    if (domain == "climate")
        return in({"set_temperature", "set_hvac_mode", "set_preset_mode", "set_fan_mode"});
    if (domain == "media_player")
        return in({"media_play_pause", "media_next_track", "media_previous_track", "volume_set",
                   "volume_mute", "select_source", "select_sound_mode", "shuffle_set", "repeat_set",
                   "join", "unjoin"});
    if (domain == "humidifier")
        return in({"toggle", "turn_on", "turn_off", "set_humidity", "set_mode"});
    if (domain == "number" || domain == "input_number")
        return in({"set_value"});
    if (domain == "select" || domain == "input_select")
        return in({"select_option"});
    if (domain == "lock")
        return in({"lock", "unlock"});
    if (domain == "vacuum")
        return in({"start", "pause", "return_to_base"});
    return false;
}
std::string room_light(const RoomPage &room)
{
    if (!room.light.empty())
        return room.light;
    for (const auto &tile : room.tiles)
        if (domain_of(tile.entity) == "light")
            return tile.entity;
    return "";
}
namespace
{
// The scene in force, as the tablets read it: a mode that is on, the Off scene with the light off,
// or one within 10% of the light's level and on the same side of warm and cool.
int active_scene(const RoomPage &room, const StateTable &states)
{
    const auto &scenes = room.scenes;
    // The lamps off is Off, whatever mode the house is in: a lit pill means lamps are on (The
    // Lamp Rule), so Cinema doesn't stay warm over a dark room.
    const auto &light = states.get(room_light(room));
    if (light.state != "on")
    {
        for (std::size_t i = 0; i < scenes.size(); ++i)
            if (scenes[i].off)
                return static_cast<int>(i);
        return -1;
    }
    for (std::size_t i = 0; i < scenes.size(); ++i)
        if (!scenes[i].entity.empty() && states.get(scenes[i].entity).state == "on")
            return static_cast<int>(i);
    const double level = brightness_percent(light);
    const double kelvin = number(field(light.attributes, "color_temp_kelvin"));
    for (std::size_t i = 0; i < scenes.size(); ++i)
    {
        const auto &s = scenes[i];
        if (!s.entity.empty() || s.off || !s.brightness || std::fabs(level - s.brightness) > 10)
            continue;
        if (!s.kelvin || std::isnan(kelvin) || (s.kelvin >= 3300) == (kelvin >= 3300))
            return static_cast<int>(i);
    }
    return -1;
}
Request scene_request(const RoomScene &scene, const std::string &light)
{
    if (!scene.call.empty())
        return call_request(scene.call);
    if (scene.off)
        return service("light", "turn_off", Json::object(), light);
    auto data = Json::object();
    if (scene.brightness)
        data["brightness_pct"] = scene.brightness;
    if (scene.kelvin)
        data["color_temp_kelvin"] = scene.kelvin;
    return service("light", "turn_on", std::move(data), light);
}
} // namespace
std::optional<Control> scene_row(const RoomPage &room, const StateTable &states)
{
    const auto light = room_light(room);
    if (room.scenes.empty() || light.empty())
        return std::nullopt;
    Control c;
    c.kind = Control::Kind::options;
    c.entity = light;
    c.id = "scene";
    c.label = "Scene";
    for (const auto &scene : room.scenes)
    {
        ControlOption option;
        option.value = scene.off ? "off" : scene.name;
        option.label = scene.name;
        option.icon = scene.icon;
        option.press = scene_request(scene, light);
        c.options.push_back(std::move(option));
    }
    c.selected = active_scene(room, states);
    return c;
}
std::optional<Control> boost_row(const std::string &entity, const std::vector<NamedCall> &boosts)
{
    if (boosts.empty())
        return std::nullopt;
    Control c;
    c.kind = Control::Kind::buttons;
    c.entity = entity;
    c.id = "boost";
    c.heading = domain_of(entity) == "climate" ? "Warm up" : "Quick";
    for (const auto &boost : boosts)
    {
        ControlOption option;
        option.value = boost.name;
        option.label = boost.name;
        option.icon = boost.icon;
        option.asks = boost.confirm;
        option.done = boost.name;
        option.press = call_request(boost.call);
        c.options.push_back(std::move(option));
    }
    return c;
}
std::vector<Control> house_rows(const HouseControls &house, const StateTable &states)
{
    std::vector<Control> rows;
    if (!house.actions.empty())
    {
        Control c;
        c.kind = Control::Kind::buttons;
        c.id = "house";
        for (const auto &action : house.actions)
        {
            ControlOption option;
            option.value = action.name;
            option.label = action.name;
            option.icon = action.icon;
            option.asks = action.confirm;
            option.done = action.name;
            option.press = call_request(action.call);
            option.undo_lights = action.undo_lights && !house.undo_lights.empty();
            option.closes = true;
            c.options.push_back(std::move(option));
        }
        rows.push_back(std::move(c));
    }
    std::string heading = "Modes";
    for (const auto &mode : house.modes)
    {
        const auto &state = states.get(mode.entity);
        if (!state.available())
            continue;
        auto c = toggle_row(mode.entity, "mode", mode.name, state.state == "on");
        c.detail = mode.about;
        c.icon = mode.icon;
        c.heading = std::exchange(heading, "");
        c.asks = c.on && mode.confirm_off;
        c.press_done = mode.name + (c.on ? " off" : " on");
        if (const auto &via = c.on ? mode.off : mode.on; !via.empty())
            c.press = call_request(via);
        rows.push_back(std::move(c));
    }
    return rows;
}
std::string modes_words(const HouseControls &house, const StateTable &states)
{
    std::vector<std::string> on;
    for (const auto &mode : house.modes)
        if (states.get(mode.entity).state == "on")
            on.push_back(mode.name);
    if (on.empty())
        return "";
    std::string words = on[0];
    for (std::size_t i = 1; i < on.size(); ++i)
        words += (i + 1 == on.size() ? " and " : ", ") + on[i];
    return words + " on";
}
} // namespace lamplight

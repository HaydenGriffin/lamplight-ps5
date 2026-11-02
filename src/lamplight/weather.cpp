// Lamplight for PS5 - Forecasts from weather.get_forecasts and how a condition looks.
// Copyright (C) 2026 Hayden Griffin
// SPDX-License-Identifier: GPL-3.0-or-later
#include "lamplight/weather.hpp"
#include "lamplight/clock.hpp"
#include <cmath>
#include <map>

namespace lamplight
{
namespace
{
const Json &forecast_list(const Json &response, const std::string &entity)
{
    return field(field(response, entity), "forecast");
}
} // namespace
bool parse_hours(const Json &response, const std::string &entity, const Zone &zone,
                 std::vector<Hour> &out)
{
    const auto &list = forecast_list(response, entity);
    if (!list.is_array())
        return false;
    out.clear();
    for (const auto &h : list)
    {
        const auto at = parse_iso_ms(text(h, "datetime"), zone);
        const double t = number(field(h, "temperature"));
        if (at == kNoTime || !std::isfinite(t))
            continue;
        const double rain = number(field(h, "precipitation"));
        out.push_back({at, t, std::isfinite(rain) ? rain : 0, text(h, "condition")});
    }
    return true;
}
bool parse_days(const Json &response, const std::string &entity, const Zone &zone,
                std::vector<Day> &out)
{
    const auto &list = forecast_list(response, entity);
    if (!list.is_array())
        return false;
    out.clear();
    for (const auto &d : list)
    {
        const auto at = parse_iso_ms(text(d, "datetime"), zone);
        const double high = number(field(d, "temperature"));
        if (at == kNoTime || !std::isfinite(high))
            continue;
        const double low = number(field(d, "templow"));
        const double rain = number(field(d, "precipitation"));
        out.push_back({at, high, std::isfinite(low) ? low : high, std::isfinite(rain) ? rain : 0,
                       text(d, "condition")});
    }
    return true;
}
std::vector<Hour> next_hours(const std::vector<Hour> &hours, std::int64_t now_ms)
{
    std::vector<Hour> out;
    for (const auto &h : hours)
        if (h.at >= now_ms - 30 * 60000 && out.size() < 12)
            out.push_back(h);
    return out;
}
const char *condition_icon(const std::string &c)
{
    static const std::map<std::string, const char *> icons{
        {"clear-night", "weather-night"},
        {"cloudy", "weather-cloudy"},
        {"exceptional", "alert-circle-outline"},
        {"fog", "weather-fog"},
        {"hail", "weather-hail"},
        {"lightning", "weather-lightning"},
        {"lightning-rainy", "weather-lightning-rainy"},
        {"partlycloudy", "weather-partly-cloudy"},
        {"pouring", "weather-pouring"},
        {"rainy", "weather-rainy"},
        {"snowy", "weather-snowy"},
        {"snowy-rainy", "weather-snowy-rainy"},
        {"sunny", "weather-sunny"},
        {"windy", "weather-windy"},
        {"windy-variant", "weather-windy-variant"}};
    auto it = icons.find(c);
    return it == icons.end() ? "weather-cloudy" : it->second;
}
std::string condition_label(const std::string &c)
{
    static const std::map<std::string, std::string> labels{{"clear-night", "Clear"},
                                                           {"partlycloudy", "Partly cloudy"},
                                                           {"lightning-rainy", "Thunderstorms"},
                                                           {"snowy-rainy", "Sleet"},
                                                           {"windy-variant", "Windy"},
                                                           {"pouring", "Heavy rain"}};
    if (auto it = labels.find(c); it != labels.end())
        return it->second;
    std::string out = c;
    if (!out.empty() && out[0] >= 'a' && out[0] <= 'z')
        out[0] = static_cast<char>(out[0] - 'a' + 'A');
    return out;
}
const char *compass(double degrees)
{
    static const char *const points[] = {"N", "NE", "E", "SE", "S", "SW", "W", "NW"};
    if (!std::isfinite(degrees))
        degrees = 0;
    const long step = std::lround(std::fmod(std::fmod(degrees, 360.0) + 360.0, 360.0) / 45.0);
    return points[step % 8];
}
Sky sky_for(const std::string &c, bool daytime)
{
    if (!daytime)
        return Sky::night;
    if (c == "rainy" || c == "pouring" || c == "lightning-rainy" || c == "lightning" ||
        c == "hail" || c == "snowy-rainy")
        return Sky::rain;
    if (c == "cloudy" || c == "fog" || c == "snowy")
        return Sky::grey;
    return Sky::day;
}
long rounded(double value)
{
    return std::isfinite(value) ? std::lround(value) : 0;
}
std::string degrees(double value)
{
    if (!std::isfinite(value))
        return "\xE2\x80\x93";
    long v = rounded(value);
    // "−0°" reads as a mistake.
    if (v == 0)
        v = 0;
    return std::to_string(v) + "\xC2\xB0";
}
std::string one_decimal(double value)
{
    if (!std::isfinite(value))
        return "";
    const long tenths = std::lround(value * 10);
    const long whole = std::labs(tenths) / 10, part = std::labs(tenths) % 10;
    return std::string(tenths < 0 ? "-" : "") + std::to_string(whole) + "." + std::to_string(part);
}
} // namespace lamplight

// Lamplight for PS5 - Forecasts from weather.get_forecasts and how a condition looks.
// Copyright (C) 2026 Hayden Griffin
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "lamplight/clock.hpp"
#include "lamplight/json.hpp"
#include <cstdint>

namespace lamplight
{
struct Hour
{
    std::int64_t at = 0;
    double temperature = 0, rain = 0;
    std::string condition;
};
struct Day
{
    std::int64_t at = 0;
    double high = 0, low = 0, rain = 0;
    std::string condition;
};
// The forecast list for entity in a get_forecasts response; false when it has none.
bool parse_hours(const Json &response, const std::string &entity, const Zone &zone,
                 std::vector<Hour> &out);
bool parse_days(const Json &response, const std::string &entity, const Zone &zone,
                std::vector<Day> &out);
// The next twelve hours from now (an hour that began up to half an hour ago counts).
std::vector<Hour> next_hours(const std::vector<Hour> &hours, std::int64_t now_ms);
const char *condition_icon(const std::string &condition);
std::string condition_label(const std::string &condition);
const char *compass(double degrees);
enum class Sky
{
    day,
    grey,
    rain,
    night
};
Sky sky_for(const std::string &condition, bool daytime);
// A whole number of degrees, "−" for a missing value. Rounds half away from zero.
std::string degrees(double value);
long rounded(double value);
// One decimal place ("1.4") without printf, which the console's libc does not do for doubles.
std::string one_decimal(double value);
} // namespace lamplight

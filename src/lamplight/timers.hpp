// Lamplight for PS5 - View Assist timers and appliance timers, and how long is left in words.
// Copyright (C) 2026 Hayden Griffin
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "lamplight/clock.hpp"
#include "lamplight/config.hpp"
#include "lamplight/states.hpp"
#include <cstdint>

namespace lamplight
{
struct Timer
{
    std::string id, name, kind; // kind: timer, alarm or reminder
    std::int64_t at = 0;        // expiry, ms; 0 when paused without one
    bool paused = false;
    double left = -1;  // seconds left (paused: the time it had left); -1 when unknown
    double total = -1; // seconds from set to expiry, for the ring; -1 when unknown
};
// From the View Assist sensor's "timers" attribute: soonest first, paused ones last, expired
// ones gone. A paused timer's time left is "remaining" when given, else expiry less the pause.
std::vector<Timer> view_assist_timers(const Entity &sensor, std::int64_t now_ms, const Zone &zone);
struct ApplianceTimer
{
    Choice choice;
    std::int64_t at = 0;
};
// The appliance timers that are running: state "active" with a finishes_at.
std::vector<ApplianceTimer> appliance_timers(const std::vector<Choice> &choices,
                                             const StateTable &states, const Zone &zone);
// Time left as number and unit pairs, so it can't be read as a time of day:
// {1,"h"},{05,"min"} · {17,"min"},{39,"s"} · {45,"s"}.
std::vector<std::pair<std::string, std::string>> left_parts(double seconds);
std::string left_text(double seconds); // "17 min 39 s"
std::string capitalised(std::string s);
// view_assist.set_timer's time for Undo: the time it had left, at least 30 seconds, in hours,
// minutes and seconds ("1 minute 47 seconds"). View Assist sets nothing for "107 seconds".
std::string undo_time(double left_seconds);
} // namespace lamplight

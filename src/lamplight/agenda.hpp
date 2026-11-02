// Lamplight for PS5 - Calendar events from calendar.get_events, sorted into days.
// Copyright (C) 2026 Hayden Griffin
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "lamplight/clock.hpp"
#include "lamplight/json.hpp"
#include <cstdint>

namespace lamplight
{
struct Event
{
    std::string calendar, summary, place;
    std::int64_t start = 0, end = 0;
    bool all_day = false, cancelled = false;
};
// The events for entity in a get_events response, appended to out; false when it has none.
bool parse_events(const Json &response, const std::string &entity, const Zone &zone,
                  std::vector<Event> &out);
// A location short enough for one line: "Teams", "Zoom", or up to the first comma.
std::string short_place(const std::string &location);
struct DayEvents
{
    std::vector<Event> all_day, timed;
    bool empty() const
    {
        return all_day.empty() && timed.empty();
    }
};
// Everything that overlaps [day_start, day_end): all-day items by name, then by time.
DayEvents day_events(const std::vector<Event> &events, std::int64_t day_start_ms,
                     std::int64_t day_end_ms);
} // namespace lamplight

// Lamplight for PS5 - The house's notices: what Home Assistant says needs attention, newest first.
// Copyright (C) 2026 Hayden Griffin
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "lamplight/clock.hpp"
#include "lamplight/json.hpp"
#include <string>
#include <vector>

namespace lamplight
{
// One notice, from an entity's "items" attribute: {id, key, title, message, icon, level
// ("info", "warn" or "alert"), rooms, at}. Any list of that shape works (a template sensor, say).
struct Notice
{
    std::string id, key, title, message, icon, level;
    std::vector<std::string> rooms;
    std::int64_t at = 0;
};
// The notices in an items attribute, newest first, at most 30; ones without a title are left out.
std::vector<Notice> parse_notices(const Json &items, const Zone &zone);
// "just now", "5 min ago", "3 h ago", or the day and time for anything older than a day.
std::string ago(std::int64_t at, std::int64_t now, const Zone &zone);
} // namespace lamplight

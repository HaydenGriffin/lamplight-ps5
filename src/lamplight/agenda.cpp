// Lamplight for PS5 - Calendar events from calendar.get_events, sorted into days.
// Copyright (C) 2026 Hayden Griffin
// SPDX-License-Identifier: GPL-3.0-or-later
#include "lamplight/agenda.hpp"
#include "lamplight/clock.hpp"
#include <algorithm>

namespace lamplight
{
namespace
{
std::string lower(std::string s)
{
    for (auto &c : s)
        if (c >= 'A' && c <= 'Z')
            c = static_cast<char>(c - 'A' + 'a');
    return s;
}
std::string trim(const std::string &s)
{
    const auto first = s.find_first_not_of(" \t\r\n");
    if (first == std::string::npos)
        return "";
    return s.substr(first, s.find_last_not_of(" \t\r\n") - first + 1);
}
// "Cancelled: Swimming" (also "Canceled:"), as calendars mark a cancelled event: the cards'
// /^\s*(cancel+ed)\s*:\s*/i.
bool strip_cancelled(std::string &summary)
{
    const auto l = lower(summary);
    if (!l.starts_with("cance"))
        return false;
    std::size_t at = 5;
    while (at < l.size() && l[at] == 'l')
        ++at;
    if (at == 5 || l.compare(at, 2, "ed") != 0)
        return false;
    at = l.find_first_not_of(" \t", at + 2);
    if (at == std::string::npos || l[at] != ':')
        return false;
    summary = trim(summary.substr(at + 1));
    return true;
}
std::string when(const Json &value)
{
    if (value.is_string())
        return value.get<std::string>();
    auto t = text(value, "dateTime");
    return t.empty() ? text(value, "date") : t;
}
} // namespace

std::string short_place(const std::string &location)
{
    const auto l = trim(location);
    const auto low = lower(l);
    if (l.empty() || low.starts_with("http://") || low.starts_with("https://"))
        return "";
    if (low.find("teams") != std::string::npos)
        return "Teams";
    if (low.find("zoom.us") != std::string::npos || low.find("zoom meeting") != std::string::npos)
        return "Zoom";
    auto first = l.substr(0, l.find_first_of(",\n"));
    // Forty bytes, without cutting a UTF-8 character in half.
    if (first.size() > 40)
    {
        std::size_t cut = 40;
        while (cut > 0 && (static_cast<unsigned char>(first[cut]) & 0xC0) == 0x80)
            --cut;
        first.resize(cut);
    }
    return trim(first);
}
bool parse_events(const Json &response, const std::string &entity, const Zone &zone,
                  std::vector<Event> &out)
{
    const auto &list = field(field(response, entity), "events");
    if (!list.is_array())
        return false;
    for (const auto &e : list)
    {
        std::int64_t start = 0, end = 0;
        bool all_day = false, end_date = false;
        if (!parse_iso(when(field(e, "start")), zone, start, all_day))
            continue;
        if (!parse_iso(when(field(e, "end")), zone, end, end_date) || end <= start)
            end = all_day ? next_day(start, zone) : start + 3600000;
        auto summary = trim(text(e, "summary"));
        const bool cancelled = strip_cancelled(summary);
        if (summary.empty())
            summary = "Busy";
        out.push_back(
            {entity, summary, short_place(text(e, "location")), start, end, all_day, cancelled});
    }
    return true;
}
DayEvents day_events(const std::vector<Event> &events, std::int64_t day_start_ms,
                     std::int64_t day_end)
{
    DayEvents out;
    for (const auto &e : events)
        if (e.start < day_end && e.end > day_start_ms)
            (e.all_day ? out.all_day : out.timed).push_back(e);
    std::sort(out.all_day.begin(), out.all_day.end(),
              [](const Event &a, const Event &b) { return a.summary < b.summary; });
    std::sort(out.timed.begin(), out.timed.end(), [](const Event &a, const Event &b)
              { return a.start != b.start ? a.start < b.start : a.end < b.end; });
    return out;
}
} // namespace lamplight

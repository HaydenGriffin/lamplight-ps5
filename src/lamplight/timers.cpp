// Lamplight for PS5 - View Assist timers and appliance timers, and how long is left in words.
// Copyright (C) 2026 Hayden Griffin
// SPDX-License-Identifier: GPL-3.0-or-later
#include "lamplight/timers.hpp"
#include "lamplight/clock.hpp"
#include <algorithm>
#include <cmath>

namespace lamplight
{
std::vector<Timer> view_assist_timers(const Entity &sensor, std::int64_t now_ms, const Zone &zone)
{
    std::vector<Timer> out;
    const auto &list = field(sensor.attributes, "timers");
    if (!list.is_array())
        return out;
    for (const auto &t : list)
    {
        if (!t.is_object() || text(t, "status") == "expired")
            continue;
        Timer timer;
        const auto &id = field(t, "id");
        timer.id = id.is_string() ? id.get<std::string>() : id.is_number() ? id.dump() : "";
        timer.name = text(t, "name");
        timer.kind = text(t, "timer_class", "timer");
        timer.paused = text(t, "status") == "paused";
        const auto at = parse_iso_ms(text(t, "expires"), zone);
        const auto from = parse_iso_ms(text(t, "created_at"), zone);
        auto original = parse_iso_ms(text(t, "original_expiry"), zone);
        if (original == kNoTime)
            original = at;
        if (!timer.paused && at == kNoTime)
            continue;
        timer.at = at == kNoTime ? 0 : at;
        if (!timer.paused)
            timer.left = static_cast<double>(at - now_ms) / 1000.0;
        else
        {
            const double remaining = number(field(t, "remaining"));
            const auto updated = parse_iso_ms(text(t, "updated_at"), zone);
            if (std::isfinite(remaining))
                timer.left = remaining;
            else if (at != kNoTime && updated != kNoTime)
                timer.left = static_cast<double>(at - updated) / 1000.0;
        }
        if (timer.left != -1)
            timer.left = std::max(0.0, timer.left);
        if (from != kNoTime && original != kNoTime && original > from)
            timer.total = static_cast<double>(original - from) / 1000.0;
        out.push_back(std::move(timer));
    }
    std::stable_sort(out.begin(), out.end(), [](const Timer &a, const Timer &b)
                     { return a.paused != b.paused ? !a.paused : (!a.paused && a.at < b.at); });
    return out;
}
std::vector<ApplianceTimer> appliance_timers(const std::vector<Choice> &choices,
                                             const StateTable &states, const Zone &zone)
{
    std::vector<ApplianceTimer> out;
    for (const auto &c : choices)
    {
        const auto &s = states.get(c.entity);
        const auto at = parse_iso_ms(text(s.attributes, "finishes_at"), zone);
        if (s.state == "active" && at != kNoTime)
            out.push_back({c, at});
    }
    return out;
}
std::vector<std::pair<std::string, std::string>> left_parts(double seconds)
{
    const long s = std::max(0L, static_cast<long>(std::ceil(std::isfinite(seconds) ? seconds : 0)));
    const long h = s / 3600, m = s % 3600 / 60, x = s % 60;
    if (h)
        return {{std::to_string(h), "h"}, {two(static_cast<int>(m)), "min"}};
    if (m)
        return {{std::to_string(m), "min"}, {std::to_string(x), "s"}};
    return {{std::to_string(x), "s"}};
}
std::string left_text(double seconds)
{
    std::string out;
    for (const auto &[n, unit] : left_parts(seconds))
        out += (out.empty() ? "" : " ") + n + " " + unit;
    return out;
}
std::string capitalised(std::string s)
{
    if (!s.empty() && s[0] >= 'a' && s[0] <= 'z')
        s[0] = static_cast<char>(s[0] - 'a' + 'A');
    return s;
}
std::string undo_time(double left_seconds)
{
    const long total = std::lround(std::max(30.0, left_seconds));
    const long parts[3] = {total / 3600, total % 3600 / 60, total % 60};
    const char *units[3] = {"hour", "minute", "second"};
    std::string out;
    for (int i = 0; i < 3; ++i)
        if (parts[i])
            out += (out.empty() ? "" : " ") + std::to_string(parts[i]) + " " + units[i] +
                   (parts[i] == 1 ? "" : "s");
    return out;
}
} // namespace lamplight

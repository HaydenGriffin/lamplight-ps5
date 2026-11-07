// Lamplight for PS5 - The house's notices: what Home Assistant says needs attention, newest first.
// Copyright (C) 2026 Hayden Griffin
// SPDX-License-Identifier: GPL-3.0-or-later
#include "lamplight/notices.hpp"
#include <algorithm>

namespace lamplight
{
std::vector<Notice> parse_notices(const Json &items, const Zone &zone)
{
    std::vector<Notice> out;
    if (!items.is_array())
        return out;
    for (const auto &i : items)
    {
        Notice n{text(i, "id"),
                 text(i, "key"),
                 text(i, "title"),
                 text(i, "message"),
                 text(i, "icon"),
                 text(i, "level", "info"),
                 {},
                 0};
        if (n.title.empty())
            continue;
        if (n.id.empty())
            n.id = n.key.empty() ? n.title : n.key;
        if (n.level != "warn" && n.level != "alert")
            n.level = "info";
        const auto &rooms = field(i, "rooms");
        if (rooms.is_array())
            for (const auto &r : rooms)
                if (r.is_string())
                    n.rooms.push_back(r.get<std::string>());
        const auto at = parse_iso_ms(text(i, "at"), zone);
        n.at = at == kNoTime ? 0 : at;
        out.push_back(std::move(n));
    }
    std::stable_sort(out.begin(), out.end(),
                     [](const Notice &a, const Notice &b) { return a.at > b.at; });
    if (out.size() > 30)
        out.resize(30);
    return out;
}
std::string ago(std::int64_t at, std::int64_t now, const Zone &zone)
{
    if (!at)
        return "";
    const auto minutes = (now - at) / 60000;
    if (minutes < 1)
        return "just now";
    if (minutes < 60)
        return std::to_string(minutes) + " min ago";
    if (minutes < 24 * 60)
        return std::to_string(minutes / 60) + " h ago";
    return short_date(at, zone) + " " + hhmm(at, zone);
}
} // namespace lamplight

// Lamplight for PS5 - House: the notices Home Assistant has up, and a new one as it arrives.
// Copyright (C) 2026 Hayden Griffin
// SPDX-License-Identifier: GPL-3.0-or-later
#include "lamplight/application.hpp"
#include "lamplight/paint.hpp"
#include <algorithm>

namespace lamplight
{
using hui::Action;
using hui::Direction;
using hui::gfx::Align;
namespace
{
constexpr float kRowH = 132, kRowGap = 16;
// A notice older than this when first seen is not news (Lamplight opening, say).
constexpr std::int64_t kNewsMs = 15 * 60000;
} // namespace

// The notices as Home Assistant lists them. One that wasn't there before pops up: a toast, or
// the alert screen for an alert; and the resting clock wakes for it.
void Application::update_notices()
{
    if (config_.notices.entity.empty() || !snapshot_.states.has(config_.notices.entity))
        return;
    // A list that isn't there for a moment (the entity reloading) is not every notice gone.
    const auto &items = field(snapshot_.states.get(config_.notices.entity).attributes, "items");
    if (!items.is_array())
        return;
    notices_ = parse_notices(items, clock_.zone);
    auto rank = [](const std::string &level) { return level == "alert" ? 2 : level == "warn"; };
    std::map<std::string, int> seen;
    const Notice *news = nullptr;
    for (const auto &n : notices_)
    {
        // The same notice raised again keeps its key; it is news only if its level rose.
        const auto identity = n.key.empty() ? "id:" + n.id : "key:" + n.key;
        const int level = rank(n.level);
        seen[identity] = std::max(seen[identity], level);
        const auto before = notices_seen_.find(identity);
        const bool fresh = !n.at || now() - n.at < kNewsMs;
        if (!notices_known_ || !fresh || (before != notices_seen_.end() && before->second >= level))
            continue;
        if (n.level == "alert")
            raise_alert({"notice:" + identity, n.title, n.message,
                         n.icon.empty() ? "bell-ring-outline" : n.icon, "", now()});
        else if (!news)
            news = &n;
    }
    if (news)
    {
        toast(news->message.empty() ? news->title : news->title + " \xC2\xB7 " + news->message,
              news->level == "warn", 8000, std::nullopt, "", true);
        idle_s_ = 0;
        ambient_ = std::min(ambient_, 0.49f);
    }
    notices_seen_ = std::move(seen);
    notices_known_ = true;
    house_focus_ = std::clamp(house_focus_, 0, std::max(0, static_cast<int>(notices_.size()) - 1));
}
void Application::update_house(const hui::InputFrame &input)
{
    if (notices_.empty())
    {
        if (input.is_pressed(Action::confirm))
            open_house_sheet();
        return;
    }
    if (input.nav == Direction::up || input.nav == Direction::down)
        house_focus_ = std::clamp(house_focus_ + (input.nav == Direction::down ? 1 : -1), 0,
                                  static_cast<int>(notices_.size()) - 1);
    if (!input.is_pressed(Action::confirm) || config_.notices.dismiss_event.empty())
        return;
    const auto &n = notices_[static_cast<std::size_t>(house_focus_)];
    Request dismiss{{{"type", "fire_event"},
                     {"event_type", config_.notices.dismiss_event},
                     {"event_data", {{"id", n.id}, {"key", n.key}}}}};
    if (dispatch(std::move(dismiss), Job::dismiss))
        toast("Dismissed: " + n.title);
    else
        toast("Not connected to Home Assistant", true, 5000);
}
std::string Application::house_chip() const
{
    int on = 0;
    std::string one;
    for (const auto &mode : config_.house.modes)
        if (snapshot_.states.get(mode.entity).state == "on")
        {
            ++on;
            one = mode.name;
        }
    return !notices_.empty()
               ? std::to_string(notices_.size()) + (notices_.size() == 1 ? " notice" : " notices")
           : on == 1 ? one + " on"
           : on      ? std::to_string(on) + " modes on"
                     : "All quiet";
}
void Application::draw_house(Paint &p) const
{
    const int alerts = static_cast<int>(std::count_if(
        notices_.begin(), notices_.end(), [](const Notice &n) { return n.level != "info"; }));
    const auto modes = modes_words(config_.house, snapshot_.states);
    draw_header(p, page_title(),
                !modes.empty() && notices_.empty() ? modes
                : notices_.empty()                 ? std::string("Nothing needs you")
                : notices_.size() == 1
                    ? std::string("1 notice")
                    : std::to_string(notices_.size()) + " notices" +
                          (alerts ? " \xC2\xB7 " + std::to_string(alerts) +
                                        (alerts == 1 ? " needs a look" : " need a look")
                                  : ""),
                alerts                               ? tone::warn
                : !modes.empty() && notices_.empty() ? tone::accent
                                                     : tone::muted);
    const Rect area{size::margin, size::content_top, 1920 - 2 * size::margin,
                    size::content_bottom - size::content_top};
    if (notices_.empty())
    {
        p.empty(area, "home-outline", "All quiet",
                config_.notices.entity.empty()
                    ? "Everything off and the house's modes are here, or L2 from any page."
                    : "Notices from Home Assistant show here as they come: a finished dishwasher, "
                      "a door left open, a full tank.");
        return;
    }
    // As many rows as fit, scrolled to keep the focused one in view.
    const int fit = static_cast<int>((area.h + kRowGap) / (kRowH + kRowGap));
    const int first =
        std::clamp(house_focus_ - fit + 1, 0, std::max(0, static_cast<int>(notices_.size()) - fit));
    for (int i = first; i < static_cast<int>(notices_.size()) && i < first + fit; ++i)
    {
        const auto &n = notices_[static_cast<std::size_t>(i)];
        const Rect r{area.x, area.y + static_cast<float>(i - first) * (kRowH + kRowGap), area.w,
                     kRowH};
        p.panel(r, size::tile_radius);
        const auto kind = n.level == "alert"  ? DiscKind::alert
                          : n.level == "warn" ? DiscKind::warn
                                              : DiscKind::rest;
        p.disc(r.x + 72, r.cy(), size::disc, n.icon.empty() ? "bell-outline" : n.icon, kind);
        const auto when = ago(n.at, now(), clock_.zone);
        std::string where;
        for (const auto &room : n.rooms)
            where += (where.empty() ? "" : ", ") + capitalised(room);
        const float right = when.empty() ? 0 : p.measure(when, size::label) + 40;
        p.text(n.title, r.x + 140, r.y + 56, size::title, tone::text, true, Align::left,
               r.w - 180 - right);
        const auto detail = where.empty()       ? n.message
                            : n.message.empty() ? where
                                                : where + " \xC2\xB7 " + n.message;
        p.text(detail, r.x + 140, r.y + 100, size::body, tone::muted, false, Align::left,
               r.w - 180);
        if (!when.empty())
            p.text(when, r.x + r.w - 36, r.y + 56, size::label, tone::sky_muted, false,
                   Align::right);
        if (i == house_focus_ && !alert_)
            p.focus(r, size::tile_radius);
    }
}
} // namespace lamplight

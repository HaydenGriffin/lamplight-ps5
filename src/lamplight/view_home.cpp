// Lamplight for PS5 - The clock: the home page, with a chip for every other page.
// Copyright (C) 2026 Hayden Griffin
// SPDX-License-Identifier: GPL-3.0-or-later
#include "lamplight/application.hpp"
#include "lamplight/paint.hpp"
#include "lamplight/room.hpp"
#include <cmath>
#include <algorithm>

namespace lamplight
{
using hui::gfx::Align;
namespace
{
const char *chip_title(Page page)
{
    switch (page)
    {
    case Page::weather:
        return "Weather";
    case Page::agenda:
        return "Agenda";
    case Page::timers:
        return "Timers";
    case Page::wifi:
        return "Wi-Fi";
    case Page::help:
        return "Help";
    case Page::info:
        return "Assistant";
    default:
        return "";
    }
}
} // namespace

// One line on a chip: what that page would tell you now. warm when its subject is running.
std::string Application::home_status(Page page, std::string &icon, bool &warm) const
{
    warm = false;
    switch (page)
    {
    case Page::room:
    {
        const auto &room = config_.room_page;
        icon = room.icon.empty() ? "sofa-outline" : room.icon;
        const auto &m = snapshot_.states.get(room.media);
        if (!room.media.empty() && m.state == "playing")
        {
            warm = true;
            const auto title = text(m.attributes, "media_title");
            return title.empty() ? "Playing" : title;
        }
        int on = 0;
        for (const auto &t : room.tiles)
            on +=
                is_on(t.entity, snapshot_.states.get(t.entity)) && domain_of(t.entity) != "climate";
        warm = on > 0;
        return on ? std::to_string(on) + " on" : "Everything's off";
    }
    case Page::weather:
    {
        const auto &w = snapshot_.states.get(config_.weather);
        icon = condition_icon(w.state);
        if (config_.weather.empty())
            return "Choose it in Setup";
        for (const auto &d : forecast_.days)
            if (day_start(d.at, clock_.zone) == day_start(now(), clock_.zone))
                return "High " + degrees(d.high) + " \xC2\xB7 Low " + degrees(d.low);
        return w.available() ? condition_label(w.state) : "Not available";
    }
    case Page::agenda:
    {
        icon = "calendar-outline";
        if (config_.calendars.empty())
            return "Choose calendars in Setup";
        const auto state = agenda_state();
        if (state == "loading")
            return "Loading\xE2\x80\xA6";
        if (state == "failed")
            return "Couldn't read the calendars";
        const auto start = day_start(now(), clock_.zone);
        const auto today = day_events(events_, start, next_day(start, clock_.zone));
        for (const auto &e : today.timed)
            if (!e.cancelled && e.start <= now() && e.end > now())
            {
                warm = true;
                return "Now: " + e.summary;
            }
        for (const auto &e : today.timed)
            if (!e.cancelled && e.start > now())
                return hhmm(e.start, clock_.zone) + " " + e.summary;
        return "Nothing else today";
    }
    case Page::timers:
    {
        icon = "timer-outline";
        const auto timers =
            view_assist_timers(snapshot_.states.get(config_.view_assist), now(), clock_.zone);
        for (const auto &t : timers)
            if (t.kind == "timer" && !t.paused)
            {
                warm = true;
                const double left = static_cast<double>(t.at - now()) / 1000.0;
                return left <= 0 ? "Done" : left_text(left) + " left";
            }
        const auto apps = appliance_timers(config_.appliances, snapshot_.states, clock_.zone);
        if (!apps.empty())
        {
            warm = true;
            const auto &a = apps.front();
            return (a.choice.name.empty()
                        ? snapshot_.states.get(a.choice.entity).name(a.choice.entity)
                        : a.choice.name) +
                   " " + left_text(static_cast<double>(a.at - now()) / 1000.0);
        }
        for (const auto &t : timers)
            if (t.paused)
                return "Paused";
        for (const auto &t : timers)
            if (t.kind != "timer")
            {
                icon = "alarm";
                return capitalised(t.kind) + " at " + hhmm(t.at, clock_.zone);
            }
        return "None running";
    }
    case Page::wifi:
        icon = "wifi";
        return config_.wifi_ssid.empty() ? "Add the network in Setup" : config_.wifi_ssid;
    case Page::help:
        icon = "microphone-question";
        return read_only() ? "Visitors mode is on" : "Voice commands";
    case Page::info:
        icon = "message-text-outline";
        if (answer_.empty())
            return "Nothing yet";
        {
            auto line = answer_;
            std::replace(line.begin(), line.end(), '\n', ' ');
            return line;
        }
    default:
        icon = "information-outline";
        return "";
    }
}
void Application::draw_home(Paint &p) const
{
    // At rest the face dims (much more at night), the chips go, and everything drifts a few
    // pixels over minutes so nothing stays still long enough to mark an OLED.
    const float rest = ambient_;
    const auto hour = local(now(), clock_.zone).hour;
    const bool night =
        snapshot_.states.get("sun.sun").state == "below_horizon" && (hour >= 22 || hour < 6);
    const float dx = rest * 30 * std::sin(time_ * 6.2832f / 317),
                dy = rest * 18 * std::sin(time_ * 6.2832f / 211);
    p.list.push_transform(1, 0, 0, dx, dy);
    p.list.push_opacity(1 - rest * (night ? 0.62f : 0.3f));
    // The outdoor temperature at the top left, lighter than the time so the time stays the one
    // dominant number.
    const auto &w = snapshot_.states.get(config_.weather);
    if (w.available())
    {
        p.icon(condition_icon(w.state), size::margin + 44, 118, 88, tone::warm);
        const float t = p.text(degrees(number(field(w.attributes, "temperature"))),
                               size::margin + 110, 150, 92, tone::text);
        p.text(condition_label(w.state), size::margin + 110 + t + 24, 146, size::title,
               tone::sky_muted);
    }
    p.text(long_date(now(), clock_.zone), 1920 - size::margin, 146, size::title, tone::sky_muted,
           false, Align::right);
    // The display clock: Lexend 600 with tabular digits, filling the face.
    p.number(hhmm(now(), clock_.zone), 960, 640 + rest * 60, 420, tone::text, Align::center);
    p.list.pop_opacity();
    p.list.push_opacity(1 - rest);
    if (!ready())
        p.text(snapshot_.status, 960, 752, size::body, tone::warn, false, Align::center, 1600);
    // The glance chips: clock glass, one line each; the focused one has the ring.
    const auto pages = content_pages();
    const int n = static_cast<int>(pages.size());
    const float gap = 20, h = 160, y = 786;
    const float w_chip =
        (1920 - 2 * size::margin - gap * static_cast<float>(n - 1)) / static_cast<float>(n);
    for (int i = 0; i < n; ++i)
    {
        const Rect r{size::margin + static_cast<float>(i) * (w_chip + gap), y, w_chip, h};
        std::string icon;
        bool warm = false;
        const auto status = home_status(pages[static_cast<std::size_t>(i)], icon, warm);
        p.list.rounded_rect(r, size::chip_radius, tone::glass);
        if (warm)
            p.wash(r, size::chip_radius, tone::warm_glow.with_alpha(0.5f));
        p.icon(icon, r.x + 46, r.y + 46, 42, warm ? tone::warm : tone::text);
        const auto title = pages[static_cast<std::size_t>(i)] == Page::room
                               ? room_title()
                               : std::string(chip_title(pages[static_cast<std::size_t>(i)]));
        p.text(title, r.x + 26, r.y + 108, size::label, tone::text, true, Align::left, r.w - 46);
        p.text(status, r.x + 26, r.y + 142, size::caption, warm ? tone::warm : tone::sky_muted,
               false, Align::left, r.w - 46);
        if (i == home_focus_ && rest < 0.5f)
            p.focus(r, size::chip_radius);
    }
    p.list.pop_opacity();
    p.list.pop_transform();
}
} // namespace lamplight

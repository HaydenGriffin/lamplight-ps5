// Lamplight for PS5 - Timers: live countdowns with a ring, paused timers, alarms, appliance timers
// and quick-start buttons.
// Copyright (C) 2026 Hayden Griffin
// SPDX-License-Identifier: GPL-3.0-or-later
#include "lamplight/application.hpp"
#include "lamplight/paint.hpp"
#include <algorithm>
#include <cmath>

namespace lamplight
{
using hui::gfx::Align;
namespace
{
constexpr float kRow = 132, kRowGap = 12;
// "", " tomorrow" or " Monday": an alarm's day, when it isn't today.
std::string day_word(std::int64_t at, std::int64_t now, const Zone &zone)
{
    const auto today = day_start(now, zone), tomorrow = next_day(today, zone);
    const auto day = day_start(at, zone);
    const auto days = day < tomorrow ? 0 : day < next_day(tomorrow, zone) ? 1 : 2;
    if (days <= 0)
        return "";
    if (days == 1)
        return " tomorrow";
    return std::string(" ") + weekday_name(local(at, zone).weekday);
}
// "17 min 39 s" with the units smaller and muted, so it can't be read as a time of day.
float countdown(Paint &p, double seconds, float x, float baseline, Color colour)
{
    // A timer that has run out says so until View Assist takes it away, as the card does.
    if (seconds <= 0)
        return p.text("Done", x, baseline, 52, colour, true);
    float pen = x;
    for (const auto &[n, unit] : left_parts(seconds))
    {
        pen += p.number(n, pen, baseline, 52, colour);
        pen += 6;
        pen += p.text(unit, pen, baseline, size::label, colour.with_alpha(0.8f));
        pen += 18;
    }
    return pen - x;
}
} // namespace

std::string Application::timers_subtitle() const
{
    const auto timers =
        view_assist_timers(snapshot_.states.get(config_.view_assist), now(), clock_.zone);
    int running = 0, paused = 0, alarms = 0;
    std::int64_t next = 0;
    for (const auto &t : timers)
    {
        if (t.kind != "timer")
            ++alarms;
        else if (t.paused)
            ++paused;
        else if (!running++)
            next = t.at;
    }
    if (running)
        return std::to_string(running) + " running \xC2\xB7 next done at " +
               hhmm(next, clock_.zone);
    if (paused)
        return std::to_string(paused) + " paused";
    if (alarms)
        return std::to_string(alarms) + (alarms > 1 ? " alarms set" : " alarm set");
    const auto apps = appliance_timers(config_.appliances, snapshot_.states, clock_.zone);
    if (!apps.empty())
    {
        const auto &a = apps.front();
        const auto name = a.choice.name.empty()
                              ? snapshot_.states.get(a.choice.entity).name(a.choice.entity)
                              : a.choice.name;
        return name + " done at " + hhmm(a.at, clock_.zone) +
               (apps.size() > 1 ? " \xC2\xB7 " + std::to_string(apps.size()) + " appliances running"
                                : "");
    }
    if (config_.view_assist.empty())
        return "Choose a View Assist device in Setup to set timers here";
    return "Nothing running in " +
           (config_.room.empty() ? snapshot_.states.get(config_.view_assist).name("this room")
                                 : "the " + config_.room);
}
void Application::draw_timers(Paint &p) const
{
    const auto timers =
        view_assist_timers(snapshot_.states.get(config_.view_assist), now(), clock_.zone);
    const auto apps = appliance_timers(config_.appliances, snapshot_.states, clock_.zone);
    const bool running = std::any_of(timers.begin(), timers.end(),
                                     [](const Timer &t) { return t.kind == "timer" && !t.paused; });
    draw_header(p, "Timers", timers_subtitle(),
                running || !apps.empty() ? tone::warm : tone::muted);
    const Rect list{size::margin, size::content_top, 1164,
                    size::content_bottom - size::content_top};
    const Rect start{list.x + list.w + 24, list.y, 1920 - size::margin - (list.x + list.w + 24),
                     list.h};
    p.panel(list);
    p.panel(start);
    if (timers.empty() && apps.empty())
        p.empty(list, "timer-outline", "No timers running",
                config_.view_assist.empty()
                    ? "Choose a View Assist device in Setup to start one here."
                    : "Choose a time on the right to start one here.");
    // Rows: View Assist's timers (with Cancel), then appliance timers (read only).
    const int rows = static_cast<int>(timers.size() + apps.size());
    const int fit = static_cast<int>((list.h - 24) / (kRow + kRowGap));
    const int focus_row = timer_column_ == 0 ? timer_row_[0] : 0;
    const int first = std::clamp(focus_row - fit + 1, 0, std::max(0, rows - fit));
    float y = list.y + 18;
    for (int i = first; i < rows && i < first + fit; ++i, y += kRow + kRowGap)
    {
        const Rect row{list.x + 18, y, list.w - 36, kRow};
        p.list.rounded_rect(row, size::small_radius, tone::raised);
        const float cx = row.x + 72, cy = row.cy();
        if (i >= static_cast<int>(timers.size()))
        {
            const auto &a = apps[static_cast<std::size_t>(i) - timers.size()];
            const auto name = a.choice.name.empty()
                                  ? snapshot_.states.get(a.choice.entity).name(a.choice.entity)
                                  : a.choice.name;
            p.wash(row, size::small_radius, tone::warm_glow.with_alpha(0.5f));
            p.disc(cx, cy, size::disc, a.choice.icon.empty() ? "timer-sand" : a.choice.icon,
                   DiscKind::on);
            countdown(p, static_cast<double>(a.at - now()) / 1000.0, row.x + 140, row.y + 66,
                      tone::warm);
            p.text(name + " \xC2\xB7 done at " + hhmm(a.at, clock_.zone), row.x + 140, row.y + 108,
                   size::label, tone::muted, false, Align::left, row.w - 170);
            if (timer_column_ == 0 && timer_row_[0] == i)
                p.focus(row, size::small_radius);
            continue;
        }
        const auto &t = timers[static_cast<std::size_t>(i)];
        const bool armed = armed_timer_ == t.id;
        const bool focused = timer_column_ == 0 && timer_row_[0] == i;
        std::string label = t.name.empty() ? (t.kind == "timer"   ? "Timer"
                                              : t.kind == "alarm" ? "Alarm"
                                                                  : "Reminder")
                                           : capitalised(t.name);
        if (t.kind != "timer")
        {
            p.disc(cx, cy, size::disc, t.kind == "alarm" ? "alarm" : "bell-outline",
                   DiscKind::rest);
            const float w =
                p.number(hhmm(t.at, clock_.zone), row.x + 140, row.y + 66, 52, tone::text);
            p.text(day_word(t.at, now(), clock_.zone), row.x + 140 + w, row.y + 66, size::body,
                   tone::text, true);
        }
        else if (t.paused)
        {
            p.disc(cx, cy, size::disc, "timer-pause-outline", DiscKind::rest);
            if (t.left >= 0)
                countdown(p, t.left, row.x + 140, row.y + 66, tone::text);
            else
                p.text("Paused", row.x + 140, row.y + 66, 52, tone::text, true);
            label += " \xC2\xB7 paused";
        }
        else
        {
            // The timer ring, one of Lamplight's two looping animations: it empties as time runs.
            const double left = static_cast<double>(t.at - now()) / 1000.0;
            const float fraction =
                t.total > 0 ? static_cast<float>(std::clamp(left / t.total, 0.0, 1.0)) : 1.0f;
            p.wash(row, size::small_radius, tone::warm_glow.with_alpha(0.5f));
            p.list.ring(cx, cy, 46, 9, tone::raised2);
            if (fraction > 0)
                p.list.arc(cx, cy, 46, 9, 0, fraction * 6.2832f, tone::warm);
            countdown(p, left, row.x + 140, row.y + 66, tone::warm);
            label += " \xC2\xB7 done at " + hhmm(t.at, clock_.zone);
        }
        p.text(label, row.x + 140, row.y + 108, size::label, tone::muted, false, Align::left,
               row.w - 450);
        const float pw = armed ? 400 : 210;
        p.pill({row.x + row.w - pw - 22, row.cy() - size::target / 2, pw, size::target}, "close",
               armed ? "Press again to cancel" : "Cancel",
               armed ? PillKind::danger : PillKind::rest, focused, false, true);
    }
    // Start a timer: big minute buttons.
    p.text("Start a timer", start.x + 32, start.y + 62, size::body, tone::text, true);
    const auto &quick = config_.quick_minutes;
    const float gap = 20, bw = (start.w - 64 - gap) / 2;
    const float rows_of_buttons = static_cast<float>((quick.size() + 1) / 2);
    const float bh = std::min(250.0f, (start.h - 120 - gap * (rows_of_buttons - 1)) /
                                          std::max(1.0f, rows_of_buttons));
    for (std::size_t i = 0; i < quick.size(); ++i)
    {
        const Rect b{start.x + 32 + static_cast<float>(i % 2) * (bw + gap),
                     start.y + 96 + static_cast<float>(i / 2) * (bh + gap), bw, bh};
        const bool disabled = config_.view_assist.empty() || !ready();
        const bool busy = busy_minutes_ == quick[i];
        if (disabled)
            p.list.push_opacity(0.45f);
        else if (busy)
            p.list.push_opacity(0.7f);
        p.list.rounded_rect(b, size::tile_radius, tone::raised2);
        p.number(std::to_string(quick[i]), b.cx(), b.cy() + 22, 84, tone::text, Align::center);
        p.text(busy ? "Starting\xE2\x80\xA6" : "min", b.cx(), b.cy() + 70, size::label, tone::muted,
               false, Align::center);
        if (disabled || busy)
            p.list.pop_opacity();
        if (timer_column_ == 1 && timer_row_[1] == static_cast<int>(i))
            p.focus(b, size::tile_radius);
    }
}
} // namespace lamplight

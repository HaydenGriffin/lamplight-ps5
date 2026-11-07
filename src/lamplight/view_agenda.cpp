// Lamplight for PS5 - Agenda: today and tomorrow from the household calendars, side by side.
// Copyright (C) 2026 Hayden Griffin
// SPDX-License-Identifier: GPL-3.0-or-later
#include "lamplight/application.hpp"
#include "lamplight/paint.hpp"
#include <algorithm>

namespace lamplight
{
using hui::gfx::Align;
namespace
{
constexpr float kColumnWidth = 852, kListTop = 104, kListBottomPad = 20, kGap = 12;
constexpr float kPanelHeight = size::content_bottom - size::content_top;
constexpr float kTitleWidth = kColumnWidth - 40 - 156;
const Choice *calendar_choice(const std::vector<Choice> &list, const std::string &id)
{
    for (const auto &c : list)
        if (c.entity == id)
            return &c;
    return nullptr;
}
} // namespace

std::vector<Application::AgendaRow> Application::agenda_rows(int column) const
{
    std::vector<AgendaRow> rows;
    auto day = day_start(now(), clock_.zone);
    if (column)
        day = next_day(day, clock_.zone);
    const auto events = day_events(events_, day, next_day(day, clock_.zone));
    for (const auto &e : events.all_day)
        rows.push_back({e, false, false, 100});
    for (const auto &e : events.timed)
    {
        AgendaRow r{e, !e.cancelled && e.start <= now() && e.end > now(), e.end <= now(), 0};
        const bool two_lines = fonts_.bold.measure(e.summary, size::body) > kTitleWidth;
        r.height = r.past ? 60 : two_lines ? 140 : 100;
        rows.push_back(std::move(r));
    }
    return rows;
}
int Application::agenda_last_first(int column) const
{
    const auto rows = agenda_rows(column);
    const float room = kPanelHeight - kListTop - kListBottomPad;
    float used = 0;
    for (int i = static_cast<int>(rows.size()) - 1; i >= 0; --i)
    {
        used += rows[static_cast<std::size_t>(i)].height + kGap;
        if (used - kGap > room)
            return i + 1;
    }
    return 0;
}
int Application::agenda_first(int column) const
{
    const int last = agenda_last_first(column);
    if (agenda_revealed_)
        return std::clamp(agenda_scroll_[column], 0, last);
    if (column != 0)
        return 0;
    // Today scrolls so what's on now, or next, is in view; earlier events stay above.
    const auto rows = agenda_rows(0);
    int target = -1;
    for (std::size_t i = 0; i < rows.size(); ++i)
        if (rows[i].live ||
            (!rows[i].past && !rows[i].event.all_day && rows[i].event.start > now()))
        {
            target = static_cast<int>(i);
            break;
        }
    if (target < 0)
        return last;
    const float room = kPanelHeight - kListTop - kListBottomPad;
    int first = target;
    float used = rows[static_cast<std::size_t>(target)].height;
    while (first > 0 && used + kGap + rows[static_cast<std::size_t>(first - 1)].height <= room)
        used += kGap + rows[static_cast<std::size_t>(--first)].height;
    return std::min(first, last);
}
std::string Application::agenda_subtitle() const
{
    const auto state = agenda_state();
    if (config_.calendars.empty())
        return "";
    if (state == "failed")
        return "Couldn't read the calendars";
    const auto date = long_date(now(), clock_.zone);
    if (state == "loading")
        return "Loading\xE2\x80\xA6";
    const auto start = day_start(now(), clock_.zone);
    const auto today = day_events(events_, start, next_day(start, clock_.zone));
    for (const auto &e : today.timed)
        if (!e.cancelled && e.start <= now() && e.end > now())
            return date + " \xC2\xB7 now: " + e.summary;
    for (const auto &e : today.timed)
        if (!e.cancelled && e.start > now())
            return date + " \xC2\xB7 next at " + hhmm(e.start, clock_.zone);
    return date + " \xC2\xB7 nothing else today";
}
void Application::draw_agenda(Paint &p) const
{
    const auto state = agenda_state();
    draw_header(p, "Agenda", agenda_subtitle(), state == "failed" ? tone::warn : tone::muted);
    const Rect all{size::margin, size::content_top, 1920 - 2 * size::margin, kPanelHeight};
    if (config_.calendars.empty())
    {
        p.empty(all, "calendar-outline", "No calendars chosen",
                "Choose the household calendars in Setup (Options).");
        return;
    }
    if (state == "failed")
    {
        p.panel(all);
        p.failed(all, config_.calendars.size() > 1 ? "the calendars" : "the calendar", true);
        return;
    }
    const auto today = day_start(now(), clock_.zone);
    draw_agenda_day(p, {size::margin, size::content_top, kColumnWidth, kPanelHeight}, "Today",
                    today, 0);
    draw_agenda_day(
        p, {1920 - size::margin - kColumnWidth, size::content_top, kColumnWidth, kPanelHeight},
        "Tomorrow", next_day(today, clock_.zone), 1);
}
void Application::draw_agenda_day(Paint &p, const Rect &r, const char *label, std::int64_t day,
                                  int column) const
{
    p.panel(r);
    const float title = p.text(label, r.x + 32, r.y + 66, size::title, tone::text, true);
    p.text(short_date(day, clock_.zone), r.x + 32 + title + 20, r.y + 66, size::label, tone::muted);
    if (agenda_state() == "loading")
    {
        p.text("Loading\xE2\x80\xA6", r.x + 32, r.y + kListTop + 40, size::label, tone::muted);
        return;
    }
    const auto rows = agenda_rows(column);
    if (rows.empty())
    {
        p.empty({r.x, r.y + 60, r.w, r.h - 60}, "calendar-blank-outline", "Nothing on",
                "No plans in the calendars");
        return;
    }
    const int first = agenda_first(column);
    const float top = r.y + kListTop, bottom = r.y + r.h - kListBottomPad;
    // With more than fits, the list stops short and "More" has the strip below it to itself.
    float end = top;
    bool more = false;
    for (std::size_t i = static_cast<std::size_t>(first); i < rows.size() && !more; ++i)
    {
        more = end + rows[i].height > bottom + 1;
        end += rows[i].height + kGap;
    }
    const float list_bottom = more ? bottom - 30 : bottom;
    const Rect clip{r.x, top - 8, r.w, list_bottom - top + 8};
    p.list.push_clip(clip);
    float y = top;
    for (std::size_t i = static_cast<std::size_t>(first); i < rows.size(); ++i)
    {
        const auto &row = rows[i];
        // One that doesn't fit is cut off under the fade, so the list plainly goes on.
        if (y > list_bottom - 40)
            break;
        const auto &e = row.event;
        const Rect box{r.x + 20, y, r.w - 40, row.height};
        const auto *choice = calendar_choice(config_.calendars, e.calendar);
        const auto calendar = choice && !choice->name.empty()
                                  ? choice->name
                                  : snapshot_.states.get(e.calendar).name(e.calendar);
        const auto icon =
            choice && !choice->icon.empty() ? choice->icon : std::string("calendar-outline");
        if (e.all_day)
        {
            p.list.rounded_rect(box, size::small_radius, tone::raised2);
            p.disc(box.x + 52, box.cy(), size::disc_small, icon, DiscKind::rest);
            const auto name_colour = e.cancelled ? tone::muted : tone::text;
            const float w = p.text(e.summary, box.x + 100, box.y + 46, size::body, name_colour,
                                   !e.cancelled, Align::left, box.w - 124);
            if (e.cancelled)
                p.strike(box.x + 100, box.y + 46, w, size::body, tone::muted);
            p.text(calendar + " \xC2\xB7 all day" + (e.cancelled ? " \xC2\xB7 cancelled" : ""),
                   box.x + 100, box.y + 82, size::caption, tone::muted, false, Align::left,
                   box.w - 124);
        }
        else if (row.past)
        {
            p.number(e.start < day ? "Until" : hhmm(e.start, clock_.zone), box.x + 20, box.y + 40,
                     size::label, tone::muted, Align::left, true);
            const float w = p.text(e.summary, box.x + 156, box.y + 40, size::label, tone::muted,
                                   false, Align::left, box.w - 176);
            if (e.cancelled)
                p.strike(box.x + 156, box.y + 40, w, size::label, tone::muted);
        }
        else
        {
            p.list.rounded_rect(box, size::small_radius, tone::raised);
            if (row.live)
            {
                p.wash(box, size::small_radius);
                p.list.rounded_rect(box, size::small_radius, tone::warm_soft);
                p.live_dot(box.x + 30, box.y + 38);
                p.text("Now", box.x + 50, box.y + 50, size::button, tone::warm, true);
            }
            else
                p.number(e.start < day ? "Until" : hhmm(e.start, clock_.zone), box.x + 20,
                         box.y + 50, size::button, tone::text);
            const auto lines =
                hui::ui::paragraph_lines(fonts_.bold, e.summary, size::body, kTitleWidth, 2);
            float base = box.y + 50;
            for (const auto &line : lines.lines)
            {
                const float w = p.text(line, box.x + 156, base, size::body,
                                       e.cancelled ? tone::muted : tone::text, !e.cancelled);
                if (e.cancelled)
                    p.strike(box.x + 156, base, w, size::body, tone::muted);
                base += 42;
            }
            std::string meta = calendar;
            meta += row.live ? " \xC2\xB7 until " + hhmm(e.end, clock_.zone)
                             : " \xC2\xB7 to " + hhmm(e.end, clock_.zone);
            if (!e.place.empty())
                meta += " \xC2\xB7 " + e.place;
            if (e.cancelled)
                meta += " \xC2\xB7 cancelled";
            p.text(meta, box.x + 156, base - 6, size::caption, row.live ? tone::warm : tone::muted,
                   false, Align::left, box.w - 176);
        }
        y += row.height + kGap;
    }
    p.list.pop_clip();
    // Scroll hints: earlier events above, more below.
    if (first > 0)
        p.icon("chevron-up", r.x + r.w - 52, r.y + 54, 40, tone::muted);
    if (more)
    {
        p.list.gradient_rect({r.x + 20, list_bottom - 56, r.w - 40, 56}, 0,
                             tone::panel_solid.with_alpha(0), tone::panel_solid.with_alpha(0.96f));
        p.icon("chevron-down", r.cx() - 40, bottom - 4, 36, tone::muted);
        p.text("More", r.cx() - 16, bottom + 6, size::caption, tone::muted);
    }
    const bool scrollable = first > 0 || more;
    if (scrollable && column == agenda_column_)
        p.focus(r, size::tile_radius);
}
} // namespace lamplight

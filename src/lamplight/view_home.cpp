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
constexpr float kDeepRestAfterS = 20 * 60, kDeepRestOverS = 10 * 60;
constexpr float kChipGap = 20, kChipH = 160, kChipY = 786, kChipMinW = 150;
// "12 min", "1 h 5": how long, to the minute, in few enough words for a chip.
std::string short_time_left(double seconds)
{
    if (seconds < 60)
        return std::to_string(std::max(0, static_cast<int>(std::ceil(seconds)))) + " s";
    const int minutes = static_cast<int>(std::ceil(seconds / 60));
    return minutes < 60 ? std::to_string(minutes) + " min"
                        : std::to_string(minutes / 60) + " h " + std::to_string(minutes % 60);
}
const char *chip_title(Page page)
{
    switch (page)
    {
    case Page::music:
        return "Music";
    case Page::house:
        return "House";
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
        const auto &room = current_room();
        icon = room.icon.empty() ? "sofa-outline" : room.icon;
        int on = 0, things = 0;
        for (const auto &t : room.tiles)
        {
            things += can_glow(t.entity);
            on += can_glow(t.entity) && is_on(t.entity, snapshot_.states.get(t.entity));
        }
        warm = on > 0;
        return on ? std::to_string(on) + " of " + std::to_string(things) + " on"
                  : "Everything's off";
    }
    case Page::house:
    {
        const bool urgent = house_urgent();
        icon = notices_.empty() ? "home-outline" : urgent ? "bell-ring-outline" : "bell-outline";
        if (notices_.empty())
        {
            const auto modes = modes_words(config_.house, snapshot_.states);
            return modes.empty() ? "All quiet" : modes;
        }
        return notices_.size() == 1
                   ? notices_[0].title
                   : std::to_string(notices_.size()) + " \xC2\xB7 " + notices_[0].title;
    }
    case Page::music:
    {
        icon = "music";
        const auto m = music_state();
        warm = m.state == "playing";
        const auto title = text(m.attributes, "media_title");
        if (m.state == "playing" || m.state == "paused")
            return title.empty() ? capitalised(m.state) : title;
        return m.available() ? "Nothing playing" : "Not available";
    }
    case Page::weather:
    {
        const auto &w = snapshot_.states.get(config_.weather);
        icon = condition_icon(w.state, after_dark());
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
// Twenty minutes into its rest the clock goes dimmer still, over ten more: 0 to 1.
bool Application::after_dark() const
{
    return snapshot_.states.get("sun.sun").state == "below_horizon";
}
bool Application::late_at_night() const
{
    const auto hour = local(now(), clock_.zone).hour;
    return after_dark() && (hour >= 22 || hour < 6);
}
bool Application::house_urgent() const
{
    return std::any_of(notices_.begin(), notices_.end(),
                       [](const Notice &n) { return n.level != "info"; });
}
float Application::deep_rest() const
{
    return ambient_ * std::clamp((idle_s_ - kDeepRestAfterS) / kDeepRestOverS, 0.0f, 1.0f);
}
// A chip's few words, which always fit: the focused chip's whole sentence (home_status) has a line
// of its own above them.
std::string Application::home_chip(Page page) const
{
    if (!ready() && page != Page::wifi && page != Page::help && page != Page::info)
        return "Offline";
    switch (page)
    {
    case Page::room:
    {
        int on = 0;
        for (const auto &t : current_room().tiles)
            on += can_glow(t.entity) && is_on(t.entity, snapshot_.states.get(t.entity));
        return on ? std::to_string(on) + " on" : "All off";
    }
    case Page::house:
        return house_chip();
    case Page::music:
    {
        const auto m = music_state();
        return m.state == "playing" ? "Playing" : m.state == "paused" ? "Paused" : "Stopped";
    }
    case Page::weather:
    {
        if (config_.weather.empty())
            return "Set up";
        for (const auto &d : forecast_.days)
            if (day_start(d.at, clock_.zone) == day_start(now(), clock_.zone))
                return degrees(d.high) + " / " + degrees(d.low);
        const auto &w = snapshot_.states.get(config_.weather);
        return w.available() ? degrees(number(field(w.attributes, "temperature"))) : "No data";
    }
    case Page::agenda:
    {
        if (config_.calendars.empty())
            return "Set up";
        const auto state = agenda_state();
        if (state == "loading")
            return "\xE2\x80\xA6";
        if (state == "failed")
            return "Failed";
        const auto start = day_start(now(), clock_.zone);
        const auto today = day_events(events_, start, next_day(start, clock_.zone));
        for (const auto &e : today.timed)
            if (!e.cancelled && e.start <= now() && e.end > now())
                return "Now";
        for (const auto &e : today.timed)
            if (!e.cancelled && e.start > now())
                return hhmm(e.start, clock_.zone);
        return "Free";
    }
    case Page::timers:
    {
        const auto timers =
            view_assist_timers(snapshot_.states.get(config_.view_assist), now(), clock_.zone);
        for (const auto &t : timers)
            if (t.kind == "timer" && !t.paused)
                return short_time_left(static_cast<double>(t.at - now()) / 1000.0);
        const auto apps = appliance_timers(config_.appliances, snapshot_.states, clock_.zone);
        if (!apps.empty())
            return short_time_left(static_cast<double>(apps.front().at - now()) / 1000.0);
        for (const auto &t : timers)
            if (t.paused)
                return "Paused";
        for (const auto &t : timers)
            if (t.kind != "timer")
                return hhmm(t.at, clock_.zone);
        return "None";
    }
    case Page::wifi:
        return config_.wifi_ssid.empty() ? "Set up" : "QR code";
    case Page::help:
        return read_only() ? "Visitors" : "Examples";
    case Page::info:
        return answer_.empty() ? "None yet" : "Answered";
    default:
        return "";
    }
}
// Above the chips: why it's offline, or else the whole of what the focused chip only hints at.
void Application::draw_home_line(Paint &p) const
{
    const auto pages = content_pages();
    std::string icon;
    bool warm = false;
    const auto line =
        ready() ? home_status(pages[static_cast<std::size_t>(
                                  std::clamp(home_focus_, 0, static_cast<int>(pages.size()) - 1))],
                              icon, warm)
                : snapshot_.status;
    if (line.empty())
        return;
    const bool warning = pages[static_cast<std::size_t>(std::clamp(
                             home_focus_, 0, static_cast<int>(pages.size()) - 1))] == Page::house &&
                         house_urgent();
    const auto colour = !ready() || warning ? tone::warn : warm ? tone::warm : tone::text;
    if (has_clock_backdrop())
    {
        const float w = std::min(p.measure(line, size::body), 1600.0f) + 72;
        p.list.rounded_rect({960 - w / 2, 716, w, 56}, 28, tone::glass);
    }
    p.text(line, 960, 756, size::body, colour, false, Align::center, 1600);
}
// The glance chips: clock glass, as wide as their titles need, and whatever's over shared out.
void Application::draw_chips(Paint &p, float rest) const
{
    const auto pages = content_pages();
    const int n = static_cast<int>(pages.size());
    // Eight chips or more (a room and music too) are narrower: their words a step smaller.
    const float title_size = n >= 8 ? size::caption : size::label;
    const float pad = n >= 8 ? 20 : 26;
    const auto title_of = [&](Page page)
    { return page == Page::room ? room_title() : std::string(chip_title(page)); };
    std::vector<float> widths;
    float total = 0;
    for (const auto page : pages)
    {
        widths.push_back(
            std::max(kChipMinW, p.measure(title_of(page), title_size, true) + 2 * pad));
        total += widths.back();
    }
    const float room_for = 1920 - 2 * size::margin - kChipGap * static_cast<float>(n - 1);
    float x = size::margin;
    for (int i = 0; i < n; ++i)
    {
        const auto page = pages[static_cast<std::size_t>(i)];
        const float w = total <= room_for ? widths[static_cast<std::size_t>(i)] +
                                                (room_for - total) / static_cast<float>(n)
                                          : room_for / static_cast<float>(n);
        const Rect r{x, kChipY, w, kChipH};
        x += w + kChipGap;
        std::string icon;
        bool warm = false;
        home_status(page, icon, warm);
        warm = warm && ready();
        // A notice that wants a look isn't something running: orange, not the lamp's amber.
        const bool warning = page == Page::house && house_urgent() && ready();
        p.list.rounded_rect(r, size::chip_radius, tone::glass);
        if (warning)
            p.wash(r, size::chip_radius, tone::warn.with_alpha(0.28f));
        else if (warm)
            p.wash(r, size::chip_radius, tone::warm_glow.with_alpha(0.5f));
        p.icon(icon, r.x + 46, r.y + 46, 42, warning ? tone::warn : warm ? tone::warm : tone::text);
        p.text(title_of(page), r.x + pad, r.y + 108, title_size, tone::text, true, Align::left,
               r.w - 2 * pad);
        p.text(home_chip(page), r.x + pad, r.y + 142, size::caption,
               warning ? tone::warn
               : warm  ? tone::warm
                       : tone::sky_muted,
               false, Align::left, r.w - 2 * pad);
        if (i == home_focus_ && rest < 0.5f && !alert_)
            p.focus(r, size::chip_radius);
    }
}
void Application::draw_home(Paint &p) const
{
    // At rest the face dims (much more at night), the chips go, and everything drifts a few
    // pixels over minutes so nothing stays still long enough to mark an OLED.
    const float rest = ambient_;
    const bool night = late_at_night();
    const float drift = config_.reduced_motion ? 0 : rest;
    const float dx = drift * 60 * std::sin(time_ * 6.2832f / 317),
                dy = drift * 36 * std::sin(time_ * 6.2832f / 211);
    const float deep = deep_rest();
    // At night the whole face is dim, so the words beside the time can't be dimmer still.
    const auto beside = night ? tone::text : tone::sky_muted;
    const bool over_picture = shown_picture(Pic::photo) || shown_picture(Pic::backdrop);
    draw_photo(p);
    draw_mood(p);
    p.list.push_transform(1, 0, 0, dx, dy);
    p.list.push_opacity(1 -
                        rest * (night          ? 0.62f
                                : over_picture ? 0.15f
                                               : 0.3f) -
                        deep * (night ? 0.13f : 0.25f));
    // The outdoor temperature at the top left, lighter than the time so the time stays the one
    // dominant number.
    const auto &w = snapshot_.states.get(config_.weather);
    if (w.available())
    {
        p.icon(condition_icon(w.state, after_dark()), size::margin + 44, 118, 88, tone::warm);
        const float t = p.text(degrees(number(field(w.attributes, "temperature"))),
                               size::margin + 110, 150, 92, tone::text);
        p.text(condition_label(w.state), size::margin + 110 + t + 24, 146, size::title, beside);
    }
    p.text(long_date(now(), clock_.zone), 1920 - size::margin, 146, size::title, beside, false,
           Align::right);
    // The display clock: Lexend 600 with tabular digits, filling the face.
    std::string period;
    const auto digits = clock_digits(now(), clock_.zone, period);
    // Behind an alert's card, the huge digits would only leave stray shapes either side of it.
    if (!alert_)
    {
        const float face = p.number(digits, 960, 640 + rest * 60, 420, tone::text, Align::center);
        if (!period.empty())
            p.number(period, 960 + face / 2 + 24, 640 + rest * 60, 96, beside);
    }
    p.list.pop_opacity();
    p.list.push_opacity(1 - rest);
    draw_home_line(p);
    draw_chips(p, rest);
    p.list.pop_opacity();
    p.list.pop_transform();
}
} // namespace lamplight

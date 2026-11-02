// Lamplight for PS5 - The page around every screen: sky, header, hints and the toast.
// Copyright (C) 2026 Hayden Griffin
// SPDX-License-Identifier: GPL-3.0-or-later
#include "lamplight/application.hpp"
#include "lamplight/paint.hpp"
#include "ui/glyphs.hpp"

namespace lamplight
{
using hui::gfx::Align;
using hui::ui::Button;
namespace
{
struct SkyColours
{
    Color top, middle, bottom;
};
// The Weather card's skies (linear-gradient 160deg), drawn top to bottom.
SkyColours sky_colours(Sky sky)
{
    switch (sky)
    {
    case Sky::day:
        return {Color::rgb(0x1c4f7c), Color::rgb(0x173a5c), Color::rgb(0x10233a)};
    case Sky::grey:
        return {Color::rgb(0x3a4652), Color::rgb(0x28313b), Color::rgb(0x1b2128)};
    case Sky::rain:
        return {Color::rgb(0x2c3b4a), Color::rgb(0x1f2a36), Color::rgb(0x141b23)};
    case Sky::night:
        break;
    }
    return {Color::rgb(0x141a2e), Color::rgb(0x0d1120), Color::rgb(0x07090f)};
}
} // namespace

void Application::draw_background(Paint &p) const
{
    const auto &weather = snapshot_.states.get(config_.weather);
    const auto &sun = snapshot_.states.get("sun.sun");
    const auto sky = sky_colours(sky_for(weather.state, sun.state != "below_horizon"));
    p.list.gradient_rect({0, 0, 1920, 594}, 0, sky.top, sky.middle);
    p.list.gradient_rect({0, 594, 1920, 486}, 0, sky.middle, sky.bottom);
    // Every other page sits on the sky dimmed as the tablets dim their room photo, so the
    // smoked-glass panels read as they do there. The clock and Weather show the sky itself.
    if (page_ != Page::home && page_ != Page::weather)
        p.list.rounded_rect({0, 0, 1920, 1080}, 0, tone::page.with_alpha(0.62f));
    // The clock at rest goes towards Night Ground; at night, almost all the way.
    if (page_ == Page::home && ambient_ > 0)
    {
        const auto hour = local(now(), clock_.zone).hour;
        const bool night = sun.state == "below_horizon" && (hour >= 22 || hour < 6);
        p.list.rounded_rect({0, 0, 1920, 1080}, 0,
                            tone::page.with_alpha(ambient_ * (night ? 0.94f : 0.55f)));
    }
}
void Application::draw_header(Paint &p, const std::string &title, const std::string &subtitle,
                              Color subtitle_colour) const
{
    p.text(title, size::margin, 122, size::headline, tone::text, true, Align::left, 1180);
    if (!subtitle.empty())
        p.text(subtitle, size::margin, 172, size::label, subtitle_colour, false, Align::left, 1240);
    // The clock stays in view on every page: on a TV it is the header's clock button.
    p.number(hhmm(now(), clock_.zone), 1920 - size::margin, 122, size::headline, tone::text,
             Align::right);
    p.text(short_date(now(), clock_.zone), 1920 - size::margin, 172, size::label, tone::muted,
           false, Align::right);
}
void Application::draw_hints(Paint &p) const
{
    std::vector<hui::ui::Hint> hints;
    switch (page_)
    {
    case Page::home:
        hints = {{Button::dpad, "Choose"},
                 {Button::cross, "Open"},
                 {Button::options, "Setup"},
                 {Button::circle, "Close"}};
        break;
    case Page::room:
        hints = {{Button::cross, "Select"},
                 {Button::l1, "Pages", Button::r1},
                 {Button::circle, "Clock"}};
        break;
    case Page::weather:
    case Page::wifi:
        hints = {{Button::l1, "Pages", Button::r1}, {Button::circle, "Clock"}};
        if (page_ == Page::weather && forecast_state() == "failed")
            hints.insert(hints.begin(), {Button::cross, "Retry"});
        break;
    case Page::agenda:
        hints = {
            {Button::dpad, "Scroll"}, {Button::l1, "Pages", Button::r1}, {Button::circle, "Clock"}};
        if (agenda_state() == "failed")
            hints[0] = {Button::cross, "Retry"};
        break;
    case Page::timers:
        hints = {{Button::cross, "Select"},
                 {Button::l1, "Pages", Button::r1},
                 {Button::circle, "Clock"}};
        if (toast_.undo)
            hints.insert(hints.begin(), {Button::triangle, "Undo"});
        break;
    case Page::help:
        hints = {{Button::cross, read_only() ? "Choose" : "Try it"},
                 {Button::triangle, "Type a question"},
                 {Button::l1, "Pages", Button::r1},
                 {Button::circle, "Clock"}};
        if (read_only())
            hints.erase(hints.begin() + 1);
        break;
    case Page::info:
        hints = {
            {Button::dpad, "Scroll"}, {Button::l1, "Pages", Button::r1}, {Button::circle, "Clock"}};
        if (!config_.view_assist.empty() && !va_message_.empty() && answer_ == va_message_)
            hints.insert(hints.begin() + 1, {Button::square, "Keep"});
        break;
    case Page::setup:
        hints = {{Button::cross, "Select"}, {Button::circle, "Clock"}};
        break;
    case Page::pick:
        hints = {{Button::cross, "Select"}, {Button::circle, "Back to Setup"}};
        break;
    case Page::quit:
        hints = {{Button::cross, "Select"}, {Button::circle, "Clock"}};
        break;
    }
    if (alert_)
        hints = {{Button::cross, alert_still_listed() ? "Stop" : "OK"},
                 {Button::circle, "Dismiss"}};
    hui::ui::HintLayout layout;
    layout.size = 44;
    layout.text_size = size::caption;
    layout.cy = 1012;
    layout.item_gap = 48;
    layout.font = &fonts_.text;
    auto style = hui::ui::GlyphStyle::dark();
    style.label = tone::muted;
    hui::ui::draw_hints(p.list, fonts_.kit, style, hints.data(), static_cast<int>(hints.size()),
                        size::margin, false, layout);
}
void Application::draw_toast(Paint &p) const
{
    if (toast_.text.empty())
        return;
    // A dark pill at the top centre over everything; the error pill is deep red. Help's reply
    // wraps to three lines (the one exception), and Undo grows the pill a button.
    const float max_width = 1500, pad_x = 40, line_height = 46, sz = size::button;
    const auto lines =
        toast_.wrap
            ? hui::ui::paragraph_lines(fonts_.bold, toast_.text, sz, max_width - 2 * pad_x, 3)
            : hui::ui::ParagraphLines{
                  {fonts_.bold.font->fit(toast_.text, sz,
                                         max_width - 2 * pad_x - (toast_.undo ? 260 : 0))},
                  false};
    float text_width = 0;
    for (const auto &l : lines.lines)
        text_width = std::max(text_width, p.measure(l, sz, true));
    const float undo_width = toast_.undo ? 230 : 0;
    const float h = toast_.wrap ? 40 + line_height * static_cast<float>(lines.lines.size()) : 96;
    const float w = text_width + 2 * pad_x + (undo_width ? undo_width + 24 : 0);
    const Rect r{960 - w / 2, 28, w, h};
    p.list.shadow(r.inset(-4), toast_.wrap ? 44 : h / 2, 30, Color::rgb(0x000000, 0.5f));
    p.list.bordered_rect(r, toast_.wrap ? 44 : h / 2,
                         toast_.error ? tone::error_ground : tone::toast, 2,
                         toast_.error ? tone::error_line : Color::rgb(0xffffff, 0.12f));
    float y = r.y + (toast_.wrap ? 20 + 34 : h / 2 + sz * 0.36f);
    for (const auto &l : lines.lines)
    {
        p.text(l, r.x + pad_x, y, sz, toast_.error ? tone::error_text : tone::text, true);
        y += line_height;
    }
    if (toast_.undo)
    {
        const Rect undo{r.x + r.w - undo_width - 8, r.y + 8, undo_width, h - 16};
        p.list.rounded_rect(undo, undo.h / 2, tone::raised2);
        auto style = hui::ui::GlyphStyle::dark();
        hui::ui::draw_button(p.list, fonts_.kit, style, Button::triangle, undo.x + 22, undo.cy(),
                             40);
        p.icon("undo", undo.x + 100, undo.cy(), 36, tone::warm);
        p.text("Undo", undo.x + 128, undo.cy() + 12, sz, tone::text, true);
    }
}
std::string Application::page_title() const
{
    switch (page_)
    {
    case Page::room:
        return room_title();
    case Page::weather:
        return "Weather";
    case Page::agenda:
        return "Agenda";
    case Page::timers:
        return "Timers";
    case Page::wifi:
        return "Wi-Fi";
    case Page::help:
        return "What can I say?";
    case Page::info:
        return answer_title_.empty() ? "Assistant" : answer_title_;
    case Page::setup:
        return "Setup";
    case Page::pick:
        switch (pick_)
        {
        case Pick::weather:
            return "Weather";
        case Pick::calendars:
            return "Calendars";
        case Pick::view_assist:
            return "View Assist device";
        case Pick::appliances:
            return "Appliance timers";
        case Pick::agent:
            return "Assistant";
        case Pick::read_only:
            return "Visitors switch";
        }
        break;
    case Page::quit:
        return "Close Lamplight?";
    case Page::home:
        break;
    }
    return "Lamplight";
}
void Application::draw(hui::gfx::DrawList &list) const
{
    Paint p(list, fonts_, time_, config_.reduced_motion);
    draw_background(p);
    switch (page_)
    {
    case Page::home:
        draw_home(p);
        break;
    case Page::room:
        draw_room(p);
        break;
    case Page::weather:
        draw_weather(p);
        break;
    case Page::agenda:
        draw_agenda(p);
        break;
    case Page::timers:
        draw_timers(p);
        break;
    case Page::wifi:
        draw_wifi(p);
        break;
    case Page::help:
        draw_help(p);
        break;
    case Page::info:
        draw_info(p);
        break;
    case Page::setup:
    case Page::pick:
    case Page::quit:
        draw_setup(p);
        break;
    }
    if (alert_)
        draw_alert(p);
    p.list.push_opacity(page_ == Page::home && !alert_ ? 1 - ambient_ : 1);
    draw_hints(p);
    p.list.pop_opacity();
    draw_toast(p);
}
} // namespace lamplight

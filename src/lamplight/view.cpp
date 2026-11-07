// Lamplight for PS5 - The page around every screen: sky, header, hints and the toast.
// Copyright (C) 2026 Hayden Griffin
// SPDX-License-Identifier: GPL-3.0-or-later
#include "lamplight/application.hpp"
#include "lamplight/paint.hpp"
#include "ui/glyphs.hpp"
#include <algorithm>

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
// What the buttons do on the sheet's focused row, in the row's own words.
std::vector<hui::ui::Hint> sheet_hints(const Control &row, int cursor, bool armed)
{
    const hui::ui::Hint done{Button::circle, "Done"};
    if (armed)
        return {{Button::dpad, "Move"}, {Button::cross, "Confirm"}, {Button::circle, "Cancel"}};
    switch (row.kind)
    {
    case Control::Kind::slider:
    {
        if (row.disabled)
            return {{Button::dpad, "Move"}, done};
        std::vector<hui::ui::Hint> hints{
            {Button::dpad, "Up, down: move \xC2\xB7 Left, right: adjust"}};
        if (row.press)
            hints.push_back({Button::cross, row.press_words.c_str()});
        hints.push_back({Button::right_stick, row.label.c_str()});
        hints.push_back(done);
        return hints;
    }
    case Control::Kind::power:
        return {{Button::dpad, "Move"}, {Button::cross, row.on ? "Turn off" : "Turn on"}, done};
    case Control::Kind::buttons:
    {
        const auto &option = row.options[static_cast<std::size_t>(cursor)];
        // Nothing to press until it can run: the row says why.
        if (option.disabled)
            return {{Button::dpad, "Move"}, done};
        return {{Button::dpad, "Move"}, {Button::cross, option.label.c_str()}, done};
    }
    case Control::Kind::chart:
        return {done};
    case Control::Kind::options:
    case Control::Kind::swatches:
        break;
    }
    return {{Button::dpad, "Move"}, {Button::cross, "Choose"}, done};
}
} // namespace

void Application::draw_background(Paint &p) const
{
    const auto &weather = snapshot_.states.get(config_.weather);
    const auto sky = sky_colours(sky_for(weather.state, !after_dark()));
    p.list.gradient_rect({0, 0, 1920, 594}, 0, sky.top, sky.middle);
    p.list.gradient_rect({0, 594, 1920, 486}, 0, sky.middle, sky.bottom);
    // Every other page sits on the sky dimmed as the tablets dim their room photo (or on the photo
    // itself, when there is one), so the smoked-glass panels read as they do there. The clock and
    // Weather show the sky itself.
    if (has_page_backdrop())
        draw_page_backdrop(p);
    else if (page_ != Page::home && page_ != Page::weather)
        p.list.rounded_rect({0, 0, 1920, 1080}, 0, tone::page.with_alpha(0.62f));
    // The clock at rest goes towards Night Ground; at night, almost all the way.
    if (page_ == Page::home && ambient_ > 0)
    {
        p.list.rounded_rect({0, 0, 1920, 1080}, 0,
                            tone::page.with_alpha(ambient_ * (late_at_night() ? 0.94f : 0.55f)));
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
    // Hints only point at their words, so these stay until the row is drawn.
    std::string level_words, cross_words;
    const auto rows = sheet_ ? sheet_rows() : std::vector<Control>{};
    switch (page_)
    {
    case Page::home:
        hints = {{Button::dpad, "Move"},
                 {Button::cross, "Open"},
                 {Button::r2, "Hold to ask"},
                 {Button::options, "Setup"},
                 {Button::circle, "Close"}};
        // Asking needs Home Assistant, and visitors mode says no.
        if (read_only() || !ready())
            hints.erase(hints.begin() + 2);
        if (!config_.house.empty())
            hints.insert(hints.begin() + 2, {Button::l2, "House controls"});
        if (!config_.photos.empty())
            hints.insert(hints.begin() + 2, {Button::triangle, "Photos"});
        break;
    case Page::room:
    {
        hints = {{Button::l1, "Pages", Button::r1}, {Button::circle, "Clock"}};
        if (config_.rooms.size() > 1)
            hints.insert(hints.begin(), {Button::square, "Next room"});
        // What Cross, Triangle and the right stick do depends on what has the focus.
        const auto slots = room_slots();
        if (room_focus_ >= 0 && room_focus_ < static_cast<int>(slots.size()))
        {
            const auto &slot = slots[static_cast<std::size_t>(room_focus_)];
            const auto entity = slot_entity(slot);
            const auto &state = snapshot_.states.get(entity);
            const std::string press = press_words(slot);
            const bool own = slot.kind != RoomSlot::Kind::action && state.available() &&
                             !controls_for(entity, state).empty();
            const auto level = slot_level(slot);
            if (level)
            {
                level_words = level->label;
                hints.insert(hints.begin(), {Button::right_stick, level_words.c_str()});
            }
            if (own && press != "Controls" && press != "History")
                hints.insert(hints.begin(), {Button::triangle, "Controls"});
            else if (!own && room_has_lamps())
                hints.insert(hints.begin(), {Button::triangle, "All lamps"});
            if (!press.empty())
                hints.insert(
                    hints.begin(),
                    {Button::cross, room_focus_ == room_armed_ ? "Confirm" : press_words(slot)});
            // Waiting for a second press there are only two answers, and the way out to a page.
            if (room_focus_ == room_armed_)
                hints = {{Button::cross, "Confirm"},
                         {Button::l1, "Pages", Button::r1},
                         {Button::circle, "Cancel"}};
            else
                hints.insert(hints.begin(), {Button::dpad, "Move"});
        }
        break;
    }
    case Page::house:
        hints = {
            {Button::dpad, "Move"}, {Button::l1, "Pages", Button::r1}, {Button::circle, "Clock"}};
        if (notices_.empty() && !config_.house.empty())
            hints = {{Button::cross, "House controls"},
                     {Button::l1, "Pages", Button::r1},
                     {Button::circle, "Clock"}};
        else if (!config_.house.empty())
            hints.insert(hints.begin() + 1, {Button::l2, "House controls"});
        if (!config_.notices.dismiss_event.empty() && !notices_.empty())
            hints.insert(hints.begin() + 1, {Button::cross, "Dismiss"});
        break;
    case Page::music:
        hints = {{Button::dpad, "Left, right: move \xC2\xB7 Up, down: volume"},
                 {Button::cross, music_focus_ == 0                  ? "Previous"
                                 : music_focus_ == 2                ? "Next"
                                 : music_state().state == "playing" ? "Pause"
                                                                    : "Play"},
                 {Button::triangle, "More"},
                 {Button::right_stick, "Volume"},
                 {Button::l1, "Pages", Button::r1},
                 {Button::circle, "Clock"}};
        if (has_speakers_sheet())
            hints.insert(hints.begin() + 3, {Button::square, "Speakers"});
        break;
    case Page::photos:
        hints = {{Button::dpad, "Previous and next"},
                 {Button::cross, photo_paused_ ? "Play" : "Pause"},
                 {Button::square, photo_clock_ ? "Hide the clock" : "Show the clock"},
                 {Button::l1, "Faster and slower", Button::r1},
                 {Button::circle, "Back"}};
        break;
    case Page::weather:
    case Page::wifi:
        hints = {{Button::l1, "Pages", Button::r1}, {Button::circle, "Clock"}};
        if (page_ == Page::weather && forecast_state() == "failed")
            hints.insert(hints.begin(), {Button::cross, "Retry"});
        break;
    case Page::agenda:
        hints = {{Button::dpad, "Up, down: scroll \xC2\xB7 Left, right: move"},
                 {Button::l1, "Pages", Button::r1},
                 {Button::circle, "Clock"}};
        if (agenda_state() == "failed")
            hints[0] = {Button::cross, "Retry"};
        break;
    case Page::timers:
        cross_words = timer_press_words();
        hints = {{Button::l1, "Pages", Button::r1},
                 {Button::circle, armed_timer_.empty() ? "Clock" : "Keep timer"}};
        if (!cross_words.empty())
            hints.insert(hints.begin(), {Button::cross, cross_words.c_str()});
        if (armed_timer_.empty())
            hints.insert(hints.begin(), {Button::dpad, "Move"});
        if (toast_.undo)
            hints.insert(hints.begin(), {Button::triangle, "Undo"});
        break;
    case Page::help:
        hints = {{Button::cross, help_column_ == 0     ? "Show examples"
                                 : read_only()         ? "Choose"
                                 : armed_example_ >= 0 ? "Confirm"
                                                       : "Try it"},
                 {Button::triangle, "Type a question"},
                 {Button::r2, "Hold to ask"},
                 {Button::l1, "Pages", Button::r1},
                 {Button::circle, armed_example_ >= 0 ? "Cancel" : "Clock"}};
        if (read_only() || !ready())
            hints.erase(hints.begin() + 1, hints.begin() + 3);
        if (armed_example_ < 0)
            hints.insert(hints.begin(), {Button::dpad, "Move"});
        break;
    case Page::info:
        hints = {
            {Button::dpad, "Scroll"}, {Button::l1, "Pages", Button::r1}, {Button::circle, "Clock"}};
        if (!config_.view_assist.empty() && !va_message_.empty() && answer_ == va_message_)
            hints.insert(hints.begin() + 1,
                         {Button::square, text(snapshot_.states.get(config_.view_assist).attributes,
                                               "mode") == "hold"
                                              ? "Stop keeping"
                                              : "Keep on screen"});
        break;
    case Page::setup:
        hints = {{Button::dpad, "Move"}, {Button::cross, "Choose"}, {Button::circle, "Back"}};
        break;
    case Page::pick:
        hints = {
            {Button::dpad, "Move"}, {Button::cross, "Choose"}, {Button::circle, "Back to Setup"}};
        break;
    case Page::quit:
        hints = {{Button::dpad, "Move"}, {Button::cross, "Choose"}, {Button::circle, "Back"}};
        break;
    }
    if (sheet_)
    {
        const int focus =
            rows.empty() ? 0 : std::clamp(sheet_->focus, 0, static_cast<int>(rows.size()) - 1);
        hints = rows.empty()
                    ? std::vector<hui::ui::Hint>{{Button::circle, "Done"}}
                    : sheet_hints(rows[static_cast<std::size_t>(focus)],
                                  sheet_cursor(rows[static_cast<std::size_t>(focus)]),
                                  sheet_->armed == rows[static_cast<std::size_t>(focus)].key());
    }
    // Ringing, there are two choices; once it has stopped, only one, and Circle does the same.
    if (alert_)
        hints = alert_still_listed() ? std::vector<hui::ui::Hint>{{Button::cross, "Stop"},
                                                                  {Button::circle, "Leave ringing"}}
                                     : std::vector<hui::ui::Hint>{{Button::cross, "OK"}};
    if (camera_showing())
        // R2 asks the assistant here too: it doesn't speak to whoever is at the door.
        hints = {{Button::circle, "Close the camera"}, {Button::r2, "Hold to ask assistant"}};
    if (talking_)
        hints = {{Button::r2, "Let go to send"}};
    hui::ui::HintLayout layout;
    layout.size = 44;
    layout.text_size = size::caption;
    layout.cy = 1012;
    layout.item_gap = 48;
    layout.font = &fonts_.text;
    auto style = hui::ui::GlyphStyle::dark();
    // Straight on a picture they need every bit of contrast they can get.
    style.label = page_ == Page::photos || (page_ == Page::home && has_clock_backdrop())
                      ? tone::white
                      : tone::muted;
    hui::ui::draw_hints(p.list, fonts_.kit, style, hints.data(), static_cast<int>(hints.size()),
                        size::margin, false, layout);
}
void Application::draw_toast(Paint &p) const
{
    if (toast_.text.empty())
        return;
    // A dark pill at the top centre over everything; the error pill is deep red. Help's reply
    // wraps to three lines (the one exception), and Undo grows the pill a button. Over a sheet it
    // sits on the hints instead, so the sheet's title stays readable.
    const float max_width = 1500, pad_x = 40, line_height = 46, sz = size::button;
    const bool wrap = toast_.wrap || toast_.error;
    const auto lines =
        wrap ? hui::ui::paragraph_lines(fonts_.bold, toast_.text, sz, max_width - 2 * pad_x, 3)
             : hui::ui::ParagraphLines{
                   {fonts_.bold.font->fit(toast_.text, sz,
                                          max_width - 2 * pad_x - (toast_.undo ? 260 : 0))},
                   false};
    float text_width = 0;
    for (const auto &l : lines.lines)
        text_width = std::max(text_width, p.measure(l, sz, true));
    const float undo_width = toast_.undo ? 230 : 0;
    const float h = wrap ? 40 + line_height * static_cast<float>(lines.lines.size()) : 96;
    const float w = text_width + 2 * pad_x + (undo_width ? undo_width + 24 : 0);
    const float top = sheet_ ? 1080 - 16 - h : page_ == Page::home ? 176.0f : 44.0f;
    const Rect r{960 - w / 2, top, w, h};
    p.list.shadow(r.inset(-4), wrap ? 44 : h / 2, 30, Color::rgb(0x000000, 0.5f));
    p.list.bordered_rect(r, wrap ? 44 : h / 2, toast_.error ? tone::error_ground : tone::toast, 2,
                         toast_.error ? tone::error_line : Color::rgb(0xffffff, 0.12f));
    float y = r.y + (wrap ? 20 + 34 : h / 2 + sz * 0.36f);
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
    case Page::music:
        return "Now playing";
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
        return "What can I say?";
    case Page::info:
        return "Assistant";
    case Page::photos:
        return "Photos";
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
        case Pick::photos:
            return "Photo folder";
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
    case Page::music:
        draw_music(p);
        break;
    case Page::house:
        draw_house(p);
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
    case Page::photos:
        draw_slideshow(p);
        break;
    case Page::setup:
    case Page::pick:
    case Page::quit:
        draw_setup(p);
        break;
    }
    draw_sheet(p);
    if (alert_)
        draw_alert(p);
    if (camera_showing())
        draw_camera(p);
    draw_talk(p);
    // On the slideshow the hints go after a few seconds, so nothing sits on the pictures.
    const bool hints_up =
        page_ != Page::photos || now() < photo_hints_until_ || sheet_ || alert_ || camera_showing();
    // Late at night they are no brighter than the slideshow's clock.
    const float hint_strength = page_ == Page::photos && late_at_night() ? 0.7f : 1.0f;
    p.list.push_opacity(page_ == Page::home && !alert_ && !camera_showing() ? 1 - ambient_
                        : hints_up                                          ? hint_strength
                                                                            : 0);
    draw_hints(p);
    p.list.pop_opacity();
    draw_toast(p);
}
} // namespace lamplight

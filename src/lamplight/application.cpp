// Lamplight for PS5 - Pages, focus, navigation and toasts.
// Copyright (C) 2026 Hayden Griffin
// SPDX-License-Identifier: GPL-3.0-or-later
#include "lamplight/application.hpp"
#include "lamplight/assistant.hpp"
#include "lamplight/paint.hpp"
#include <algorithm>

namespace lamplight
{
using hui::Action;
using hui::Direction;

Application::Application(Config c, Fonts fonts) : config_(std::move(c)), fonts_(fonts)
{
    auto theme = hui::ui::default_theme();
    theme.page = tone::page;
    theme.surface = tone::panel_solid;
    theme.surface_high = Color::rgb(0x22252b);
    theme.text = tone::text;
    theme.text_muted = tone::muted;
    theme.page_text = tone::text;
    theme.page_text_muted = tone::muted;
    theme.focus = tone::accent;
    theme.primary = tone::accent;
    theme.accent = tone::accent;
    theme.on_primary = tone::accent_ink;
    theme.style = hui::ui::SurfaceStyle::flat;
    theme.radius = size::small_radius;
    theme.radius_card = size::tile_radius;
    theme.border = 0;
    theme.focus_width = 5;
    theme.focus_gap = 6;
    list_.style.theme = theme;
    list_.style.row_height = 92;
    list_.style.title_size = size::body;
    list_.style.subtitle_size = size::caption;
    list_.style.value_size = size::label;
    list_.style.highlight.kind = hui::ui::HighlightKind::ring;
    list_.style.cards = false;
    list_.set_bounds({size::margin, size::content_top, 1060, 700});
    qr_ =
        encode_qr(config_.wifi_ssid.empty() ? ""
                                            : wifi_payload(config_.wifi_ssid, config_.wifi_password,
                                                           config_.wifi_security));
    if (!config_.configured())
        open(Page::setup);
}
std::vector<Page> Application::content_pages() const
{
    std::vector<Page> pages;
    if (!config_.room_page.empty())
        pages.push_back(Page::room);
    for (auto page :
         {Page::weather, Page::agenda, Page::timers, Page::wifi, Page::help, Page::info})
        pages.push_back(page);
    return pages;
}
void Application::resumed()
{
    // Rest mode suspended everything: what was fetched is stale, and the connection is new.
    forecast_.at = 0;
    forecast_failed_at_ = 0;
    calendar_signature_.clear();
    toast_ = {};
    idle_s_ = 0;
    ambient_ = 0;
}
std::set<std::string> Application::watched() const
{
    std::set<std::string> ids;
    for (const auto *id : {&config_.weather, &config_.view_assist, &config_.read_only})
        if (!id->empty())
            ids.insert(*id);
    for (const auto &c : config_.calendars)
        ids.insert(c.entity);
    for (const auto &a : config_.appliances)
        ids.insert(a.entity);
    if (!config_.room_page.media.empty())
        ids.insert(config_.room_page.media);
    for (const auto *list : {&config_.room_page.tiles, &config_.room_page.actions})
        for (const auto &t : *list)
            ids.insert(t.entity);
    return ids;
}
void Application::persist()
{
    if (save)
        save(config_);
}
// After Setup changes what the pages read: subscribe to it and forget what was loaded for the
// old choices.
void Application::apply_settings()
{
    persist();
    if (watch)
        watch(watched());
    qr_ =
        encode_qr(config_.wifi_ssid.empty() ? ""
                                            : wifi_payload(config_.wifi_ssid, config_.wifi_password,
                                                           config_.wifi_security));
    list_.style.reduced_motion = config_.reduced_motion;
}
std::uint64_t Application::dispatch(Request request, Job job, std::string key)
{
    const auto ticket = send ? send(std::move(request)) : 0;
    if (ticket)
        pending_[ticket] = {job, std::move(key)};
    return ticket;
}
void Application::toast(std::string text, bool error, std::int64_t ms, std::optional<Request> undo,
                        std::string undo_done, bool wrap)
{
    toast_ = {std::move(text), error, wrap, now() + ms, std::move(undo), std::move(undo_done)};
}
void Application::receive(Snapshot s)
{
    if (generation_ != s.generation)
    {
        generation_ = s.generation;
        fail_pending("Connection changed");
    }
    snapshot_ = std::move(s);
    // Home Assistant can answer in any order, so each result is handled once by its ticket.
    std::set<std::uint64_t> listed;
    for (const auto &r : snapshot_.results)
        if (listed.insert(r.ticket).second && handled_.insert(r.ticket).second)
            handle(r);
    std::erase_if(handled_, [&](std::uint64_t t) { return !listed.contains(t); });
    if (!snapshot_.ready)
        fail_pending("Not connected");
    refresh_lists();
    // The View Assist device's answer: newer news than one asked here earlier.
    if (!config_.view_assist.empty())
    {
        const auto &a = snapshot_.states.get(config_.view_assist).attributes;
        auto message = text(a, "message");
        if (!message.empty() && message != va_message_)
        {
            const bool first = va_message_.empty() && answer_at_ == 0;
            va_message_ = message;
            answer_ = message;
            answer_title_ = text(a, "title");
            answer_image_ = text(a, "image");
            answer_at_ = first ? 0 : now();
            info_scroll_ = 0;
        }
    }
    // Each picture is asked for once, when the answer changes to it.
    if (const auto url = answer_picture_url(); url != picture_url_)
    {
        picture_url_ = url;
        if (picture_.texture)
            released_.push_back(picture_.texture);
        picture_ = {};
        if (!url.empty() && request_picture)
            request_picture(url);
    }
    // A change in the timers clears the busy quick-start button.
    if (!config_.view_assist.empty())
    {
        auto signature =
            field(snapshot_.states.get(config_.view_assist).attributes, "timers").dump();
        if (signature != timers_signature_)
        {
            timers_signature_ = signature;
            busy_minutes_ = 0;
        }
    }
}
void Application::open(Page p)
{
    if (p == Page::setup && page_ != Page::pick)
        setup_focus_ = 0;
    page_ = p;
    armed_timer_.clear();
    armed_example_ = -1;
    if (p == Page::agenda)
        agenda_revealed_ = false;
    if (p == Page::room)
    {
        room_focus_ = 0;
        room_armed_ = -1;
    }
    if (p == Page::info)
        info_scroll_ = 0;
    if (p == Page::setup || p == Page::pick || p == Page::quit)
    {
        rebuild_list();
        list_.set_focus(p == Page::setup ? setup_focus_ : 0);
        list_.enter();
    }
}
std::string Application::answer_picture_url() const
{
    if (answer_image_.starts_with("/"))
        return "http://" + config_.host + ":" + std::to_string(config_.port) + answer_image_;
    return answer_image_;
}
void Application::picture(const std::string &url, std::uint32_t texture, int width, int height)
{
    if (!texture)
        return;
    // A picture for an answer that has since changed is not kept.
    if (url != picture_url_ || width <= 0 || height <= 0)
    {
        released_.push_back(texture);
        return;
    }
    if (picture_.texture)
        released_.push_back(picture_.texture);
    picture_ = {url, texture, width, height};
}
void Application::update(const hui::InputFrame &input, float dt, const WallClock &clock,
                         hui::ui::Feedback &feedback)
{
    clock_ = clock;
    // Home Assistant's time, when the console's is more than two seconds out: the clock, the
    // countdowns and "now" on Agenda then agree with the house.
    if (snapshot_.clock_known &&
        (snapshot_.clock_skew_ms > 2000 || snapshot_.clock_skew_ms < -2000))
        clock_.utc_ms += snapshot_.clock_skew_ms;
    time_ += dt;
    list_.update(dt);
    if (toast_.until && now() > toast_.until)
        toast_ = {};
    if (!armed_timer_.empty() && now() > armed_until_)
        armed_timer_.clear();
    if (armed_example_ >= 0 && now() > armed_example_until_)
        armed_example_ = -1;
    if (busy_minutes_ && now() > busy_until_)
        busy_minutes_ = 0;
    if (room_armed_ >= 0 && now() > room_armed_until_)
        room_armed_ = -1;
    if (!room_busy_.empty() && now() > room_busy_until_)
        room_busy_.clear();
    if (ready())
    {
        load_forecast(false);
        load_agenda();
    }
    check_alerts();
    // The Clock Home Rule, for a TV: after five minutes untouched every page goes back to the
    // clock, and two minutes later the clock rests (ambient: dimmer, chips gone, drifting slowly,
    // so a static picture can't burn into an OLED).
    const bool touched = input.pressed || input.nav != hui::Direction::none ||
                         input.stick_x * input.stick_x + input.stick_y * input.stick_y > 0.04f;
    idle_s_ = touched || alert_ ? 0 : idle_s_ + dt;
    if (idle_s_ > 300 && page_ != Page::home && page_ != Page::quit)
        open(Page::home);
    const float target = page_ == Page::home && idle_s_ > 120 ? 1.0f : 0.0f;
    const float step = dt / (target > ambient_ ? 3.0f : 0.4f);
    ambient_ =
        target > ambient_ ? std::min(target, ambient_ + step) : std::max(target, ambient_ - step);
    if (input.focus_lost)
    {
        local_blocked_ = true;
        return;
    }
    if (!input.connected)
        return;
    // The first press after the clock has rested only wakes it.
    if (touched && ambient_ > 0.5f && !alert_)
    {
        ambient_ = 0.49f;
        return;
    }
    // After the system menu, wait for every button to be let go.
    if (local_blocked_)
    {
        if (!input.held && !input.pressed)
            local_blocked_ = false;
        return;
    }
    navigate(input, feedback);
}
void Application::navigate(const hui::InputFrame &input, hui::ui::Feedback &feedback)
{
    if (alert_)
    {
        update_alert(input);
        return;
    }
    if (input.is_pressed(Action::menu))
    {
        open(page_ == Page::setup ? Page::home : Page::setup);
        return;
    }
    // Undo, while a toast offers it.
    if (input.is_pressed(Action::north) && toast_.undo && page_ != Page::help)
    {
        auto undo = std::move(*toast_.undo);
        auto done = toast_.undo_done;
        toast_ = {};
        if (dispatch(std::move(undo), Job::undo_timer))
            toast(done);
        else
            toast("Not connected; nothing was set", true, 5000);
        return;
    }
    if (input.is_pressed(Action::back))
    {
        if (page_ == Page::pick)
            open(Page::setup);
        else
            open(page_ == Page::home ? Page::quit : Page::home);
        return;
    }
    const auto pages = content_pages();
    const auto at = std::find(pages.begin(), pages.end(), page_);
    if (at != pages.end() &&
        (input.is_pressed(Action::page_prev) || input.is_pressed(Action::page_next)))
    {
        const int count = static_cast<int>(pages.size());
        const int index = static_cast<int>(at - pages.begin());
        const int step = input.is_pressed(Action::page_next) ? 1 : -1;
        open(pages[static_cast<std::size_t>((index + step + count) % count)]);
        return;
    }
    switch (page_)
    {
    case Page::home:
        if (input.nav == Direction::left || input.nav == Direction::right)
        {
            const int count = static_cast<int>(pages.size());
            home_focus_ =
                std::clamp(home_focus_ + (input.nav == Direction::left ? -1 : 1), 0, count - 1);
        }
        if (input.is_pressed(Action::confirm))
            open(pages[static_cast<std::size_t>(
                std::clamp(home_focus_, 0, static_cast<int>(pages.size()) - 1))]);
        break;
    case Page::weather:
        if (input.is_pressed(Action::confirm) && forecast_state() == "failed")
            load_forecast(true);
        break;
    case Page::room:
        update_room(input);
        break;
    case Page::agenda:
        update_agenda(input);
        break;
    case Page::timers:
        update_timers(input);
        break;
    case Page::help:
        update_help(input);
        break;
    case Page::info:
        update_info(input);
        break;
    case Page::setup:
    case Page::pick:
    case Page::quit:
        if (list_.handle(input, feedback) == hui::ui::Event::activated)
        {
            if (page_ == Page::setup)
                activate_setup(list_.focus());
            else if (page_ == Page::pick)
                activate_pick(list_.focus());
            else if (list_.focus() == 1)
                quitting_ = true;
            else
                open(Page::home);
        }
        if (page_ == Page::setup)
            setup_focus_ = list_.focus();
        break;
    case Page::wifi:
        break;
    }
}
} // namespace lamplight

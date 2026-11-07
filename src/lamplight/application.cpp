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

namespace
{
constexpr std::int64_t kPictureRetryMs = 60000;
} // namespace

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
    list_.style.focus_shift = 0;
    list_.set_bounds({size::margin + 16, size::content_top + 16, 1028, 688});
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
    if (!config_.rooms.empty())
        pages.push_back(Page::room);
    if (!music_speaker().empty())
        pages.push_back(Page::music);
    if (!config_.notices.entity.empty() || !config_.house.empty())
        pages.push_back(Page::house);
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
    chart_ = {};
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
    if (!config_.settings_entity.empty())
        ids.insert(config_.settings_entity);
    ids.insert(group_lamps_.begin(), group_lamps_.end());
    ids.insert(house_lamps_.begin(), house_lamps_.end());
    for (const auto &mode : config_.house.modes)
        ids.insert(mode.entity);
    if (!config_.speakers.group.empty())
        ids.insert(config_.speakers.group);
    for (const auto &speaker : speaker_list(config_))
    {
        ids.insert(speaker.player);
        if (!speaker.sleep_timer.empty())
            ids.insert(speaker.sleep_timer);
    }
    // A picture named by an entity's background attribute.
    auto add_picture_entity = [&](const std::string &source)
    {
        if (source.find('.') != std::string::npos && source.find('/') == std::string::npos)
            ids.insert(source);
    };
    add_picture_entity(config_.background);
    if (!config_.notices.entity.empty())
        ids.insert(config_.notices.entity);
    if (!config_.doorbell.empty())
    {
        ids.insert(config_.doorbell.trigger);
        ids.insert(config_.doorbell.camera);
    }
    for (const auto &room : config_.rooms)
    {
        if (!room.media.empty())
            ids.insert(room.media);
        add_picture_entity(room.photo);
        if (const auto light = room_light(room); !light.empty())
            ids.insert(light);
        for (const auto &scene : room.scenes)
            if (!scene.entity.empty())
                ids.insert(scene.entity);
        for (const auto *list : {&room.tiles, &room.actions})
            for (const auto &t : *list)
            {
                ids.insert(t.entity);
                if (!t.sensor.empty())
                    ids.insert(t.sensor);
            }
    }
    return ids;
}
void Application::persist()
{
    if (save)
        save(config_);
}
// Home Assistant's settings entity: its "lamplight" attribute (rooms, Help's categories, the
// doorbell...) over these settings, each time it changes. The connection is never taken from it.
void Application::apply_remote_settings()
{
    if (config_.settings_entity.empty() || !snapshot_.ready)
        return;
    const auto &state = snapshot_.states.get(config_.settings_entity);
    const auto &settings = field(state.attributes, "lamplight");
    if (!settings.is_object())
        return;
    auto signature = settings.dump();
    if (signature == remote_signature_)
        return;
    remote_signature_ = std::move(signature);
    const auto before = config_;
    config_ = merged(config_, settings, false);
    sheet_.reset();
    settings_.clear();
    speaker_move_.reset();
    room_hold_ = -1;
    // A second press belongs to what the first one was for, which may be gone or have changed.
    disarm();
    forget_changed(before);
    if (page_ == Page::room && config_.rooms.empty())
        open(Page::home);
    apply_settings();
}
void Application::forget_changed(const Config &before)
{
    // Answers on their way for the old choices are dropped (their results find no job).
    auto drop = [&](std::initializer_list<Job> jobs)
    {
        std::erase_if(pending_, [&](const auto &p)
                      { return std::find(jobs.begin(), jobs.end(), p.second.job) != jobs.end(); });
    };
    if (before.weather != config_.weather)
    {
        drop({Job::hours, Job::days});
        forecast_ = {};
        forecast_failed_at_ = 0;
        forecast_loading_ = forecast_failed_part_ = false;
        forecast_parts_ = 0;
        hours_loading_.clear();
        days_loading_.clear();
    }
    if (before.notices.entity != config_.notices.entity)
    {
        // Another list: the old rows can't be dismissed through the new one's event.
        drop({Job::dismiss});
        notices_.clear();
        notices_seen_.clear();
        notices_known_ = false;
        house_focus_ = 0;
    }
    if (before.photos != config_.photos)
    {
        drop({Job::photo_list, Job::photo_url});
        photo_busy_ = false;
        photo_list_.clear();
        photo_found_.clear();
        photo_walk_.clear();
        photo_walked_.clear();
        photo_listed_at_ = photo_at_ = 0;
        want_picture(Pic::photo, "", true);
        release(previous_photo_);
        if (page_ == Page::photos && config_.photos.empty())
            open(Page::home);
    }
    if (before.doorbell.camera != config_.doorbell.camera ||
        before.doorbell.trigger != config_.doorbell.trigger)
    {
        doorbell_seen_.clear();
        close_camera();
    }
    room_index_ = std::min(room_index_, std::max(0, static_cast<int>(config_.rooms.size()) - 1));
    // Help draws its chosen category even with no controller connected to move it.
    help_category_ =
        std::min(help_category_, std::max(0, static_cast<int>(config_.help().size()) - 1));
}
// After Setup changes what the pages read: subscribe to it and forget what was loaded for the
// old choices.
void Application::apply_settings()
{
    persist();
    if (watch)
        watch(config_, watched());
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
        if (photo_list_.empty())
            photo_listed_at_ = 0;
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
    apply_remote_settings();
    track_groups();
    update_notices();
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
bool Application::disarm()
{
    const bool armed = room_armed_ >= 0 || !armed_timer_.empty() || armed_example_ >= 0;
    room_armed_ = -1;
    armed_timer_.clear();
    armed_example_ = -1;
    return armed;
}
void Application::open(Page p)
{
    if (p == Page::setup && page_ != Page::pick)
        setup_focus_ = 0;
    page_ = p;
    disarm();
    sheet_.reset();
    room_hold_ = -1;
    if (p == Page::agenda)
        agenda_revealed_ = false;
    if (p == Page::room)
    {
        room_focus_ = first_room_focus();
        room_armed_ = -1;
    }
    if (p == Page::info)
        info_scroll_ = 0;
    if (p == Page::music)
        music_focus_ = 1;
    if (p == Page::house)
        house_focus_ = 0;
    if (p == Page::photos)
    {
        photo_paused_ = false;
        photo_hints_until_ = now() + 6000;
    }
    if (p == Page::setup || p == Page::pick || p == Page::quit)
    {
        rebuild_list();
        list_.set_focus(p == Page::setup ? setup_focus_ : p == Page::pick ? pick_current_row() : 0);
        list_.enter();
    }
}
std::string Application::ha_url(const std::string &path) const
{
    if (path.starts_with("/"))
        return "http://" + config_.host + ":" + std::to_string(config_.port) + path;
    return path;
}
std::string Application::answer_picture_url() const
{
    return ha_url(answer_image_);
}
void Application::want_picture(Pic which, const std::string &url, bool keep)
{
    auto &s = pics_[static_cast<std::size_t>(which)];
    s.keep = keep;
    if (url == s.wanted && (!s.retry_at || now() < s.retry_at))
        return;
    s.retry_at = 0;
    s.wanted = url;
    s.waiting = !url.empty() && request_picture;
    if ((url.empty() || !keep) && s.shown.texture)
        release(s.shown);
    if (s.waiting)
        request_picture(url);
}
const Application::Shown *Application::shown_picture(Pic which) const
{
    const auto &s = slot(which);
    if (!s.shown.texture || (!s.keep && s.shown.url != s.wanted))
        return nullptr;
    return &s.shown;
}
void Application::release(Shown &shown)
{
    if (shown.texture)
        released_.push_back(shown.texture);
    if (shown.soft)
        released_.push_back(shown.soft);
    shown = {};
}
void Application::picture(const std::string &url, std::uint32_t texture, int width, int height,
                          std::uint32_t soft, float brightness)
{
    // A picture for something that has since changed is not kept. The loader fetches a URL once
    // at a time, so a second slot waiting on the same one asks again for its own copy.
    bool taken = false;
    for (auto &s : pics_)
        if (s.waiting && url == s.wanted)
        {
            if (taken)
            {
                request_picture(url);
                continue;
            }
            s.waiting = false;
            const bool is_photo = &s == &pics_[static_cast<std::size_t>(Pic::photo)];
            if (!texture || width <= 0 || height <= 0)
            {
                if (is_photo)
                {
                    // Not left as wanted: the same address is worth fetching again.
                    s.wanted.clear();
                    photo_failed();
                }
                // The backdrop asks again by itself (update_backdrop).
                else if (&s != &pics_[static_cast<std::size_t>(Pic::backdrop)])
                    s.retry_at = now() + kPictureRetryMs;
                continue;
            }
            if (is_photo)
            {
                // Its time starts now it shows, however long it took to come.
                photo_at_ = now();
                // The photo it replaces stays to fade out under it.
                release(previous_photo_);
                previous_photo_ = s.shown;
                previous_photo_t_ = std::min(
                    1.0f, photo_age_s_ / static_cast<float>(std::max(config_.photo_seconds, 1)));
                photo_age_s_ = 0;
            }
            else
                release(s.shown);
            s.shown = {url, texture, width, height, soft, brightness};
            taken = true;
        }
    if (taken)
        return;
    if (texture)
        released_.push_back(texture);
    if (soft)
        released_.push_back(soft);
}
void Application::update(const hui::InputFrame &input, float dt, const WallClock &clock,
                         hui::ui::Feedback &feedback)
{
    clock_ = clock;
    clock_.zone.twelve_hour = config_.clock == "12h" ||
                              (config_.clock == "auto" && twelve_hour_country(snapshot_.country));
    // Home Assistant's time, when the console's is more than two seconds out: the clock, the
    // countdowns and "now" on Agenda then agree with the house.
    if (snapshot_.clock_known &&
        (snapshot_.clock_skew_ms > 2000 || snapshot_.clock_skew_ms < -2000))
        clock_.utc_ms += snapshot_.clock_skew_ms;
    time_ += dt;
    frame_dt_ = dt;
    if (room_hold_ >= 0)
        room_hold_s_ += dt;
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
    if (sheet_ && !sheet_->armed.empty() && now() > sheet_->armed_until)
        sheet_->armed.clear();
    if (!room_busy_.empty() && now() > room_busy_until_)
        room_busy_.clear();
    if (ready())
    {
        load_forecast(false);
        load_agenda();
    }
    check_alerts();
    check_doorbell();
    refresh_camera();
    update_photos();
    send_volume();
    send_settings();
    check_speaker_move();
    update_chart();
    update_backdrop();
    want_picture(Pic::art, cover_url(), false);
    // Each picture is asked for once, when the answer changes to it.
    want_picture(Pic::answer, answer_picture_url(), false);
    // The Clock Home Rule, for a TV: after five minutes untouched every page goes back to the
    // clock, and two minutes later the clock rests (ambient: dimmer, chips gone, drifting slowly,
    // so a static picture can't burn into an OLED).
    const bool touched = input.pressed || input.nav != hui::Direction::none ||
                         input.stick_x * input.stick_x + input.stick_y * input.stick_y > 0.04f ||
                         input.stick2_x * input.stick2_x + input.stick2_y * input.stick2_y > 0.04f;
    idle_s_ = touched || alert_ || camera_showing() ? 0 : idle_s_ + dt;
    // The slideshow is what was asked for, so it stays; its pictures change by themselves.
    if (idle_s_ > 300 && page_ != Page::home && page_ != Page::quit && page_ != Page::photos)
        open(Page::home);
    const float target = page_ == Page::home && idle_s_ > 120 ? 1.0f : 0.0f;
    const float step = dt / (target > ambient_ ? 3.0f : 0.4f);
    ambient_ =
        target > ambient_ ? std::min(target, ambient_ + step) : std::max(target, ambient_ - step);
    update_talk(input, dt);
    if (input.focus_lost)
    {
        // A hold cut short is not a tap: nothing presses when the buttons come back.
        room_hold_ = -1;
        local_blocked_ = true;
        return;
    }
    if (!input.connected)
    {
        room_hold_ = -1;
        return;
    }
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
    if (camera_showing())
    {
        room_hold_ = -1;
        update_camera(input);
        return;
    }
    if (alert_)
    {
        room_hold_ = -1;
        update_alert(input);
        return;
    }
    if (sheet_)
    {
        update_sheet(input);
        return;
    }
    if (input.is_pressed(Action::menu))
    {
        open(page_ == Page::setup ? Page::home : Page::setup);
        return;
    }
    // L2, from any page: the whole-house actions and the modes.
    if (input.is_pressed(Action::jump_prev) && page_ != Page::setup && page_ != Page::pick &&
        page_ != Page::quit && open_house_sheet())
    {
        room_hold_ = -1;
        return;
    }
    // Undo, while a toast offers it.
    if (input.is_pressed(Action::north) && toast_.undo)
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
        // With a second press waiting, Circle backs out of that, not off the page.
        if (disarm())
            return;
        if (page_ == Page::pick && pick_ == Pick::photos && folder_path_.size() > 1)
        {
            folder_path_.pop_back();
            browse_folder();
        }
        else if (page_ == Page::pick)
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
        else if (input.is_pressed(Action::north) && !config_.photos.empty())
            open(Page::photos);
        break;
    case Page::weather:
        if (input.is_pressed(Action::confirm) && forecast_state() == "failed")
            load_forecast(true);
        break;
    case Page::room:
        update_room(input);
        break;
    case Page::music:
        update_music(input);
        break;
    case Page::house:
        update_house(input);
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
    case Page::photos:
        update_slideshow(input);
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

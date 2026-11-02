// Lamplight for PS5 - Pages, focus, data loading and actions.
// Copyright (C) 2026 Hayden Griffin
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "lamplight/agenda.hpp"
#include "lamplight/client.hpp"
#include "lamplight/clock.hpp"
#include "lamplight/fonts.hpp"
#include "lamplight/timers.hpp"
#include "lamplight/weather.hpp"
#include "lamplight/wifi_code.hpp"
#include "platform/ps5/keyboard.hpp"
#include "ui/components/list.hpp"
#include <functional>
#include <deque>
#include <map>
#include <optional>
#include <set>
#include <utility>

namespace lamplight
{
enum class Page
{
    home,
    room,
    weather,
    agenda,
    timers,
    wifi,
    help,
    info,
    setup,
    pick,
    quit
};
// What a Setup list chooses.
enum class Pick
{
    weather,
    calendars,
    view_assist,
    appliances,
    agent,
    read_only
};
class Paint;
class Application
{
  public:
    Application(Config config, Fonts fonts);
    // Wiring to the client, the storage worker and the picture loader.
    std::function<std::uint64_t(Request)> send;
    std::function<void(const Config &)> save;
    std::function<void(const Config &, std::set<std::string>)> reconnect;
    std::function<void(std::set<std::string>)> watch;
    std::function<void()> refresh_catalog;
    std::function<void(const std::string &)> request_picture;

    void receive(Snapshot snapshot);
    void update(const hui::InputFrame &input, float dt, const WallClock &clock,
                hui::ui::Feedback &feedback);
    void draw(hui::gfx::DrawList &list) const;
    void open(Page page);
    void keyboard_result(const hui::ps5::KeyboardOutcome &result);
    std::optional<hui::ps5::KeyboardOptions> take_keyboard();
    // A picture the loader finished: texture 0 when it could not be loaded.
    void picture(const std::string &url, std::uint32_t texture, int width, int height);
    // Textures the app no longer shows, for main to delete.
    std::vector<std::uint32_t> take_released()
    {
        return std::exchange(released_, {});
    }
    // The answer's picture as an http:// URL ("" for none); a path is on Home Assistant.
    std::string answer_picture_url() const;
    void storage_status(std::string status)
    {
        storage_status_ = std::move(status);
    }
    // The entities the pages read, for the client's subscription.
    std::set<std::string> watched() const;
    // The console woke from rest mode: everything is fetched again and the clock face comes back.
    void resumed();
    // A timer or an appliance has finished and its alert is up: main pulses the light bar.
    bool alerting() const
    {
        return alert_.has_value();
    }
    // The pages in the order of the clock's chips (and of L1 and R1).
    std::vector<Page> content_pages() const;
    bool quitting() const
    {
        return quitting_;
    }
    const Config &config() const
    {
        return config_;
    }
    Page page() const
    {
        return page_;
    }
    const std::string &toast_text() const
    {
        return toast_.text;
    }

  private:
    struct Toast
    {
        std::string text;
        bool error = false, wrap = false;
        std::int64_t until = 0;
        std::optional<Request> undo;
        std::string undo_done;
    };
    enum class Job
    {
        hours,
        days,
        events,
        start_timer,
        cancel_timer,
        undo_timer,
        keep,
        ask,
        room
    };
    struct Pending
    {
        Job job;
        std::string key;
    };
    struct Forecast
    {
        std::vector<Hour> hours;
        std::vector<Day> days;
        std::int64_t at = 0;
    };
    struct TimerSlot // a focusable thing on the Timers page
    {
        int column = 0;
        std::string timer_id; // a Cancel pill, or empty for a quick-start button
        int minutes = 0;
        bool appliance = false; // read only: it can be scrolled to, not cancelled
    };
    // A thing on the room page the controller can move to.
    struct RoomSlot
    {
        enum class Kind
        {
            play_pause,
            next,
            action,
            tile
        } kind;
        int index = 0;
        hui::gfx::Rect rect;
    };
    struct Alert
    {
        std::string key, title, detail, icon, timer_id;
        std::int64_t since = 0;
    };
    // application.cpp
    void navigate(const hui::InputFrame &input, hui::ui::Feedback &feedback);
    void toast(std::string text, bool error = false, std::int64_t ms = 2500,
               std::optional<Request> undo = std::nullopt, std::string undo_done = "",
               bool wrap = false);
    std::uint64_t dispatch(Request request, Job job, std::string key = "");
    void persist();
    void apply_settings();
    bool ready() const
    {
        return snapshot_.ready;
    }
    std::int64_t now() const
    {
        return clock_.utc_ms;
    }
    // data.cpp
    void load_forecast(bool force);
    void load_agenda();
    void handle(const Result &result);
    void finish_forecast();
    void finish_agenda();
    void fail_pending(const std::string &why);
    std::string forecast_state() const;
    std::string agenda_state() const;
    // actions.cpp
    void update_timers(const hui::InputFrame &input);
    std::vector<TimerSlot> timer_slots() const;
    void start_timer(int minutes);
    void cancel_timer(const std::string &id);
    void update_help(const hui::InputFrame &input);
    void try_example(int index);
    void ask_assistant(const std::string &words, const std::string &agent, bool follow_up);
    bool read_only() const;
    void update_info(const hui::InputFrame &input);
    void update_room(const hui::InputFrame &input);
    void press_room(const RoomSlot &slot);
    // alerts.cpp
    void check_alerts();
    void update_alert(const hui::InputFrame &input);
    void raise_alert(Alert alert);
    void next_alert();
    // The alert's timer is still in View Assist's list (it rings until cancelled); View Assist
    // can drop a finished timer by itself, and cancelling it then is an error.
    bool alert_still_listed() const;
    void update_agenda(const hui::InputFrame &input);
    // setup.cpp
    void rebuild_list();
    // Setup's rows follow the connection and the catalogue, keeping the focus on the same row.
    void refresh_lists();
    void activate_setup(int index);
    void activate_pick(int index);
    void open_pick(Pick pick);
    std::vector<CatalogEntry> pick_options() const;
    void request_keyboard(int kind);
    // view*.cpp
    void draw_background(Paint &p) const;
    void draw_header(Paint &p, const std::string &title, const std::string &subtitle,
                     hui::gfx::Color subtitle_colour) const;
    void draw_hints(Paint &p) const;
    void draw_toast(Paint &p) const;
    void draw_home(Paint &p) const;
    void draw_weather(Paint &p) const;
    void draw_agenda(Paint &p) const;
    void draw_agenda_day(Paint &p, const hui::gfx::Rect &r, const char *label, std::int64_t day,
                         int column) const;
    struct AgendaRow
    {
        Event event;
        bool live = false, past = false;
        float height = 0;
    };
    std::vector<AgendaRow> agenda_rows(int column) const;
    // The first row shown: what's on now (or next) in view, until the controller scrolls.
    int agenda_first(int column) const;
    int agenda_last_first(int column) const;
    void draw_timers(Paint &p) const;
    void draw_wifi(Paint &p) const;
    void draw_help(Paint &p) const;
    void draw_info(Paint &p) const;
    void draw_room(Paint &p) const;
    void draw_alert(Paint &p) const;
    std::vector<RoomSlot> room_slots() const;
    std::string room_title() const;
    void draw_setup(Paint &p) const;
    std::string page_title() const;
    std::string home_status(Page page, std::string &icon, bool &warm) const;
    std::string timers_subtitle() const;
    std::string agenda_subtitle() const;

    Config config_;
    Fonts fonts_;
    Snapshot snapshot_;
    WallClock clock_;
    std::int64_t mono_us_ = 0;
    float time_ = 0;
    Page page_ = Page::home;
    Toast toast_;
    std::map<std::uint64_t, Pending> pending_;
    std::set<std::uint64_t> handled_;
    std::uint64_t generation_ = 0;
    bool quitting_ = false, local_blocked_ = false;
    std::string storage_status_;
    // Home
    int home_focus_ = 0;
    // Weather
    Forecast forecast_;
    std::vector<Hour> hours_loading_;
    std::vector<Day> days_loading_;
    int forecast_parts_ = 0;
    bool forecast_loading_ = false, forecast_failed_part_ = false;
    std::int64_t forecast_failed_at_ = 0;
    // Agenda
    std::vector<Event> events_, events_loading_;
    bool agenda_loading_ = false, agenda_loaded_ = false;
    int agenda_waiting_ = 0, agenda_failed_ = 0, agenda_read_ = 0, agenda_failed_loading_ = 0;
    std::int64_t agenda_at_ = 0, agenda_day_ = 0;
    std::string calendar_signature_;
    int agenda_column_ = 0;
    int agenda_scroll_[2] = {0, 0};
    bool agenda_revealed_ = false;
    // Timers
    std::string timers_signature_;
    int timer_column_ = 1, timer_row_[2] = {0, 0};
    std::string armed_timer_;
    std::int64_t armed_until_ = 0;
    int busy_minutes_ = 0;
    std::int64_t busy_until_ = 0;
    // Help
    int help_column_ = 0, help_category_ = 0, help_example_ = 0;
    int armed_example_ = -1;
    std::int64_t armed_example_until_ = 0;
    std::uint64_t asking_ = 0;
    int asking_example_ = -1;
    std::string conversation_;
    std::int64_t conversation_at_ = 0;
    // Info: the assistant's last answer, from View Assist or asked here
    std::string answer_, answer_title_, answer_image_, va_message_;
    std::int64_t answer_at_ = 0;
    int info_scroll_ = 0;
    // Only the current answer's picture is kept; one it replaces goes back to main to delete.
    struct Shown
    {
        std::string url;
        std::uint32_t texture = 0;
        int width = 0, height = 0;
    };
    Shown picture_;
    std::string picture_url_;
    std::vector<std::uint32_t> released_;
    // Names and icons of calendars and appliances turned off in Setup, for turning them on again.
    std::map<std::string, Choice> set_aside_;
    // Wi-Fi
    QrCode qr_;
    // Room
    int room_focus_ = 0, room_armed_ = -1;
    std::int64_t room_armed_until_ = 0, room_busy_until_ = 0;
    std::string room_busy_;
    // Alerts: what has been alerted (so each fires once), and each running appliance's finish
    std::optional<Alert> alert_;
    std::deque<Alert> waiting_alerts_; // finished while another alert was up: shown next
    std::set<std::string> alerted_;
    struct Seen
    {
        std::int64_t at = 0;
        std::string name, kind;
    };
    std::map<std::string, Seen> seen_running_;
    std::map<std::string, std::int64_t> appliance_finish_;
    // The clock face at rest: seconds since the last button, and how far into ambient it is (0-1)
    float idle_s_ = 0, ambient_ = 0;
    // Setup and its lists
    hui::ui::ListView list_;
    Pick pick_ = Pick::weather;
    // The entity each picker row stands for ("" for None), so a press picks what was shown.
    std::vector<std::string> pick_ids_;
    bool lists_ready_ = false;
    std::size_t lists_catalog_ = 0;
    std::optional<hui::ps5::KeyboardOptions> keyboard_;
    int keyboard_kind_ = 0;
    int setup_focus_ = 0;
};
} // namespace lamplight

// Lamplight for PS5 - Pages, focus, data loading and actions.
// Copyright (C) 2026 Hayden Griffin
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "lamplight/agenda.hpp"
#include "lamplight/client.hpp"
#include "lamplight/clock.hpp"
#include "lamplight/controls.hpp"
#include "lamplight/fonts.hpp"
#include "lamplight/history.hpp"
#include "lamplight/notices.hpp"
#include "lamplight/speakers.hpp"
#include "lamplight/timers.hpp"
#include "lamplight/weather.hpp"
#include "lamplight/wifi_code.hpp"
#include "platform/ps5/keyboard.hpp"
#include "ui/components/list.hpp"
#include <array>
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
    music,
    house,
    weather,
    agenda,
    timers,
    wifi,
    help,
    info,
    photos,
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
    read_only,
    photos
};
class Paint;
// Cross held this long on a tile opens its sheet, as the tablets' hold.
inline constexpr float kRoomHoldS = 0.5f;
// The pictures in a media-source folder Home Assistant browsed (media_source/browse_media).
std::vector<std::string> photo_ids(const Json &browsed);
// A folder's own name, not its whole address.
std::string photo_folder_name(const std::string &id);
// The folders in it, as (id, title).
std::vector<std::pair<std::string, std::string>> photo_folders(const Json &browsed);
// How far through its month the moon is: 0 new, 0.5 full.
double moon_phase(std::int64_t utc_ms);
// How strongly the moon draws: quieter late at night and in deep rest, so the time stays the
// brightest thing on the screen.
float moon_strength(bool late, float deep);
// How dark the shade over a page's picture is: as strong as that picture needs.
float page_shade(float brightness);
// How strongly the slideshow draws a picture: full, except late at night, when a bright one would
// light the room.
float slideshow_strength(bool late, float brightness);
// Whether a doorbell trigger's change from before to now is a ring.
bool doorbell_rang(const std::string &trigger, const std::string &before, const std::string &now);
class Application
{
  public:
    Application(Config config, Fonts fonts);
    // Wiring to the client, the storage worker and the picture loader.
    std::function<std::uint64_t(Request)> send;
    std::function<void(const Config &)> save;
    std::function<void(const Config &, std::set<std::string>)> reconnect;
    std::function<void(const Config &, std::set<std::string>)> watch;
    std::function<void()> refresh_catalog;
    std::function<void(const std::string &)> request_picture;
    // Push-to-talk: a run on the client, the microphone opened (false: none) and closed (which
    // ends the run's audio), and how loud it is.
    std::function<std::uint64_t(const std::string &, const std::string &)> listen;
    std::function<bool()> open_microphone;
    std::function<void()> close_microphone;
    std::function<float()> microphone_level;

    void receive(Snapshot snapshot);
    void update(const hui::InputFrame &input, float dt, const WallClock &clock,
                hui::ui::Feedback &feedback);
    void draw(hui::gfx::DrawList &list) const;
    void open(Page page);
    void keyboard_result(const hui::ps5::KeyboardOutcome &result);
    std::optional<hui::ps5::KeyboardOptions> take_keyboard();
    // A picture the loader finished: texture 0 when it could not be loaded; soft is its blurred
    // copy, when it was asked for one (kSoftPicture).
    void picture(const std::string &url, std::uint32_t texture, int width, int height,
                 std::uint32_t soft = 0, float brightness = 0.5f);
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
    // A timer or an appliance has finished and its alert is up, or the doorbell rang: main
    // pulses the light bar.
    bool alerting() const
    {
        return alert_.has_value() || (camera_showing() && watching_.empty());
    }
    bool camera_showing() const
    {
        return camera_until_ && now() < camera_until_;
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
    std::string page_title() const;
    const std::string &toast_text() const
    {
        return toast_.text;
    }
    bool sheet_showing() const
    {
        return sheet_.has_value();
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
        room,
        setting,
        photo_list,
        photo_url,
        folder,
        dismiss,
        said,     // what it did goes in a toast: the service's own words, or the key's
        history,  // a sheet's chart
        snapshot, // the lamps noted, so the action waiting on it can go
        undoable  // a house action: its Undo is offered once it has finished, under the key's words
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
    void apply_remote_settings();
    // What was fetched for settings that changed is forgotten (Setup does the same).
    void forget_changed(const Config &before);
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
    void finish_room_hold(const hui::InputFrame &input, const std::vector<RoomSlot> &slots);
    void adjust_with_right_stick(const hui::InputFrame &input, const RoomSlot &slot);
    // The entity a slot on the room page stands for, and what it is called.
    std::string slot_entity(const RoomSlot &slot) const;
    std::string slot_name(const RoomSlot &slot) const;
    std::string slot_icon(const RoomSlot &slot) const;
    bool slot_confirms(const RoomSlot &slot) const;
    // The level the right stick sets from the page: none for a tile that asks, or that the
    // house's heating plan runs.
    std::optional<Control> slot_level(const RoomSlot &slot) const;
    int first_room_focus() const;
    const char *press_words(const RoomSlot &slot) const;
    std::string timer_press_words() const;
    // sheet.cpp
    bool open_sheet(const std::string &entity, const std::string &name, const std::string &icon);
    bool open_slot_sheet(const RoomSlot &slot);
    bool open_lamps_sheet();
    bool room_has_lamps() const;
    bool open_house_sheet();
    bool open_speakers_sheet();
    bool has_speakers_sheet() const;
    std::vector<Control> sheet_rows() const;
    std::vector<Control> thing_rows() const;
    std::string sheet_words(const std::vector<Control> &rows, bool &lit) const;
    // A press whose outcome goes in a toast.
    void run_called(Request request, const std::string &done);
    // Everything off, Good night: the lamps are noted first, so Undo can put them back.
    void run_noting_lamps(const ControlOption &option);
    void finish_house_action(const ControlOption &option, bool can_undo);
    void offer_lamps_undo(const std::string &done);
    std::vector<std::string> undo_lamps() const;
    void check_speaker_move();
    void update_chart();
    void draw_chart(Paint &p, const Control &row, const hui::gfx::Rect &r) const;
    void update_sheet(const hui::InputFrame &input);
    void press_sheet_row(const Control &row);
    int sheet_cursor(const Control &row) const;
    // A slider's level: shown at once, and sent as it settles.
    void adjust(const Control &control, double value);
    void drop_settings(const std::string &entity);
    void drop_overridden_settings(const Control &row);
    Control with_latest_other_end(Control control) const;
    void adjust_row(const std::vector<Control> &rows, const Control &row, double value);
    void add_all_lamps(std::vector<Control> &rows) const;
    double shown_value(const Control &control) const;
    double current_level(const Control &control) const;
    void send_settings();
    void draw_sheet(Paint &p) const;
    void draw_sheet_row(Paint &p, const Control &row, const hui::gfx::Rect &r, bool focused) const;
    // The lamps inside the rooms' light groups, once Home Assistant has named them.
    void track_groups();
    // music.cpp
    // The speaker Now Playing is about: the current room's, or the first room's that has one.
    std::string music_speaker() const;
    // What its controls act on: the group, while it is playing with the group.
    std::string music_player() const;
    Entity music_state() const;
    void update_music(const hui::InputFrame &input);
    void send_volume();
    // The cover of what's playing, while Now Playing is open.
    std::string cover_url() const;
    void press_music(int control);
    // Where the track is now, in seconds (Home Assistant's position moved on by the clock).
    double music_position(const Entity &player) const;
    // alerts.cpp
    void check_alerts();
    void update_alert(const hui::InputFrame &input);
    void raise_alert(Alert alert);
    void next_alert();
    // The alert's timer is still in View Assist's list (it rings until cancelled); View Assist
    // can drop a finished timer by itself, and cancelling it then is an error.
    bool alert_still_listed() const;
    void update_agenda(const hui::InputFrame &input);
    // house.cpp
    void update_notices();
    void update_house(const hui::InputFrame &input);
    void draw_house(Paint &p) const;
    // mood.cpp
    void draw_mood(Paint &p) const;
    // photos.cpp
    bool photos_wanted() const;
    bool disarm();
    void update_photos();
    void update_backdrop();
    void step_photo(int by);
    void start_photo_walk();
    void continue_photo_walk();
    void finish_photo_walk();
    bool photo_walk_failed_ = false; // a folder of this walk couldn't be read
    void photo_failed();
    void update_slideshow(const hui::InputFrame &input);
    // Setup's folder chooser.
    void browse_folder();
    void folder_listed(const Result &result, const std::string &folder);
    void activate_folder(int index);
    // What goes behind the pages.
    std::string picture_path(const std::string &source) const;
    std::string backdrop_url() const;
    void draw_photo(Paint &p) const;
    void draw_slideshow(Paint &p) const;
    void draw_page_backdrop(Paint &p) const;
    bool has_page_backdrop() const;
    bool has_clock_backdrop() const;
    // doorbell.cpp
    void check_doorbell();
    void update_camera(const hui::InputFrame &input);
    void close_camera();
    void refresh_camera();
    void watch_camera(const std::string &entity, const std::string &name);
    void draw_camera(Paint &p) const;
    // talk.cpp
    void update_talk(const hui::InputFrame &input, float dt);
    void start_talk();
    void finish_talk();
    void answer_talk(const Assist &assist);
    void draw_talk(Paint &p) const;
    // setup.cpp
    void rebuild_list();
    // Setup's rows follow the connection and the catalogue, keeping the focus on the same row.
    void refresh_lists();
    void activate_setup(int index);
    void activate_pick(int index);
    void open_pick(Pick pick);
    int pick_current_row() const;
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
    void draw_music(Paint &p) const;
    void draw_alert(Paint &p) const;
    std::vector<RoomSlot> room_slots() const;
    std::string room_title() const;
    std::string house_chip() const;
    // The room the room page shows (Square steps through them); an empty one without rooms.
    const RoomPage &current_room() const;
    void draw_setup(Paint &p) const;
    std::string home_status(Page page, std::string &icon, bool &warm) const;
    std::string home_chip(Page page) const;
    void draw_home_line(Paint &p) const;
    float deep_rest() const;
    bool after_dark() const;
    // After dark, between 22:00 and 06:00: the room is probably dark too.
    bool late_at_night() const;
    bool house_urgent() const;
    float clock_picture_brightness() const;
    void draw_chips(Paint &p, float rest) const;
    std::string timers_subtitle() const;
    // The forecast's rain unit ("mm" or "in"), as the weather entity gives it.
    std::string rain_unit() const;
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
    std::string remote_signature_; // the settings entity's last applied "lamplight" attribute
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
    // Pictures: the answer's, the album art, the doorbell camera and the resting clock's photo.
    // Each slot keeps one; one it replaces goes back to main to delete.
    struct Shown
    {
        std::string url;
        std::uint32_t texture = 0;
        int width = 0, height = 0;
        std::uint32_t soft = 0;  // the blurred copy, for behind a page
        float brightness = 0.5f; // overall, 0 to 1: the words over it need more shade the brighter
    };
    enum class Pic
    {
        answer,
        art,
        camera,
        photo,
        backdrop, // a fixed picture behind the pages (the room's, or the background setting's)
        count
    };
    struct PicSlot
    {
        std::string wanted;        // the last URL asked for
        bool waiting = false;      // asked for and not back yet
        bool keep = false;         // show the last picture until the new one arrives
        std::int64_t retry_at = 0; // it failed: the same URL is worth asking for again from then
        Shown shown;
    };
    std::array<PicSlot, static_cast<std::size_t>(Pic::count)> pics_;
    // Ask for a slot's picture ("" for none); the same URL again asks nothing.
    void want_picture(Pic which, const std::string &url, bool keep);
    // What a slot shows now, or nothing.
    const Shown *shown_picture(Pic which) const;
    void release(Shown &shown);
    // A photo filling the screen, as far through its time as t (0-1).
    void draw_framed(Paint &p, const Shown &photo, float alpha, float t) const;
    // The current photo over the one before it, fading in.
    void draw_fading_photo(Paint &p, float alpha) const;
    const PicSlot &slot(Pic which) const
    {
        return pics_[static_cast<std::size_t>(which)];
    }
    // A Home Assistant path ("/api/...") as a URL on it; anything else as given.
    std::string ha_url(const std::string &path) const;
    std::vector<std::uint32_t> released_;
    // Names and icons of calendars and appliances turned off in Setup, for turning them on again.
    std::map<std::string, Choice> set_aside_;
    // Wi-Fi
    QrCode qr_;
    // Room
    int room_index_ = 0, room_focus_ = 0, room_armed_ = -1;
    std::int64_t room_armed_until_ = 0, room_busy_until_ = 0;
    std::string room_busy_;
    // Cross is down on a tile with a sheet: a tap presses it, a hold opens the sheet.
    int room_hold_ = -1;
    float room_hold_s_ = 0;
    // A sheet: its name, the focused row, and each row of choices' place.
    enum class SheetKind
    {
        thing,
        lamps,   // every lamp in the room
        house,   // the whole-house actions and the modes
        speakers // where the music plays, and how loud in each room
    };
    struct Sheet
    {
        std::string entity, title, icon;
        SheetKind kind = SheetKind::thing;
        bool guarded = false; // its tile asks for a second press, so its Power row does
        std::string sensor;   // what its chart reads instead of the thing
        std::vector<NamedCall> boosts;
        int focus = 0;
        std::map<std::string, int> cursors;
        std::string armed; // the option waiting for its second press
        std::int64_t armed_until = 0;
    };
    std::optional<Sheet> sheet_;
    // Levels being set, by control: at most one call every 300 ms each, and the last always goes.
    struct Setting
    {
        Control control;
        double value = 0, sent = 0;
        std::int64_t sent_at = 0, touched_at = 0;
    };
    std::map<std::string, Setting> settings_;
    std::set<std::string> group_lamps_;
    // The lamps inside the group Undo restores, once Home Assistant has named them.
    std::set<std::string> house_lamps_;
    std::optional<ControlOption> waiting_action_; // its lamps are being noted
    History chart_;
    // Music on its way to another speaker: said once it plays there, and if it never does.
    struct SpeakerMove
    {
        std::string target, done;
        std::int64_t until = 0;
        // What the target was doing before: already playing something isn't the move arriving.
        std::string was_session, was_title;
        bool was_playing = false;
    };
    std::optional<SpeakerMove> speaker_move_;
    float frame_dt_ = 0;
    // Now Playing: the focused control (previous, play/pause, next), the volume being set, and
    // when it was last sent
    int music_focus_ = 1;
    double music_volume_ = -1, music_volume_sent_level_ = -1;
    std::string music_volume_player_;
    std::int64_t music_volume_sent_ = 0, music_busy_until_ = 0;
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
    // The house's notices: as Home Assistant lists them, the ids already seen (a new one pops up),
    // and the focused row
    std::vector<Notice> notices_;
    // Each notice by its key (else its id), with the level it was last announced at.
    std::map<std::string, int> notices_seen_;
    bool notices_known_ = false;
    int house_focus_ = 0;
    // The photo frame: the folder's pictures (shuffled, subfolders too), which is showing, and
    // when each was asked for
    std::vector<std::string> photo_list_, photo_found_;
    std::deque<std::string> photo_walk_; // folders still to read
    std::set<std::string> photo_walked_;
    std::size_t photo_next_ = 0;
    std::int64_t photo_listed_at_ = 0, photo_at_ = 0;
    bool photo_busy_ = false;
    // The photo before this one, fading out; how long this one has been up, and how far through
    // its time the one before was.
    Shown previous_photo_;
    float photo_age_s_ = 0, previous_photo_t_ = 0;
    // The slideshow page
    bool photo_paused_ = false, photo_clock_ = true;
    std::int64_t backdrop_asked_ = 0;
    std::int64_t photo_hints_until_ = 0;
    // A step pressed while the last was still being asked for goes once that's back; one pressed
    // by hand says so until its picture is up (a phone's photo can take a few seconds).
    int photo_queued_ = 0;
    bool photo_stepped_ = false;
    // Setup's folder chooser: the way down, and what the folder holds
    std::vector<std::string> folder_path_;
    std::vector<std::pair<std::string, std::string>> folder_items_;
    int folder_pictures_ = 0;
    bool folder_loading_ = false, folder_failed_ = false;
    // The doorbell: the trigger's last state, when it rang, until when the camera shows, and its
    // frames. A camera tile opened from a room shows the same way: its entity and name.
    std::string doorbell_seen_, watching_, watching_name_;
    std::int64_t doorbell_at_ = 0, camera_until_ = 0, camera_asked_ = 0;
    std::uint64_t camera_frame_ = 0;
    // Push-to-talk: the run, whether R2 is held with the microphone open, and since when
    std::uint64_t talk_run_ = 0;
    bool talking_ = false;
    float talk_held_s_ = 0, talk_peak_ = 0; // and the loudest the microphone heard
    std::int64_t talk_wait_until_ = 0;
    std::string talk_conversation_; // the conversation the next run continues
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

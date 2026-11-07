// Lamplight for PS5 - Host tests: the models, the app driven by the fixture, and the client.
// Copyright (C) 2026 Hayden Griffin
// SPDX-License-Identifier: GPL-3.0-or-later
#include "../host/fixture_home.hpp"
#include "../payload/console.hpp"
#include "lamplight/assistant.hpp"
#include "lamplight/controls.hpp"
#include "lamplight/paint.hpp"
#include "lamplight/resources.hpp"
#include "lamplight/room.hpp"
#include <filesystem>
#include <fstream>
#include "core/save_file.hpp"
#include "platform/ps5/system.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>

using namespace lamplight;
using hui::Action;
using hui::Direction;
int checks = 0;
#define CHECK(condition)                                                                           \
    do                                                                                             \
    {                                                                                              \
        ++checks;                                                                                  \
        if (!(condition))                                                                          \
        {                                                                                          \
            std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #condition);              \
            std::exit(1);                                                                          \
        }                                                                                          \
    } while (false)

namespace
{
// Europe/London in 2026: summer time from 29 March 01:00 UTC to 25 October 01:00 UTC.
std::int64_t london(std::int64_t utc_s)
{
    const std::int64_t starts = days_from_civil(2026, 3, 29) * 86400 + 3600;
    const std::int64_t ends = days_from_civil(2026, 10, 25) * 86400 + 3600;
    return utc_s >= starts && utc_s < ends ? 3600 : 0;
}
Fonts no_fonts()
{
    static hui::gfx::Font faces[5];
    static bool loaded = false;
    static Fonts fonts;
    if (!loaded)
    {
        const char *names[5] = {"text-regular", "text-semibold", "lexend-semibold",
                                "lexend-regular", "icons"};
        hui::ui::FontRef *refs[5] = {&fonts.text, &fonts.bold, &fonts.num, &fonts.num_light,
                                     &fonts.icons};
        for (int i = 0; i < 5; ++i)
        {
            std::string bytes;
            CHECK(
                hui::save::read_file(std::string("assets/fonts/") + names[i] + ".huifont", &bytes));
            CHECK(faces[i].load(bytes));
            *refs[i] = {&faces[i], 1};
        }
        fonts.kit.regular = fonts.text;
        fonts.kit.semibold = fonts.bold;
        fonts.kit.display = fonts.kit.mono = fonts.num;
        fonts.kit.pixel = fonts.kit.hand = fonts.text;
        loaded = true;
    }
    return fonts;
}
std::string sent_call(const Json &m)
{
    return text(m, "type") == "call_service" ? text(m, "domain") + "." + text(m, "service")
                                             : text(m, "type");
}
// The calls sent since the last look, without charts, which any sheet may ask for.
std::vector<Json> calls_sent(FixtureHome &h)
{
    std::vector<Json> calls;
    for (const auto &[ticket, message] : h.sent)
        if (text(message, "type") == "call_service")
            calls.push_back(message);
    return calls;
}
void test_clock()
{
    std::int64_t ms = 0;
    bool date_only = false;
    CHECK(parse_iso("2026-11-02T09:30:00+01:00", Zone{}, ms, date_only) && !date_only);
    CHECK(ms == (days_from_civil(2026, 11, 2) * 86400 + 8 * 3600 + 30 * 60) * 1000);
    CHECK(parse_iso("2026-11-02T09:30:00.250Z", Zone{3600}, ms, date_only));
    CHECK(ms % 1000 == 250);
    CHECK(parse_iso("2026-11-02 09:30:00-05:00", Zone{}, ms, date_only));
    CHECK(hhmm(ms, Zone{}) == "14:30");
    // A plain date is local midnight, not UTC midnight.
    CHECK(parse_iso("2026-07-01", Zone{3600}, ms, date_only) && date_only);
    CHECK(hhmm(ms, Zone{3600}) == "00:00");
    CHECK(parse_iso_ms("not a date", Zone{}) == kNoTime);
    CHECK(parse_iso_ms("2026-13-01", Zone{}) == kNoTime);
    CHECK(parse_iso_ms("2026-02-31", Zone{}) == kNoTime &&
          parse_iso_ms("2025-02-29", Zone{}) == kNoTime);
    CHECK(parse_iso_ms("2028-02-29", Zone{}) != kNoTime);
    CHECK(parse_iso_ms("2026-11-02T09:00:00+99:00", Zone{}) == kNoTime);
    const auto now = parse_iso_ms("2026-11-02T14:05:00+00:00", Zone{});
    CHECK(long_date(now, Zone{}) == "Monday 2 November");
    CHECK(short_date(now, Zone{}) == "Mon 2 Nov");
    CHECK(civil(0).weekday == 3);
    CHECK(parse_http_date("Mon, 02 Nov 2026 13:15:51 GMT") ==
          parse_iso_ms("2026-11-02T13:15:51Z", Zone{}));
    CHECK(parse_http_date("garbage") == kNoTime);
    CHECK(iso_local(day_start(now, Zone{}), Zone{}) == "2026-11-02T00:00:00+00:00");
    CHECK(iso_local(day_start(now, Zone{-5 * 3600}), Zone{-5 * 3600}) ==
          "2026-11-02T00:00:00-05:00");
    CHECK(day_start(parse_iso_ms("2026-11-02T23:30:00+00:00", Zone{}), Zone{3600}) ==
          parse_iso_ms("2026-11-03T00:00:00+01:00", Zone{}));
    // The day the clocks go back is 25 hours long, from 23:00 UTC; the day they go forward, 23.
    const Zone uk{0, &london};
    const auto back = day_start(parse_iso_ms("2026-10-25T12:00:00Z", uk), uk);
    CHECK(back == parse_iso_ms("2026-10-24T23:00:00Z", uk));
    CHECK(next_day(back, uk) - back == 25 * 3600000);
    const auto forward = day_start(parse_iso_ms("2026-03-29T12:00:00Z", uk), uk);
    CHECK(forward == parse_iso_ms("2026-03-29T00:00:00Z", uk) &&
          next_day(forward, uk) - forward == 23 * 3600000);
    // A time without a zone, and a plain date, are read in the zone at that instant.
    CHECK(hhmm(parse_iso_ms("2026-07-01T09:00:00", uk), uk) == "09:00");
    CHECK(hhmm(parse_iso_ms("2026-07-01", uk), uk) == "00:00");
    CHECK(iso_local(back, uk) == "2026-10-25T00:00:00+01:00");
}
void test_models(const Json &f)
{
    const Zone offset;
    std::vector<Hour> hours;
    std::vector<Day> days;
    CHECK(parse_hours(f["forecasts"]["hourly"], "weather.home", offset, hours));
    CHECK(hours.size() == 14);
    CHECK(parse_days(f["forecasts"]["daily"], "weather.home", offset, days));
    CHECK(days.size() == 6 && days[0].low < days[0].high);
    CHECK(!parse_hours(Json::object(), "weather.home", offset, hours));
    const auto now = parse_iso_ms(text(f, "now"), offset);
    CHECK(next_hours(hours, now).size() == 12);
    CHECK(next_hours(hours, now)[0].at == parse_iso_ms("2026-11-02T14:00:00+00:00", offset));
    CHECK(degrees(-0.4) == "0\xC2\xB0");
    CHECK(degrees(12.5) == "13\xC2\xB0");
    CHECK(one_decimal(2.55) == "2.6" || one_decimal(2.55) == "2.5");
    CHECK(one_decimal(0.04) == "0.0");
    CHECK(std::string(compass(225)) == "SW" && std::string(compass(-10)) == "N");
    CHECK(sky_for("pouring", true) == Sky::rain && sky_for("sunny", false) == Sky::night);
    CHECK(condition_label("partlycloudy") == "Partly cloudy" && condition_label("fog") == "Fog");

    std::vector<Event> events;
    CHECK(parse_events(f["events"]["calendar.family"], "calendar.family", offset, events));
    CHECK(parse_events(f["events"]["calendar.work"], "calendar.work", offset, events));
    CHECK(events.size() == 9);
    const auto today =
        day_events(events, day_start(now, offset), next_day(day_start(now, offset), offset));
    CHECK(today.all_day.size() == 1 && today.all_day[0].summary == "Bin day");
    CHECK(today.timed.size() == 5 && today.timed[0].summary == "School run");
    const auto swim = std::find_if(today.timed.begin(), today.timed.end(),
                                   [](const Event &e) { return e.cancelled; });
    CHECK(swim != today.timed.end() && swim->summary == "Swimming lesson");
    const auto tomorrow = day_events(events, next_day(day_start(now, offset), offset),
                                     next_day(next_day(day_start(now, offset), offset), offset));
    CHECK(tomorrow.all_day.size() == 1 && tomorrow.timed.size() == 2);
    CHECK(short_place("Microsoft Teams Meeting") == "Teams");
    CHECK(short_place("https://zoom.us/j/1") == "");
    CHECK(short_place("Smile Dental, 14 Station Road") == "Smile Dental");

    StateTable states;
    states.apply(f["states"]);
    const auto timers =
        view_assist_timers(states.get("sensor.viewassist_living_room"), now, offset);
    CHECK(timers.size() == 3);
    CHECK(timers[0].id == "pasta-1" && !timers[0].paused);
    CHECK(timers[0].left > 740 && timers[0].left < 742);
    CHECK(timers[1].kind == "alarm");
    CHECK(timers[2].paused && timers[2].left == 300);
    const auto apps = appliance_timers(Config::parse(f["config"]).appliances, states, offset);
    CHECK(apps.size() == 1 && apps[0].choice.name == "Washing machine");
    CHECK(left_text(3900) == "1 h 05 min");
    CHECK(left_text(1059.2) == "17 min 40 s");
    CHECK(left_text(45) == "45 s");
    CHECK(undo_time(12) == "30 seconds" && undo_time(741.4) == "12 minutes 21 seconds");
    CHECK(undo_time(3601) == "1 hour 1 second" && undo_time(120) == "2 minutes");

    CHECK(is_misfire("Sorry, I couldn't understand that", "Okay Nabu"));
    CHECK(is_misfire("No device named kitchen light found", ""));
    CHECK(is_misfire("Hello! I'm here to help. What can I do for you?", ""));
    CHECK(is_misfire("\"Okay Nabu.\"", "Okay Nabu"));
    CHECK(!is_misfire("Turned on the lights", "Okay Nabu"));
    CHECK(!is_misfire(std::string(150, 'a') + " couldn't understand", ""));
    CHECK(plain_reply("**Rain** later\n* umbrella\n- coat") ==
          "Rain later\n\xE2\x80\xA2 umbrella\n\xE2\x80\xA2 coat");

    CHECK(wifi_payload("Home; \"Guests\"", "pa:ss", "WPA") ==
          "WIFI:T:WPA;S:Home\\; \\\"Guests\\\";P:pa\\:ss;;");
    CHECK(wifi_payload("Cafe", "", "WPA") == "WIFI:T:nopass;S:Cafe;;");
    const auto qr = encode_qr(wifi_payload("Griffin guests", "correct horse battery", "WPA"));
    CHECK(qr.size >= 21 && qr.at(0, 0) && !qr.at(-1, 0));

    auto config = Config::parse(f["config"]);
    CHECK(config.configured() && config.calendars.size() == 2 &&
          config.calendars[0].name == "Family");
    CHECK(Config::parse(config.json()).json() == config.json());
    CHECK(Config::parse({{"calendars", {{"calendar.a", {{"name", "A"}}}}}}).calendars[0].name ==
          "A");
    CHECK(!Config::parse({{"host", "homeassistant.local"}, {"token", "x"}}).configured());
    CHECK(config.help().size() == default_categories().size());
    // lamplight.conf over the saved settings: its keys win, the rest is kept, and it says so.
    {
        const auto dir = std::filesystem::temp_directory_path() / "lamplight-tests";
        std::filesystem::create_directories(dir);
        std::ofstream(dir / "settings.json")
            << R"({"host":"192.0.2.20","token":"saved","weather":"weather.home"})";
        std::ofstream(dir / "lamplight.conf")
            << R"({"room_page":{"name":"Den","tiles":[{"entity":"light.den"}]}})";
        std::string error;
        bool bootstrapped = false;
        auto merged =
            load_config(dir.string(), (dir / "lamplight.conf").string(), error, bootstrapped);
        CHECK(bootstrapped && error.empty());
        CHECK(merged.token == "saved" && merged.weather == "weather.home");
        CHECK(merged.rooms.size() == 1 && merged.rooms[0].name == "Den" &&
              merged.rooms[0].tiles.size() == 1);
        std::filesystem::remove(dir / "lamplight.conf");
        merged = load_config(dir.string(), (dir / "lamplight.conf").string(), error, bootstrapped);
        CHECK(!bootstrapped && merged.rooms.empty());
        std::filesystem::remove_all(dir);
    }
    CHECK(fill_room("turn on the lights in the {room}", "") == "turn on the lights");
    CHECK(fill_room("play music in the {room}", "kitchen") == "play music in the kitchen");

    CHECK(allowed(service("weather", "get_forecasts", {}, "weather.home", true).message, config));
    CHECK(!allowed(service("lock", "unlock", {}, "lock.front_door").message, config));
    CHECK(!allowed(ask("", "", "").message, config));
    // The room page's entities, with the services a press calls on them, and nothing else.
    CHECK(allowed(press_action("light.living_room")->message, config));
    CHECK(allowed(press_action("script.good_night")->message, config));
    CHECK(!allowed(service("light", "toggle", {}, "light.bedroom").message, config));
    CHECK(!allowed(service("script", "turn_off", {}, "script.good_night").message, config));
    CHECK(!allowed(service("homeassistant", "toggle", {}, "light.living_room").message, config));
    CHECK(!press_action("climate.living_room") && !press_action("sensor.x"));
    CHECK(std::isnan(number(Json("unknown"))) && std::isnan(number(Json(""))));
    CHECK(number(Json(" 300")) == 300 && number(Json("12.5 kWh")) == 12.5);
}
void test_app(const Json &)
{
    // Loading: the forecast and every calendar are asked for once ready.
    FixtureHome home;
    Application app(home.config(), no_fonts());
    home.wire(app);
    home.reply(app);
    home.frame(app);
    int forecasts = 0, calendars = 0;
    for (const auto &[t, m] : home.sent)
    {
        forecasts += sent_call(m) == "weather.get_forecasts";
        if (sent_call(m) == "calendar.get_events")
        {
            ++calendars;
            CHECK(text(field(m, "service_data"), "start_date_time") == "2026-11-02T00:00:00+00:00");
            CHECK(text(field(m, "service_data"), "end_date_time") == "2026-11-04T00:00:00+00:00");
        }
    }
    CHECK(forecasts == 2 && calendars == 2);
    home.reply(app);
    home.frame(app);
    CHECK(home.sent.empty()); // nothing is fetched again while it is fresh

    // Navigation: the clock's chips, L1 and R1, Circle and Options.
    CHECK(app.page() == Page::home);
    home.press(app, Action::confirm);
    CHECK(app.page() == Page::room); // a room page comes first, then Now Playing and House
    home.press(app, Action::page_next);
    CHECK(app.page() == Page::music);
    home.press(app, Action::page_next);
    CHECK(app.page() == Page::house);
    home.press(app, Action::page_next);
    CHECK(app.page() == Page::weather);
    for (int i = 0; i < 4; ++i)
        home.press(app, Action::page_prev);
    CHECK(app.page() == Page::info);
    home.press(app, Action::back);
    CHECK(app.page() == Page::home);
    home.press(app, Action::menu);
    CHECK(app.page() == Page::setup);
    home.press(app, Action::back);
    home.press(app, Action::back);
    CHECK(app.page() == Page::quit);
    home.press(app, Action::back);

    // Timers: a running timer cancels at once, and Undo sets it again for the time it had left.
    app.open(Page::timers);
    home.move(app, Direction::left);
    home.press(app, Action::confirm);
    CHECK(home.sent.size() == 1 && sent_call(home.sent[0].second) == "view_assist.cancel_timer");
    CHECK(text(field(home.sent[0].second, "service_data"), "timer_id") == "pasta-1");
    CHECK(app.toast_text() == "Pasta cancelled");
    home.press(app, Action::north);
    CHECK(home.sent.size() == 2 && sent_call(home.sent[1].second) == "view_assist.set_timer");
    const auto &again = field(home.sent[1].second, "service_data");
    CHECK(text(again, "time") == "12 minutes 21 seconds" && text(again, "name") == "pasta");
    CHECK(app.toast_text() == "Pasta back on");
    home.reply(app);
    // An alarm needs a second press.
    home.move(app, Direction::down);
    home.press(app, Action::confirm);
    CHECK(home.sent.empty());
    home.press(app, Action::confirm);
    CHECK(home.sent.size() == 1 &&
          text(field(home.sent[0].second, "service_data"), "timer_id") == "alarm-3");
    CHECK(app.toast_text() == "Alarm cancelled");
    home.reply(app);
    // Quick start.
    home.move(app, Direction::right);
    home.press(app, Action::confirm);
    CHECK(home.sent.size() == 1 &&
          text(field(home.sent[0].second, "service_data"), "time") == "5 minutes");
    home.press(app, Action::confirm); // busy: not twice
    CHECK(home.sent.size() == 1);
    home.reply(app);

    // Help: confirm examples ask for a second press; the reply shows; visitors mode runs nothing.
    app.open(Page::help);
    home.move(app, Direction::right);
    for (int i = 0; i < 3; ++i)
        home.move(app, Direction::down);
    home.press(app, Action::confirm);
    CHECK(home.sent.empty());
    home.press(app, Action::confirm);
    CHECK(home.sent.size() == 1 && sent_call(home.sent[0].second) == "conversation/process");
    CHECK(text(home.sent[0].second, "text") == "turn everything off");
    home.reply(app);
    home.frame(app);
    CHECK(app.toast_text() == "Turned on the lights in the living room.");
    home.move(app, Direction::up);
    home.press(app, Action::confirm);
    CHECK(home.sent.size() == 1);
    CHECK(text(home.sent[0].second, "text") == "turn off the lights in the kitchen");
    CHECK(text(home.sent[0].second, "conversation_id") == "01JFIXTURE"); // a follow-up
    home.reply(app);
    home.snapshot.states.apply({{"c", {{"input_boolean.visitors", {{"+", {{"s", "on"}}}}}}}});
    home.reply(app);
    home.press(app, Action::confirm);
    CHECK(home.sent.empty());

    // A question typed while visitors mode came on runs nothing.
    {
        FixtureHome h;
        Application a(h.config(), no_fonts());
        h.wire(a);
        h.settle(a);
        a.open(Page::help);
        h.press(a, Action::north);
        CHECK(a.take_keyboard().has_value());
        h.snapshot.states.apply({{"c", {{"input_boolean.visitors", {{"+", {{"s", "on"}}}}}}}});
        h.reply(a);
        a.keyboard_result({true, "turn everything off"});
        CHECK(h.sent.empty());
    }
    // Appliance timers below View Assist's can be reached, and pressing one does nothing.
    {
        FixtureHome h;
        Application a(h.config(), no_fonts());
        h.wire(a);
        h.settle(a);
        a.open(Page::timers);
        h.move(a, Direction::left);
        for (int i = 0; i < 3; ++i)
            h.move(a, Direction::down);
        h.press(a, Action::confirm);
        CHECK(h.sent.empty());
    }
    // A picker row picks the entity it showed, even after the catalogue changes underneath it.
    {
        FixtureHome h;
        Application a(h.config(), no_fonts());
        h.wire(a);
        h.settle(a);
        a.open(Page::setup);
        for (int i = 0; i < 9; ++i)
            h.move(a, Direction::down);
        h.press(a, Action::confirm);
        h.move(a, Direction::down); // UK holidays
        h.snapshot.catalog.insert(h.snapshot.catalog.begin(),
                                  {"calendar.a_new_one", "A new one", false});
        h.reply(a);
        h.press(a, Action::confirm);
        CHECK(std::any_of(a.config().calendars.begin(), a.config().calendars.end(),
                          [](const Choice &c) { return c.entity == "calendar.uk_holidays"; }));
        // Off and on again keeps the name and icon it had.
        h.move(a, Direction::up); // Family, above UK holidays
        CHECK(std::any_of(a.config().calendars.begin(), a.config().calendars.end(),
                          [](const Choice &c) { return c.entity == "calendar.family"; }));
        h.press(a, Action::confirm);
        CHECK(std::none_of(a.config().calendars.begin(), a.config().calendars.end(),
                           [](const Choice &c) { return c.entity == "calendar.family"; }));
        h.press(a, Action::confirm);
        const auto family =
            std::find_if(a.config().calendars.begin(), a.config().calendars.end(),
                         [](const Choice &c) { return c.entity == "calendar.family"; });
        CHECK(family != a.config().calendars.end() && family->name == "Family" &&
              family->icon == "mdi:home-heart");
    }
    // The room page: a tile toggles its light; a confirm action asks for a second press.
    {
        FixtureHome h;
        Application a(h.config(), no_fonts());
        h.wire(a);
        h.settle(a);
        a.open(Page::room);
        h.press(a, Action::confirm); // Main lights first: not Pause, which a stray press would stop
        CHECK(h.sent.size() == 1 && sent_call(h.sent[0].second) == "light.toggle");
        CHECK(text(field(h.sent[0].second, "target"), "entity_id") == "light.living_room");
        h.reply(a);
        h.move(a, Direction::down); // the radiator tile: no press of its own, so its sheet
        h.press(a, Action::confirm);
        CHECK(a.sheet_showing());
        CHECK(h.sent.size() == 1 &&
              text(h.sent[0].second, "type") == "history/history_during_period");
        h.reply(a);
        h.press(a, Action::back);
        CHECK(!a.sheet_showing() && a.page() == Page::room);
        h.move(a, Direction::up);
        h.move(a, Direction::left); // Next
        h.move(a, Direction::left); // Play or Pause
        h.press(a, Action::confirm);
        CHECK(h.sent.size() == 1 && sent_call(h.sent[0].second) == "media_player.media_play_pause");
        h.reply(a);
        // Down from the hero's buttons reaches the actions; Good night is the fourth.
        h.move(a, Direction::down);
        h.move(a, Direction::down);
        h.move(a, Direction::right);
        h.press(a, Action::confirm);
        CHECK(h.sent.empty());
        h.press(a, Action::confirm);
        CHECK(h.sent.size() == 1 &&
              text(field(h.sent[0].second, "target"), "entity_id") == "script.good_night");
        h.reply(a);
        CHECK(a.watched().contains("media_player.living_room_speaker") &&
              a.watched().contains("script.movie_time"));
    }
    // A View Assist timer that runs out alerts once; Stop cancels it so View Assist stops ringing.
    {
        FixtureHome h;
        Application a(h.config(), no_fonts());
        h.wire(a);
        h.settle(a);
        CHECK(
            !a.alerting()); // the old "eggs" timer View Assist still lists as expired doesn't alert
        h.clock.utc_ms += 12 * 60000 + 30000; // past the pasta timer
        h.frame(a);
        CHECK(a.alerting());
        h.press(a, Action::confirm);
        CHECK(!a.alerting());
        // (Moving the clock on also refreshes the calendars; only the cancel matters here.)
        CHECK(std::count_if(h.sent.begin(), h.sent.end(),
                            [](const auto &s)
                            {
                                return sent_call(s.second) == "view_assist.cancel_timer" &&
                                       text(field(s.second, "service_data"), "timer_id") ==
                                           "pasta-1";
                            }) == 1);
        h.reply(a);
        h.frame(a);
        CHECK(!a.alerting()); // once only
        // View Assist drops a finished timer itself: then OK only closes the alert (cancelling it
        // would fail).
        auto late = Json::parse(R"({"c":{"sensor.viewassist_living_room":{"+":{"a":{"timers":[
            {"id":"late-5","status":"running","timer_class":"timer","expires":""}]}}}}})");
        late["c"]["sensor.viewassist_living_room"]["+"]["a"]["timers"][0]["expires"] =
            iso_local(h.clock.utc_ms + 20000, Zone{});
        h.snapshot.states.apply(late);
        h.reply(a);
        h.frame(a);
        h.clock.utc_ms += 30000;
        h.snapshot.states.apply(
            Json::parse(R"({"c":{"sensor.viewassist_living_room":{"+":{"a":{"timers":[]}}}}})"));
        h.reply(a);
        h.frame(a);
        CHECK(a.alerting());
        h.press(a, Action::confirm);
        CHECK(!a.alerting());
        CHECK(std::none_of(h.sent.begin(), h.sent.end(), [](const auto &s)
                           { return sent_call(s.second) == "view_assist.cancel_timer"; }));
        h.reply(a);
        // The washing machine, which finishes at 15:10, alerts when it gets there.
        h.clock.utc_ms = parse_iso_ms("2026-11-02T15:10:30Z", Zone{});
        h.frame(a);
        CHECK(a.alerting());
        h.press(a, Action::back);
        CHECK(!a.alerting());
        CHECK(std::none_of(h.sent.begin(), h.sent.end(), [](const auto &s)
                           { return sent_call(s.second) == "view_assist.cancel_timer"; }));
    }
    // A timer that leaves the list well before its end was cancelled: no alert.
    {
        FixtureHome h;
        Application a(h.config(), no_fonts());
        h.wire(a);
        h.settle(a);
        h.snapshot.states.apply(
            Json::parse(R"({"c":{"sensor.viewassist_living_room":{"+":{"a":{"timers":[]}}}}})"));
        h.reply(a);
        h.frame(a);
        CHECK(!a.alerting());
        h.clock.utc_ms += 20 * 60000;
        h.frame(a);
        CHECK(!a.alerting());
    }
    // Codex review 2: alerts wait their turn; Stop keeps an alert it couldn't send; busy room
    // controls refuse a second press; a sensor tile shows its value.
    {
        FixtureHome h;
        Application a(h.config(), no_fonts());
        h.wire(a);
        h.settle(a);
        h.clock.utc_ms =
            parse_iso_ms("2026-11-02T15:10:30Z", Zone{}); // pasta and the washing machine
        h.frame(a);
        CHECK(a.alerting());
        h.press(a, Action::back);
        CHECK(a.alerting()); // the second one, which finished while the first was up
        h.press(a, Action::back);
        CHECK(!a.alerting());
    }
    {
        FixtureHome h;
        Application a(h.config(), no_fonts());
        h.wire(a);
        h.settle(a);
        h.clock.utc_ms += 12 * 60000 + 30000;
        h.frame(a);
        CHECK(a.alerting());
        h.snapshot.ready = false; // Home Assistant gone: the cancel can't be sent
        h.press(a, Action::confirm);
        CHECK(a.alerting());
    }
    {
        FixtureHome h;
        Application a(h.config(), no_fonts());
        h.wire(a);
        h.settle(a);
        a.open(Page::room);
        h.move(a, Direction::left);
        h.move(a, Direction::left);
        h.press(a, Action::confirm);
        h.press(a, Action::confirm); // before Home Assistant answers
        CHECK(h.sent.size() == 1);
        h.reply(a);
        h.move(a, Direction::right); // Next: playing, so it works
        h.press(a, Action::confirm);
        CHECK(h.sent.size() == 1 && sent_call(h.sent[0].second) == "media_player.media_next_track");
        h.reply(a);
        Entity reading;
        reading.state = "21.5";
        reading.attributes = {{"unit_of_measurement", "\xC2\xB0"
                                                      "C"}};
        CHECK(state_words("sensor.living_room_temperature", reading) == "21.5 \xC2\xB0"
                                                                        "C");
        CHECK(!is_on("sensor.living_room_temperature", reading));
        reading.attributes = {{"unit_of_measurement", "%"}};
        CHECK(state_words("sensor.humidity", reading) == "21.5%");
        // A temperature or a percentage reads to a tenth at most; other units as they come.
        reading.state = "75.02";
        CHECK(state_words("sensor.humidity", reading) == "75%");
        reading.state = "16.53";
        reading.attributes = {{"unit_of_measurement", "\xC2\xB0"
                                                      "C"}};
        CHECK(state_words("sensor.kitchen_temperature", reading) == "16.5 \xC2\xB0"
                                                                    "C");
        reading.state = "0.053";
        reading.attributes = {{"unit_of_measurement", "kWh"}};
        CHECK(state_words("sensor.energy", reading) == "0.053 kWh");
    }
    // Untouched, a page goes back to the clock after five minutes; then the clock rests, and the
    // first press only wakes it.
    {
        FixtureHome h;
        Application a(h.config(), no_fonts());
        h.wire(a);
        h.settle(a);
        a.open(Page::weather);
        for (int i = 0; i < 301; ++i)
            h.frame(a, {}, 1.0f);
        CHECK(a.page() == Page::home);
        for (int i = 0; i < 130; ++i)
            h.frame(a, {}, 1.0f);
        h.press(a, Action::confirm);
        CHECK(a.page() == Page::home);
        h.press(a, Action::confirm);
        CHECK(a.page() == Page::room);
    }
    // A failed forecast is retried a minute later, not every frame, and Retry tries at once.
    FixtureHome failing;
    failing.fail_forecast = true;
    Application weather(failing.config(), no_fonts());
    failing.wire(weather);
    failing.settle(weather);
    failing.frame(weather);
    CHECK(failing.sent.empty());
    weather.open(Page::weather);
    failing.press(weather, Action::confirm);
    CHECK(failing.sent.size() == 2);
    failing.reply(weather);
    failing.clock.utc_ms += 61000;
    failing.frame(weather);
    CHECK(failing.sent.size() == 2);

    // Not connected: nothing is sent, and the page says so.
    FixtureHome offline;
    offline.snapshot.ready = false;
    Application quiet(offline.config(), no_fonts());
    offline.wire(quiet);
    offline.reply(quiet);
    offline.frame(quiet);
    quiet.open(Page::timers);
    offline.move(quiet, Direction::right);
    offline.press(quiet, Action::confirm);
    CHECK(offline.sent.empty());
    CHECK(quiet.toast_text() == "Not connected; nothing was set");

    // Setup: picking a calendar toggles it and changes what the client watches.
    FixtureHome setup;
    Application s(setup.config(), no_fonts());
    setup.wire(s);
    std::set<std::string> watched;
    s.watch = [&](const Config &, std::set<std::string> w) { watched = std::move(w); };
    bool saved = false;
    s.save = [&](const Config &) { saved = true; };
    setup.settle(s);
    s.open(Page::setup);
    for (int i = 0; i < 9; ++i)
        setup.move(s, Direction::down);
    setup.press(s, Action::confirm);
    CHECK(s.page() == Page::pick);
    setup.move(s, Direction::down); // UK holidays
    setup.press(s, Action::confirm);
    CHECK(saved && watched.contains("calendar.uk_holidays"));
    CHECK(s.config().calendars.size() == 3);
    setup.press(s, Action::back);
    CHECK(s.page() == Page::setup);
}
Entity entity_of(const char *state, const char *attributes)
{
    Entity e;
    e.state = state;
    e.attributes = Json::parse(attributes);
    return e;
}
// A thermostat's rows start with its chart; these tests are about what can be set.
std::vector<Control> settable(std::vector<Control> rows)
{
    std::erase_if(rows, [](const Control &row) { return row.kind == Control::Kind::chart; });
    return rows;
}
std::vector<std::string> ids_of(const std::vector<Control> &rows)
{
    std::vector<std::string> ids;
    for (const auto &r : rows)
        ids.push_back(r.id);
    return ids;
}
void test_controls()
{
    Config config;
    RoomPage room;
    auto tile = [&](const char *id) { room.tiles.push_back({id, "", "", false}); };
    for (const char *id :
         {"light.full", "light.plain", "climate.x", "cover.x", "fan.x", "media_player.x",
          "number.x", "select.x", "lock.x", "vacuum.x", "humidifier.x", "switch.x"})
        tile(id);
    config.rooms.push_back(room);
    // A light offers what its attributes say, and each row's call is allowed.
    const auto full = entity_of("on", R"({"brightness": 128,
        "supported_color_modes": ["color_temp", "hs"], "color_mode": "hs", "hs_color": [232, 90],
        "min_color_temp_kelvin": 2000, "max_color_temp_kelvin": 6500, "color_temp_kelvin": 2700,
        "effect_list": ["None", "Candle"], "effect": "Candle"})");
    auto rows = controls_for("light.full", full);
    CHECK((ids_of(rows) ==
           std::vector<std::string>{"power", "brightness", "warmth", "colour", "effect"}));
    const auto &brightness = rows[1];
    CHECK(brightness.value == 50 && brightness.min == 0 && brightness.max == 100);
    auto call = slider_request(brightness, 53);
    CHECK(sent_call(call->message) == "light.turn_on" &&
          field(call->message, "service_data")["brightness_pct"] == 55);
    CHECK(allowed(call->message, config));
    // The bottom of the slider is off; one service turns it off, not a brightness of nothing.
    call = slider_request(brightness, 0);
    CHECK(sent_call(call->message) == "light.turn_off" && allowed(call->message, config));
    CHECK(value_words(brightness, 0) == "Off" && value_words(brightness, 72) == "70%");
    call = slider_request(rows[2], 3333);
    CHECK(field(call->message, "service_data")["color_temp_kelvin"] == 3300);
    CHECK(value_words(rows[2], 2700) == "Warm white, 2700 K");
    // At rest a slider reads what the lamp reports, not the nearest step: 195 of 255 is 76%.
    CHECK(reading_words(brightness, 195 * 100.0 / 255) == "76%" &&
          value_words(brightness, 195 * 100.0 / 255) == "75%");
    CHECK(reading_words(brightness, 0.4) == "1%" && reading_words(brightness, 0) == "Off");
    CHECK(reading_words(rows[2], 2702) == "Warm white, 2700 K");
    CHECK(rows[3].kind == Control::Kind::swatches);
    CHECK(rows[3].options[static_cast<std::size_t>(rows[3].selected)].label == "Blue");
    call = option_request(rows[3], 1);
    CHECK(field(call->message, "service_data")["hs_color"] == Json::array({0, 100}));
    CHECK(rows[4].selected == 1 && allowed(option_request(rows[4], 0)->message, config));
    CHECK(!option_request(rows[4], 9) && !option_request(rows[4], -1));
    // White when it's a colour temperature now, or the colour has little saturation.
    CHECK(controls_for("light.full", entity_of("on", R"({"supported_color_modes": ["hs"],
        "hs_color": [10, 5]})"))[2]
              .selected == 0);
    // A light that only switches is only on or off, even with no attributes at all.
    CHECK((ids_of(controls_for("light.plain",
                               entity_of("off", R"({"supported_color_modes": ["onoff"]})"))) ==
           std::vector<std::string>{"power"}));
    CHECK((ids_of(controls_for("light.plain", entity_of("off", "{}"))) ==
           std::vector<std::string>{"power"}));
    CHECK(controls_for("light.plain", entity_of("on", R"({"brightness": 3})"))[1].id ==
          "brightness");
    // Off, a dimmable light reads as 0 and its range is unchanged.
    CHECK(controls_for("light.plain",
                       entity_of("off", R"({"supported_color_modes": ["brightness"]})"))[1]
              .value == 0);
    // Climate: its own range and step; off, the target falls back on the room's temperature.
    const auto heat = entity_of("off", R"({"current_temperature": 19.5, "temperature": null,
        "min_temp": 10, "max_temp": 30, "target_temp_step": 0.5,
        "hvac_modes": ["off", "heat"], "preset_modes": ["eco"], "preset_mode": "eco"})");
    // Its chart reads its own reading, with no line for a target while it is off.
    rows = controls_for("climate.x", heat);
    CHECK(rows[0].kind == Control::Kind::chart && rows[0].attribute == "current_temperature");
    CHECK(std::isnan(rows[0].target));
    rows = settable(rows);
    CHECK((ids_of(rows) == std::vector<std::string>{"temperature", "hvac", "preset"}));
    CHECK(rows[0].value == 19.5 && rows[1].selected == 0 && rows[2].selected == 0);
    call = slider_request(rows[0], 21.3);
    CHECK(sent_call(call->message) == "climate.set_temperature" &&
          field(call->message, "service_data")["temperature"] == 21.5);
    CHECK(allowed(call->message, config));
    CHECK(value_words(rows[0], 21.5) == "21.5\xC2\xB0" && value_words(rows[0], 21) == "21\xC2\xB0");
    CHECK(text(field(option_request(rows[1], 1)->message, "service_data"), "hvac_mode") == "heat");
    CHECK(allowed(option_request(rows[1], 1)->message, config));
    // A thermostat that only takes a range has two ends, sent together and never crossed; one that
    // takes neither has no target at all, and one that takes both follows its mode.
    rows = settable(controls_for("climate.x", entity_of("heat_cool", R"({"supported_features": 2,
        "current_temperature": 20, "target_temp_low": 18, "target_temp_high": 24,
        "min_temp": 7, "max_temp": 35, "target_temp_step": 0.5})")));
    CHECK((ids_of(rows) == std::vector<std::string>{"temp_low", "temp_high"}));
    call = slider_request(rows[0], 19);
    CHECK(sent_call(call->message) == "climate.set_temperature" && allowed(call->message, config));
    CHECK(field(call->message, "service_data")["target_temp_low"] == 19 &&
          field(call->message, "service_data")["target_temp_high"] == 24 &&
          !field(call->message, "service_data").contains("temperature"));
    call = slider_request(rows[0], 26);
    CHECK(field(call->message, "service_data")["target_temp_high"] == 26);
    call = slider_request(rows[1], 16);
    CHECK(field(call->message, "service_data")["target_temp_low"] == 16);
    CHECK(value_words(rows[1], 21.5) == "21.5\xC2\xB0");
    CHECK(ids_of(controls_for("climate.x", entity_of("fan_only", R"({"supported_features": 8})")))
              .empty());
    CHECK(controls_for("climate.x", entity_of("heat", R"({"supported_features": 3,
        "temperature": 21})"))[0]
              .id == "temperature");
    CHECK(controls_for("climate.x", entity_of("heat_cool", R"({"supported_features": 3,
        "target_temp_low": 18, "target_temp_high": 24})"))[0]
              .id == "temp_low");
    // A three-speed fan: each step is its own speed in Home Assistant's mapping, and the top and
    // the bottom are reached.
    {
        const auto fan = controls_for("fan.x", entity_of("on", R"({"percentage": 33,
            "percentage_step": 33.33})"))[1];
        CHECK(field(slider_request(fan, 33.33)->message, "service_data")["percentage"] == 33);
        CHECK(field(slider_request(fan, 66.66)->message, "service_data")["percentage"] == 66);
        CHECK(field(slider_request(fan, 100)->message, "service_data")["percentage"] == 100);
        CHECK(snapped(fan, 99) == 100);
        CHECK(sent_call(slider_request(fan, 0)->message) == "fan.turn_off" &&
              allowed(slider_request(fan, 0)->message, config));
    }
    // A position it reports but can't be set is no slider; one it can be set is.
    CHECK((ids_of(controls_for("cover.x", entity_of("open", R"({"current_position": 40,
        "current_tilt_position": 10, "supported_features": 3})"))) ==
           std::vector<std::string>{"cover"}));
    CHECK((ids_of(controls_for("cover.x", entity_of("open", R"({"current_position": 40,
        "current_tilt_position": 10, "supported_features": 135})"))) ==
           std::vector<std::string>{"position", "tilt", "cover"}));
    // Only the commands a thing supports: a garage door without Stop; a sensor-like cover, none.
    rows = controls_for("cover.x", entity_of("closed", R"({"supported_features": 3})"));
    CHECK(rows.size() == 1 && rows[0].options.size() == 2 && rows[0].options[1].label == "Close");
    CHECK(controls_for("cover.x", entity_of("closed", R"({"supported_features": 4})")).empty());
    // Covers, fans and players.
    rows = controls_for("cover.x", entity_of("open", R"({"current_position": 40,
        "current_tilt_position": 10})"));
    CHECK((ids_of(rows) == std::vector<std::string>{"position", "tilt", "cover"}));
    CHECK(sent_call(slider_request(rows[0], 80)->message) == "cover.set_cover_position");
    CHECK(sent_call(option_request(rows[2], 1)->message) == "cover.stop_cover");
    CHECK(allowed(option_request(rows[2], 1)->message, config));
    CHECK(state_words("cover.x", entity_of("open", R"({"current_position": 40})")) == "40% open");
    CHECK(state_words("cover.x", entity_of("open", R"({"current_position": 100})")) == "Open");
    rows = controls_for("fan.x", entity_of("on", R"({"percentage": 33, "percentage_step": 33.3,
        "preset_modes": ["sleep"], "oscillating": false})"));
    CHECK((ids_of(rows) == std::vector<std::string>{"power", "speed", "preset", "oscillate"}));
    CHECK(field(power_request(rows[3]).message, "service_data")["oscillating"] == true);
    CHECK(allowed(power_request(rows[3]).message, config));
    rows = controls_for("media_player.x", entity_of("playing", R"({"volume_level": 0.32,
        "is_volume_muted": false, "source_list": ["Radio"], "source": "Radio",
        "shuffle": false, "repeat": "off"})"));
    CHECK((ids_of(rows) ==
           std::vector<std::string>{"transport", "volume", "mute", "source", "shuffle", "repeat"}));
    CHECK(rows[0].options[1].label == "Pause" && rows[1].value == 32);
    CHECK(field(slider_request(rows[1], 50)->message, "service_data")["volume_level"] == 0.5);
    for (const auto &row : rows)
        if (row.kind == Control::Kind::power)
            CHECK(allowed(power_request(row).message, config));
        else if (row.kind != Control::Kind::slider)
            CHECK(allowed(option_request(row, 0)->message, config));
    // On-or-off settings are toggles: Cross asks for the opposite of what it is.
    CHECK(field(power_request(rows[2]).message, "service_data")["is_volume_muted"] == true);
    CHECK(field(power_request(rows[4]).message, "service_data")["shuffle"] == true);
    // Helpers.
    rows = controls_for("number.x", entity_of("4", R"({"min": 0, "max": 8, "step": 2})"));
    CHECK(rows[0].value == 4 && snapped(rows[0], 5.1) == 6);
    CHECK(sent_call(slider_request(rows[0], 6)->message) == "number.set_value");
    CHECK(allowed(slider_request(rows[0], 6)->message, config));
    rows = controls_for("select.x", entity_of("b", R"({"options": ["a", "b"]})"));
    CHECK(rows[0].selected == 1);
    CHECK(sent_call(option_request(rows[0], 0)->message) == "select.select_option");
    CHECK(allowed(option_request(rows[0], 0)->message, config));
    rows = controls_for("humidifier.x", entity_of("on", R"({"humidity": 45})"));
    CHECK(allowed(slider_request(rows[1], 50)->message, config));
    // A lock asks for a second press on Unlock (the sheet's), and only a room's lock is allowed.
    rows = controls_for("lock.x", entity_of("locked", "{}"));
    CHECK(rows[0].options[1].value == "unlock" && rows[0].options[1].asks);
    CHECK(!rows[0].options[0].asks);
    CHECK(allowed(option_request(rows[0], 1)->message, config));
    CHECK(!allowed(service("lock", "unlock", {}, "lock.other").message, config));
    CHECK(!allowed(service("lock", "open", {}, "lock.x").message, config));
    CHECK(
        allowed(option_request(controls_for("vacuum.x", entity_of("docked", "{}"))[0], 0)->message,
                config));
    // A thing that only reports has no sheet; a level comes first where there is one.
    CHECK(controls_for("sensor.x", entity_of("dry", "{}")).empty());
    CHECK(controls_for("binary_sensor.x", entity_of("on", "{}")).empty());
    CHECK(controls_for("script.x", entity_of("off", "{}")).empty());
    CHECK(!primary_slider("switch.x", entity_of("on", "{}")));
    CHECK(primary_slider("cover.x", entity_of("open", R"({"current_position": 1})"))->id ==
          "position");
    // A group's lamps, once it has more than one; never a loop of itself, and only lights.
    const auto group =
        entity_of("on", R"({"entity_id": ["light.a", "light.b", "light.me", "switch.c"]})");
    CHECK((group_members("light.me", group) == std::vector<std::string>{"light.a", "light.b"}));
    CHECK(group_members("light.me", entity_of("on", R"({"entity_id": ["light.a"]})")).empty());
    CHECK(group_members("switch.me", group).empty());
    const auto lamp = service("light", "turn_on", {{"brightness_pct", 40}}, "light.a").message;
    CHECK(!allowed(lamp, config));
    CHECK(allowed(lamp, config, {"light.a"}));
    CHECK(!allowed(service("switch", "toggle", {}, "switch.a").message, config, {"switch.a"}));
    CHECK(lamp_control("light.a", entity_of("on", R"({"brightness": 255})"), "A").kind ==
          Control::Kind::slider);
    CHECK(lamp_control("light.a", entity_of("off", R"({"supported_color_modes": ["onoff"]})"), "A")
              .kind == Control::Kind::power);
}
hui::InputFrame held_frame(Action action)
{
    hui::InputFrame in;
    in.held = hui::action_bit(action);
    return in;
}
void hold(FixtureHome &h, Application &a, Action action, float seconds)
{
    hui::InputFrame down;
    down.pressed = down.held = hui::action_bit(action);
    h.frame(a, down);
    h.frame(a, held_frame(action), seconds);
}
void test_sheets()
{
    FixtureHome h;
    // A thermostat of its own here, not one the house's plan runs (that's test_charts_and_boosts).
    auto config = h.config();
    for (auto &tile : config.rooms[0].tiles)
        tile.boosts.clear();
    Application a(config, no_fonts());
    h.wire(a);
    std::set<std::string> watched;
    a.watch = [&](const Config &, std::set<std::string> ids) { watched = std::move(ids); };
    h.settle(a);
    // The lamps inside a group come to be watched, once Home Assistant has named them.
    CHECK(watched.contains("light.living_room_wall") &&
          a.watched().contains("light.living_room_wall"));
    h.sent.clear();
    a.open(Page::room);
    // A tap toggles; it goes when Cross comes up.
    hui::InputFrame down;
    down.pressed = down.held = hui::action_bit(Action::confirm);
    h.frame(a, down);
    CHECK(h.sent.empty());
    h.frame(a);
    CHECK(h.sent.size() == 1 && sent_call(h.sent[0].second) == "light.toggle" &&
          !a.sheet_showing());
    h.reply(a);
    // A hold opens the sheet and doesn't toggle, on the way in or when it's let go.
    hold(h, a, Action::confirm, 0.3f);
    CHECK(!a.sheet_showing());
    h.frame(a, held_frame(Action::confirm), 0.3f);
    CHECK(a.sheet_showing() && h.sent.empty());
    h.frame(a);
    CHECK(a.sheet_showing() && h.sent.empty());
    hui::gfx::DrawList list;
    a.draw(list);
    // It opens on Brightness. Right: shown at once, one call, and steps from the level it's at.
    h.move(a, Direction::right);
    h.frame(a);
    CHECK(h.sent.size() == 1 && sent_call(h.sent[0].second) == "light.turn_on");
    CHECK(field(h.sent[0].second, "service_data")["brightness_pct"] == 60);
    // Another within 300 ms waits, and the last one always goes, even with the sheet closed.
    h.reply(a);
    h.move(a, Direction::right);
    h.frame(a);
    CHECK(h.sent.empty());
    h.press(a, Action::back);
    CHECK(!a.sheet_showing() && a.page() == Page::room);
    h.clock.utc_ms += 400;
    h.frame(a);
    CHECK(h.sent.size() == 1 && field(h.sent[0].second, "service_data")["brightness_pct"] == 65);
    h.reply(a);
    // Triangle opens the same sheet, and closes it.
    h.press(a, Action::north);
    CHECK(a.sheet_showing());
    a.draw(list);
    // Down through every row, the group's lamps last: Wall light only switches.
    for (int i = 0; i < 8; ++i)
        h.move(a, Direction::down);
    a.draw(list);
    h.press(a, Action::confirm);
    CHECK(h.sent.size() == 1 && sent_call(h.sent[0].second) == "light.toggle" &&
          text(field(h.sent[0].second, "target"), "entity_id") == "light.living_room_wall");
    h.reply(a);
    h.press(a, Action::north);
    CHECK(!a.sheet_showing());
    // The right stick sets the focused tile's level from the page.
    h.clock.utc_ms += 5000;
    h.frame(a);
    h.reply(a);
    h.sent.clear();
    hui::InputFrame stick;
    stick.stick2_y = -1;
    h.frame(a, stick, 0.2f);
    h.frame(a);
    CHECK(h.sent.size() == 1 && sent_call(h.sent[0].second) == "light.turn_on");
    CHECK(field(h.sent[0].second, "service_data")["brightness_pct"] > 55);
    a.draw(list);
    h.reply(a);
    h.clock.utc_ms += 5000;
    h.frame(a);
    h.sent.clear();
    // A hold cut short by the system menu presses nothing when the buttons come back.
    hold(h, a, Action::confirm, 0.1f);
    hui::InputFrame lost;
    lost.focus_lost = true;
    h.frame(a, lost);
    h.frame(a);
    h.frame(a);
    CHECK(h.sent.empty() && !a.sheet_showing());
    // Nor one cut short by an alert or a page change.
    hold(h, a, Action::confirm, 0.1f);
    h.press(a, Action::back);
    CHECK(a.page() == Page::home);
    h.frame(a);
    CHECK(h.sent.empty());
    // The radiator: Cross opens its sheet at once; moving along the modes sends nothing until
    // Cross chooses one.
    a.open(Page::room);
    h.move(a, Direction::down);
    h.press(a, Action::confirm);
    CHECK(a.sheet_showing() && calls_sent(h).empty());
    h.reply(a); // its chart
    h.move(a, Direction::right);
    h.frame(a);
    CHECK(h.sent.size() == 1 && sent_call(h.sent[0].second) == "climate.set_temperature");
    CHECK(field(h.sent[0].second, "service_data")["temperature"] == 21.5);
    h.reply(a);
    h.move(a, Direction::down);
    h.move(a, Direction::left);
    h.frame(a);
    CHECK(h.sent.empty());
    h.press(a, Action::confirm);
    CHECK(h.sent.size() == 1 && sent_call(h.sent[0].second) == "climate.set_hvac_mode" &&
          text(field(h.sent[0].second, "service_data"), "hvac_mode") == "off");
    h.reply(a);
    a.draw(list);
    h.press(a, Action::back);
    // Triangle from an action pill: every lamp in the room, with one slider for them all first.
    a.open(Page::room);
    h.move(a, Direction::left);
    h.move(a, Direction::down);
    h.press(a, Action::north);
    CHECK(a.sheet_showing());
    a.draw(list);
    h.clock.utc_ms += 5000;
    h.frame(a);
    h.reply(a);
    h.sent.clear();
    h.move(a, Direction::right);
    h.frame(a);
    // From their average, 52, to 55: Main lights is there already, so the other two are called.
    CHECK(h.sent.size() == 2);
    for (const auto &call : h.sent)
        CHECK(sent_call(call.second) == "light.turn_on" &&
              field(call.second, "service_data")["brightness_pct"] == 55);
    h.reply(a);
    h.clock.utc_ms += 5000;
    h.frame(a);
    h.sent.clear();
    // The stick on its way to another row doesn't move every lamp in the room.
    {
        hui::InputFrame diagonal;
        diagonal.stick_x = 0.5f;
        diagonal.stick_y = 0.9f;
        h.frame(a, diagonal, 0.2f);
        h.frame(a);
        CHECK(h.sent.empty());
    }
    h.press(a, Action::back);
    h.clock.utc_ms += 5000;
    h.frame(a);
    h.sent.clear();
    // Music: Triangle opens the player's sheet.
    a.open(Page::music);
    h.press(a, Action::north);
    CHECK(a.sheet_showing());
    a.draw(list);
    // A page change closes a sheet.
    a.open(Page::home);
    CHECK(!a.sheet_showing());
    // Unlock takes two presses; Lock one.
    {
        FixtureHome l;
        l.fixture["config"]["rooms"] =
            Json::array({{{"name", "Hall"}, {"tiles", Json::array({{{"entity", "lock.front"}}})}}});
        l.snapshot.states.apply(Json::parse(R"({"a": {"lock.front": {"s": "locked", "a": {}}}})"));
        Application b(l.config(), no_fonts());
        l.wire(b);
        l.settle(b);
        l.sent.clear();
        b.open(Page::room);
        // No press of its own, so Cross opens its buttons at once.
        l.press(b, Action::confirm);
        CHECK(b.sheet_showing() && l.sent.empty());
        l.move(b, Direction::right);
        l.press(b, Action::confirm);
        CHECK(l.sent.empty());
        b.draw(list);
        l.press(b, Action::confirm);
        CHECK(l.sent.size() == 1 && sent_call(l.sent[0].second) == "lock.unlock");
        // It goes away from under an open sheet: the sheet closes rather than offer dead controls.
        l.snapshot.states.apply(
            Json::parse(R"({"c": {"lock.front": {"+": {"s": "unavailable"}}}})"));
        l.reply(b);
        l.frame(b);
        CHECK(!b.sheet_showing());
    }
    // A tile that asks for a second press asks on its sheet too, and the right stick leaves it.
    {
        FixtureHome l;
        l.fixture["config"]["rooms"] = Json::array(
            {{{"name", "Hall"},
              {"tiles", Json::array({{{"entity", "light.living_room"}, {"confirm", true}}})}}});
        Application b(l.config(), no_fonts());
        l.wire(b);
        l.settle(b);
        l.sent.clear();
        b.open(Page::room);
        hui::InputFrame stick;
        stick.stick2_y = -1;
        l.frame(b, stick, 0.2f);
        l.frame(b);
        CHECK(l.sent.empty());
        l.press(b, Action::north);
        CHECK(b.sheet_showing());
        l.press(b, Action::confirm);
        CHECK(l.sent.empty());
        b.draw(list);
        l.press(b, Action::confirm);
        CHECK(l.sent.size() == 1 && sent_call(l.sent[0].second) == "light.toggle");
    }
    // Circle backs out of a second press and stays where it is, rather than leaving the page.
    {
        FixtureHome l;
        Application b(l.config(), no_fonts());
        l.wire(b);
        l.settle(b);
        l.sent.clear();
        b.open(Page::room);
        l.move(b, Direction::left);  // Next
        l.move(b, Direction::down);  // Gaming mode
        l.move(b, Direction::down);  // Lights off
        l.move(b, Direction::right); // Good night, which asks
        l.press(b, Action::confirm);
        l.press(b, Action::back);
        CHECK(b.page() == Page::room && l.sent.empty());
        l.press(b, Action::confirm);
        CHECK(l.sent.empty()); // armed again, not sent: the back really disarmed it
        l.press(b, Action::back);
        l.press(b, Action::back);
        CHECK(b.page() == Page::home && l.sent.empty());
    }
    {
        FixtureHome l;
        l.fixture["config"]["rooms"] =
            Json::array({{{"name", "Hall"}, {"tiles", Json::array({{{"entity", "lock.front"}}})}}});
        l.snapshot.states.apply(Json::parse(R"({"a": {"lock.front": {"s": "locked", "a": {}}}})"));
        Application b(l.config(), no_fonts());
        l.wire(b);
        l.settle(b);
        l.sent.clear();
        b.open(Page::room);
        l.press(b, Action::confirm);
        l.move(b, Direction::right); // Unlock
        l.press(b, Action::confirm);
        l.press(b, Action::back);
        CHECK(b.sheet_showing() && l.sent.empty());
        l.press(b, Action::back);
        CHECK(!b.sheet_showing() && l.sent.empty());
    }
    // Everything on the sheet of a tile that asks: a button takes two presses, and no slider skips
    // it. The room-wide sheet leaves its lamps alone.
    {
        FixtureHome l;
        l.fixture["config"]["rooms"] = Json::array(
            {{{"name", "Hall"},
              {"tiles",
               Json::array({{{"entity", "cover.living_room_blinds"}, {"confirm", true}}})}}});
        Application b(l.config(), no_fonts());
        l.wire(b);
        l.settle(b);
        l.sent.clear();
        b.open(Page::room);
        l.press(b, Action::north);
        CHECK(b.sheet_showing());
        l.move(b, Direction::right); // Stop, not a position
        l.frame(b);
        CHECK(l.sent.empty());
        l.press(b, Action::confirm);
        CHECK(l.sent.empty());
        b.draw(list);
        l.press(b, Action::confirm);
        CHECK(l.sent.size() == 1 && sent_call(l.sent[0].second) == "cover.stop_cover");
    }
    {
        FixtureHome l;
        l.fixture["config"]["rooms"] = Json::array(
            {{{"name", "Hall"},
              {"tiles", Json::array({{{"entity", "camera.front_door"}},
                                     {{"entity", "light.living_room"}, {"confirm", true}},
                                     {{"entity", "light.fireplace"}}})}}});
        Application b(l.config(), no_fonts());
        l.wire(b);
        l.settle(b);
        l.sent.clear();
        b.open(Page::room);
        l.press(b, Action::north);
        CHECK(b.sheet_showing());
        l.move(b, Direction::left);
        l.frame(b);
        CHECK(l.sent.size() == 1 && sent_call(l.sent[0].second) == "light.turn_on" &&
              text(field(l.sent[0].second, "target"), "entity_id") == "light.fireplace");
    }
    // Off beats a level still waiting to go: the lamp doesn't come back on.
    {
        FixtureHome l;
        l.fixture["config"]["rooms"] = Json::array(
            {{{"name", "Hall"}, {"tiles", Json::array({{{"entity", "light.fireplace"}}})}}});
        Application b(l.config(), no_fonts());
        l.wire(b);
        l.settle(b);
        l.sent.clear();
        b.open(Page::room);
        l.press(b, Action::north);
        l.move(b, Direction::left); // it opens on Brightness
        l.frame(b);
        CHECK(l.sent.size() == 1);
        l.reply(b);
        l.move(b, Direction::left); // waits its 300 ms
        l.move(b, Direction::up);
        l.press(b, Action::confirm); // Power
        CHECK(l.sent.size() == 1 && sent_call(l.sent[0].second) == "light.toggle");
        l.reply(b);
        l.clock.utc_ms += 400;
        l.frame(b);
        CHECK(l.sent.empty());
    }
    // A range's waiting end goes with the other as it stands now, not as it stood.
    {
        FixtureHome l;
        l.snapshot.states.apply(Json::parse(R"({"a": {"climate.range": {"s": "heat_cool", "a": {
            "supported_features": 2, "target_temp_low": 18, "target_temp_high": 24,
            "min_temp": 7, "max_temp": 35, "target_temp_step": 1}}}})"));
        l.fixture["config"]["rooms"] = Json::array(
            {{{"name", "Hall"}, {"tiles", Json::array({{{"entity", "climate.range"}}})}}});
        Application b(l.config(), no_fonts());
        l.wire(b);
        l.settle(b);
        l.sent.clear();
        b.open(Page::room);
        l.press(b, Action::confirm);
        CHECK(b.sheet_showing());
        l.move(b, Direction::right);
        l.frame(b);
        CHECK(l.sent.size() == 1 &&
              field(l.sent[0].second, "service_data")["target_temp_low"] == 19);
        l.reply(b);
        l.move(b, Direction::right); // 20, waiting
        l.move(b, Direction::down);
        l.move(b, Direction::left); // Cool to 23, at once
        l.frame(b);
        CHECK(l.sent.size() == 1 &&
              field(l.sent[0].second, "service_data")["target_temp_high"] == 23 &&
              field(l.sent[0].second, "service_data")["target_temp_low"] == 20);
        l.reply(b);
        l.clock.utc_ms += 400;
        l.frame(b);
        CHECK(l.sent.size() == 1 &&
              field(l.sent[0].second, "service_data")["target_temp_low"] == 20 &&
              field(l.sent[0].second, "service_data")["target_temp_high"] == 23);
    }
    // Rooms redefined from Home Assistant under a held press: what's there now isn't pressed.
    {
        FixtureHome l;
        l.fixture["config"]["rooms"] =
            Json::array({{{"name", "Hall"},
                          {"tiles", Json::array({{{"entity", "light.fireplace"}},
                                                 {{"entity", "light.living_room"}}})}}});
        Application b(l.config(), no_fonts());
        l.wire(b);
        l.settle(b);
        l.sent.clear();
        b.open(Page::room);
        hold(l, b, Action::confirm, 0.1f);
        l.snapshot.states.apply(Json::parse(R"({"a": {"sensor.lamplight_ps5": {"s": "ready",
            "a": {"lamplight": {"rooms": [{"name": "Hall", "tiles": [
                {"entity": "light.living_room"}, {"entity": "light.fireplace"}]}]}}}}})"));
        l.reply(b);
        l.frame(b);
        l.frame(b);
        CHECK(l.sent.empty());
    }
    // A camera tile fills the screen; it isn't an alert, and a ring takes over from it.
    {
        FixtureHome l;
        l.fixture["config"]["rooms"] = Json::array(
            {{{"name", "Hall"},
              {"tiles", Json::array({{{"entity", "camera.front_door"}, {"name", "Porch"}}})}}});
        l.snapshot.states.apply(Json::parse(
            R"({"a": {"camera.porch": {"s": "idle", "a": {"entity_picture": "/porch.jpg"}}}})"));
        l.fixture["config"]["rooms"][0]["tiles"][0]["entity"] = "camera.porch";
        Application b(l.config(), no_fonts());
        l.wire(b);
        std::vector<std::string> frames;
        b.request_picture = [&](const std::string &url) { frames.push_back(url); };
        l.settle(b);
        l.sent.clear();
        b.open(Page::room);
        l.press(b, Action::confirm);
        CHECK(b.camera_showing() && !b.alerting() && !b.sheet_showing() && l.sent.empty());
        l.frame(b);
        CHECK(frames.size() == 1 && frames[0].find("/porch.jpg") != std::string::npos);
        b.picture(frames[0], 31, 640, 360);
        b.draw(list);
        // The porch's next frame is still on its way when the bell rings: the door's camera is
        // asked for at once, and nothing of the porch stays under the banner.
        l.clock.utc_ms += 1000;
        l.frame(b);
        CHECK(frames.size() == 2);
        l.snapshot.states.apply(
            Json::parse(R"({"c": {"binary_sensor.front_door_ding": {"+": {"s": "on"}}}})"));
        l.reply(b);
        l.frame(b);
        CHECK(b.camera_showing() && b.alerting());
        CHECK(frames.size() == 3 && frames[2].find("camera.front_door") != std::string::npos);
        const auto gone = b.take_released();
        CHECK(std::find(gone.begin(), gone.end(), 31u) != gone.end());
        b.draw(list);
        l.press(b, Action::back);
        CHECK(!b.camera_showing() && b.page() == Page::room);
    }
}
std::vector<std::uint8_t> exif_jpeg(int orientation, bool little)
{
    std::vector<std::uint8_t> tiff;
    if (little)
        tiff = {'I',
                'I',
                42,
                0,
                8,
                0,
                0,
                0,
                1,
                0,
                0x12,
                0x01,
                3,
                0,
                1,
                0,
                0,
                0,
                static_cast<std::uint8_t>(orientation),
                0,
                0,
                0,
                0,
                0,
                0,
                0,
                0,
                0};
    else
        tiff = {'M',  'M',  0, 42, 0, 0, 0, 8, 0, 1,
                0x01, 0x12, 0, 3,  0, 0, 0, 1, 0, static_cast<std::uint8_t>(orientation),
                0,    0,    0, 0,  0, 0, 0, 0};
    std::vector<std::uint8_t> out{0xff, 0xd8, 0xff, 0xe0, 0, 4, 1, 2, 0xff, 0xe1};
    const auto length = tiff.size() + 8;
    out.push_back(static_cast<std::uint8_t>(length >> 8));
    out.push_back(static_cast<std::uint8_t>(length));
    for (char c : {'E', 'x', 'i', 'f', '\0', '\0'})
        out.push_back(static_cast<std::uint8_t>(c));
    out.insert(out.end(), tiff.begin(), tiff.end());
    out.insert(out.end(), {0xff, 0xda, 0, 2});
    return out;
}
void test_night_and_counts()
{
    // Partly cloudy after dark is a moon's cloud; the day's icons and other weather don't change.
    CHECK(std::string(condition_icon("partlycloudy", true)) == "weather-night-partly-cloudy");
    CHECK(std::string(condition_icon("sunny", true)) == "weather-night");
    CHECK(std::string(condition_icon("partlycloudy")) == "weather-partly-cloudy");
    CHECK(std::string(condition_icon("rainy", true)) == "weather-rainy");
    CHECK(has_icon("weather-night-partly-cloudy") && has_icon("weather-night"));
    // The room's count is exactly the tiles that can be lit.
    CHECK(can_glow("light.a") && can_glow("climate.a") && can_glow("cover.a") &&
          can_glow("vacuum.a") && can_glow("binary_sensor.a"));
    CHECK(!can_glow("sensor.a") && !can_glow("script.a") && !can_glow("lock.a") &&
          !can_glow("camera.a") && !can_glow("number.a"));
    // A bright picture behind a page gets more shade; a dark one keeps the page's usual amount.
    CHECK(page_shade(0.1f) == 0.62f && page_shade(0.3f) == 0.62f);
    CHECK(std::fabs(page_shade(1.0f) - 0.82f) < 1e-5f);
    // The slideshow is full strength until late at night, and then the brighter the dimmer.
    CHECK(slideshow_strength(false, 1.0f) == 1.0f);
    CHECK(slideshow_strength(true, 0.3f) == 0.45f);
    CHECK(std::fabs(slideshow_strength(true, 1.0f) - 0.3f) < 1e-5f);
    CHECK(slideshow_strength(true, 0.0f) == 0.45f);
    // The moon is quieter late, and quieter again in deep rest.
    CHECK(moon_strength(false, 0) == 0.6f && moon_strength(true, 0) == 0.3f);
    CHECK(std::fabs(moon_strength(true, 1) - 0.15f) < 1e-5f);
    // Every phase of the moon and the slideshow's night draw cleanly.
    hui::gfx::DrawList list;
    FixtureHome h;
    Application a(h.config(), no_fonts());
    h.wire(a);
    std::vector<std::string> pics;
    a.request_picture = [&](const std::string &url) { pics.push_back(url); };
    h.settle(a);
    h.clock.utc_ms = parse_iso_ms("2026-11-02T23:12:00Z", Zone{});
    h.snapshot.states.apply(Json::parse(
        R"({"c": {"sun.sun": {"+": {"s": "below_horizon"}}, "weather.home": {"+": {"s": "clear-night"}}}})"));
    h.reply(a);
    h.frame(a);
    while (a.alerting())
        h.press(a, Action::back);
    a.open(Page::photos);
    for (int i = 0; i < 4; ++i)
    {
        h.frame(a);
        h.reply(a);
    }
    CHECK(!pics.empty());
    a.picture(pics.back(), 51, 1600, 900, 52, 1.0f);
    a.draw(list);
    a.open(Page::home);
    for (int i = 0; i < 200; ++i)
        h.frame(a, {}, 1.0f);
    for (int day = 0; day < 30; ++day)
    {
        h.clock.utc_ms += 86400000;
        h.frame(a);
        list.clear();
        a.draw(list);
    }
}
void test_pictures()
{
    // EXIF orientation, either byte order; anything short, odd or absent is simply upright.
    CHECK(jpeg_orientation(exif_jpeg(6, true)) == 6);
    CHECK(jpeg_orientation(exif_jpeg(8, false)) == 8);
    CHECK(jpeg_orientation(exif_jpeg(9, true)) == 1);
    CHECK(jpeg_orientation({0xff, 0xd8, 0xff, 0xda, 0, 2}) == 1);
    CHECK(jpeg_orientation({0x89, 'P', 'N', 'G'}) == 1);
    CHECK(jpeg_orientation({}) == 1);
    const auto whole = exif_jpeg(6, true);
    for (std::size_t n = 0; n < whole.size(); ++n)
        CHECK(jpeg_orientation({whole.begin(), whole.begin() + static_cast<std::ptrdiff_t>(n)}) >=
              1);
    auto corrupt = whole;
    corrupt[20] = corrupt[21] = corrupt[22] = corrupt[23] = 0xff; // the directory's far away
    CHECK(jpeg_orientation(corrupt) == 1);
    auto many = whole;
    many[24] = 0xff; // more entries than the segment holds
    many[25] = 0xff;
    many[26] = 0;
    many[27] = 0;
    CHECK(jpeg_orientation(many) >= 1);
    // Turning: left and right of a pair, read upright.
    auto pixels = [](std::initializer_list<int> values)
    {
        std::vector<std::uint8_t> out;
        for (int v : values)
            out.insert(out.end(), {static_cast<std::uint8_t>(v), 0, 0, 255});
        return out;
    };
    auto turned = [&](int orientation)
    {
        auto p = pixels({1, 2, 3, 4, 5, 6});
        int w = 3, h = 2;
        apply_orientation(p, w, h, orientation);
        std::vector<int> reds{w, h};
        for (std::size_t i = 0; i < p.size(); i += 4)
            reds.push_back(p[i]);
        return reds;
    };
    CHECK((turned(1) == std::vector<int>{3, 2, 1, 2, 3, 4, 5, 6}));
    CHECK((turned(2) == std::vector<int>{3, 2, 3, 2, 1, 6, 5, 4}));
    CHECK((turned(3) == std::vector<int>{3, 2, 6, 5, 4, 3, 2, 1}));
    CHECK((turned(4) == std::vector<int>{3, 2, 4, 5, 6, 1, 2, 3}));
    CHECK((turned(5) == std::vector<int>{2, 3, 1, 4, 2, 5, 3, 6}));
    CHECK((turned(6) == std::vector<int>{2, 3, 4, 1, 5, 2, 6, 3}));
    CHECK((turned(7) == std::vector<int>{2, 3, 6, 3, 5, 2, 4, 1}));
    CHECK((turned(8) == std::vector<int>{2, 3, 3, 6, 2, 5, 1, 4}));
    // Shrinking averages, rather than picking every other pixel.
    auto small = shrunk(pixels({0, 100, 200, 100}), 2, 2, 1, 1);
    CHECK(small.size() == 4 && small[0] == 100 && small[3] == 255);
    CHECK(shrunk(pixels({9}), 1, 1, 1, 1)[0] == 9);
    // The soft copy is small and smooth; a long thin picture still has a size.
    std::vector<std::uint8_t> big(400 * 200 * 4, 255);
    for (int x = 0; x < 200; ++x)
        for (int y = 0; y < 200; ++y)
            big[static_cast<std::size_t>((y * 400 + x) * 4)] = 0;
    std::vector<std::uint8_t> soft;
    int sw = 0, sh = 0;
    soften(big, 400, 200, soft, sw, sh);
    CHECK(sw == 192 && sh == 96 && soft.size() == static_cast<std::size_t>(sw * sh * 4));
    CHECK(soft[0] < 5 && soft[static_cast<std::size_t>(191 * 4)] > 250);
    const auto middle = soft[static_cast<std::size_t>((48 * 192 + 96) * 4)];
    CHECK(middle > 90 && middle < 170);
    std::vector<std::uint8_t> thin(4000 * 4, 200);
    soften(thin, 4000, 1, soft, sw, sh);
    CHECK(sw == 192 && sh == 1);
    // Settings: what goes behind the pages, and how long each photo stays.
    CHECK(Config::parse(Json::object()).background == "sky");
    CHECK(Config::parse(Json::object()).photo_seconds == 60);
    CHECK(Config::parse({{"background", "photos"}}).background == "photos");
    CHECK(Config::parse({{"background", "room"}}).background == "room");
    CHECK(Config::parse({{"background", "/local/wall.jpg"}}).background == "/local/wall.jpg");
    CHECK(Config::parse({{"background", "sensor.viewassist_x"}}).background ==
          "sensor.viewassist_x");
    CHECK(Config::parse({{"background", "nonsense"}}).background == "sky");
    CHECK(Config::parse({{"background", "https://x/y.jpg"}}).background == "sky");
    CHECK(Config::parse({{"photo_seconds", 1}}).photo_seconds == 60);
    CHECK(Config::parse({{"photo_seconds", 30}}).photo_seconds == 30);
    Config saved;
    saved.background = "photos";
    saved.photo_seconds = 300;
    RoomPage withphoto;
    withphoto.tiles.push_back({"light.a", "", "", false});
    withphoto.photo = "/local/a.jpg";
    saved.rooms.push_back(withphoto);
    const auto back = Config::parse(saved.json());
    CHECK(back.background == "photos" && back.photo_seconds == 300 &&
          back.rooms[0].photo == "/local/a.jpg");
    // The folders in a listing, for the walk and Setup's chooser.
    const auto listing = Json::parse(R"({"children": [
        {"media_content_id": "a", "media_class": "directory", "title": "Holidays", "can_expand": true},
        {"media_content_id": "b", "media_class": "album", "title": "Album", "can_expand": true},
        {"media_content_id": "c", "media_class": "image", "media_content_type": "image/jpeg", "can_expand": false},
        {"media_content_id": "d", "media_class": "video", "can_expand": false}]})");
    CHECK(photo_ids(listing).size() == 1);
    const auto folders = photo_folders(listing);
    CHECK(folders.size() == 2 && folders[0].second == "Holidays");
}
int count_type(const FixtureHome &h, const char *type)
{
    return static_cast<int>(std::count_if(h.sent.begin(), h.sent.end(), [&](const auto &s)
                                          { return text(s.second, "type") == type; }));
}
void test_photos()
{
    hui::gfx::DrawList list;
    FixtureHome h;
    Application a(h.config(), no_fonts());
    h.wire(a);
    std::vector<std::string> asked;
    a.request_picture = [&](const std::string &url) { asked.push_back(url); };
    h.settle(a);
    h.sent.clear();
    // Triangle on the clock opens the slideshow; the folder is read, and the one inside it.
    h.press(a, Action::north);
    CHECK(a.page() == Page::photos);
    CHECK(count_type(h, "media_source/browse_media") == 1);
    a.draw(list);
    h.reply(a);
    h.frame(a);
    CHECK(count_type(h, "media_source/browse_media") == 1); // the 2025 folder
    h.reply(a);
    h.frame(a);
    CHECK(count_type(h, "media_source/resolve_media") == 1);
    h.reply(a);
    CHECK(asked.size() == 1 && asked[0].ends_with(kSoftPicture));
    a.picture(asked[0], 11, 1600, 900, 12);
    a.draw(list);
    // Next and previous step at once; a repeat doesn't race through them.
    h.move(a, Direction::right);
    CHECK(count_type(h, "media_source/resolve_media") == 1);
    h.reply(a);
    // The same URL from the fixture is the same picture: nothing new to fetch.
    CHECK(asked.size() == 1);
    h.move(a, Direction::left);
    CHECK(count_type(h, "media_source/resolve_media") == 1);
    h.reply(a);
    // A press while a photo is on its way isn't lost: it goes as soon as the answer is in.
    h.move(a, Direction::right);
    CHECK(count_type(h, "media_source/resolve_media") == 1);
    h.move(a, Direction::left);
    CHECK(count_type(h, "media_source/resolve_media") == 1);
    h.reply(a);
    CHECK(count_type(h, "media_source/resolve_media") == 0);
    h.frame(a);
    CHECK(count_type(h, "media_source/resolve_media") == 1);
    h.reply(a);
    h.frame(a);
    CHECK(count_type(h, "media_source/resolve_media") == 0);
    // A folder emptied since: nothing more to ask for, once it has been read. A failed read of it
    // says nothing, so the photos carry on.
    {
        FixtureHome e;
        Application c(e.config(), no_fonts());
        e.wire(c);
        std::vector<std::string> pictures;
        c.request_picture = [&](const std::string &url)
        {
            pictures.push_back(url);
            c.picture(url, static_cast<std::uint32_t>(100 + pictures.size()), 1600, 900);
        };
        e.settle(c);
        c.open(Page::photos);
        for (int i = 0; i < 6; ++i)
        {
            e.reply(c);
            e.frame(c);
        }
        CHECK(!pictures.empty());
        const auto listing = e.fixture["media"]["browse"];
        auto next_photo = [&](std::int64_t after)
        {
            e.clock.utc_ms += after;
            e.frame(c);
            while (c.alerting()) // the jump sets off the fixture's timers
                e.press(c, Action::back);
            int asked = 0;
            for (int i = 0; i < 6; ++i)
            {
                e.frame(c);
                asked += count_type(e, "media_source/resolve_media");
                e.reply(c);
            }
            return asked;
        };
        e.fail_browse = true;
        CHECK(next_photo(31 * 60000) >= 1);
        e.fail_browse = false;
        e.fixture["media"]["browse"]["children"] = Json::array();
        next_photo(31 * 60000);
        CHECK(next_photo(61000) == 0);
        CHECK(next_photo(61000) == 0);
        e.fixture["media"]["browse"] = listing;
        next_photo(6 * 60000);
        CHECK(next_photo(61000) >= 1);
    }
    // A new picture fades in over the old one, and the old one's textures go back afterwards.
    h.fixture["media"]["resolve"]["url"] = "/media/local/photos/fells.png?authSig=fixture";
    h.clock.utc_ms += 61000;
    h.frame(a);
    h.reply(a);
    CHECK(asked.size() == 2 && asked[1].find("fells") != std::string::npos);
    CHECK(a.take_released().empty());
    a.picture(asked[1], 21, 900, 1600, 22); // portrait: shown whole, over its blur
    CHECK(a.take_released().empty());
    a.draw(list);
    h.frame(a, {}, 2.0f);
    const auto gone = a.take_released();
    CHECK(gone.size() == 2 && std::count(gone.begin(), gone.end(), 11u) == 1 &&
          std::count(gone.begin(), gone.end(), 12u) == 1);
    // Cross pauses: its time passing asks for nothing; Cross again plays.
    h.press(a, Action::confirm);
    a.draw(list);
    h.sent.clear();
    h.clock.utc_ms += 120000;
    h.frame(a);
    h.reply(a);
    h.sent.clear();
    h.frame(a);
    CHECK(count_type(h, "media_source/resolve_media") == 0);
    h.press(a, Action::confirm);
    CHECK(count_type(h, "media_source/resolve_media") == 0);
    // The bumpers change how long each stays, and it's saved.
    bool saved = false;
    a.save = [&](const Config &) { saved = true; };
    h.press(a, Action::page_next);
    CHECK(a.config().photo_seconds == 120 && saved);
    h.press(a, Action::page_prev);
    h.press(a, Action::page_prev);
    CHECK(a.config().photo_seconds == 30);
    for (int i = 0; i < 9; ++i)
        h.press(a, Action::page_prev);
    CHECK(a.config().photo_seconds == 10);
    // Square hides the clock; the slideshow outlasts the five minutes every other page gets.
    h.press(a, Action::west);
    a.draw(list);
    for (int i = 0; i < 7; ++i)
        h.frame(a, {}, 60.0f);
    CHECK(a.page() == Page::photos);
    h.press(a, Action::back);
    CHECK(a.page() == Page::home);
    // Off the slideshow with nothing wanting photos, the picture is let go.
    h.frame(a);
    CHECK(a.take_released().size() >= 2);

    // Photos behind every page: the rotation runs, the clock and a page show it.
    {
        FixtureHome b;
        b.fixture["config"]["background"] = "photos";
        Application c(b.config(), no_fonts());
        b.wire(c);
        std::vector<std::string> pics;
        c.request_picture = [&](const std::string &url) { pics.push_back(url); };
        b.settle(c);
        for (int i = 0; i < 3; ++i)
        {
            b.frame(c);
            b.reply(c);
        }
        const auto wanted = std::find_if(pics.begin(), pics.end(), [](const auto &u)
                                         { return u.find("harbour") != std::string::npos; });
        CHECK(wanted != pics.end());
        c.picture(*wanted, 31, 1600, 900, 32);
        c.draw(list);
        c.open(Page::room);
        c.draw(list);
        c.open(Page::weather);
        c.draw(list);
        c.open(Page::setup);
        c.draw(list);
    }
    // A room's own picture on its page; another page has none, and it is let go.
    {
        FixtureHome b;
        b.fixture["config"]["rooms"][0]["photo"] = "sensor.viewassist_living_room";
        Application c(b.config(), no_fonts());
        b.wire(c);
        std::vector<std::string> pics;
        c.request_picture = [&](const std::string &url) { pics.push_back(url); };
        b.settle(c);
        CHECK(pics.empty());
        c.open(Page::room);
        b.frame(c);
        CHECK(pics.size() == 1 && pics[0] == "http://127.0.0.1:8123/local/rooms/living-room.jpg" +
                                                 std::string(kSoftPicture));
        c.picture(pics[0], 41, 1600, 900, 42);
        c.draw(list);
        c.open(Page::agenda);
        b.frame(c);
        c.draw(list);
        const auto gone2 = c.take_released();
        CHECK(std::count(gone2.begin(), gone2.end(), 41u) == 1 &&
              std::count(gone2.begin(), gone2.end(), 42u) == 1);
    }
    // How bright a picture is, for how much shade the words over it need.
    CHECK(mean_brightness({255, 255, 255, 255}) > 0.99f);
    CHECK(mean_brightness({0, 0, 0, 255}) < 0.01f);
    CHECK(mean_brightness({}) == 0.5f);
    // A background picture that fails is asked for again after a minute, and none before there's
    // a connection; a pause lets go after a quarter of an hour.
    {
        FixtureHome b;
        b.fixture["config"]["background"] = "/local/wall.jpg";
        Application c(b.config(), no_fonts());
        b.wire(c);
        std::vector<std::string> pics;
        c.request_picture = [&](const std::string &url) { pics.push_back(url); };
        c.update(hui::InputFrame{}, 1.0f / 60, b.clock, *std::make_unique<hui::ui::Feedback>());
        CHECK(pics.empty());
        b.settle(c);
        CHECK(pics.size() == 1 && pics[0].find("/local/wall.jpg") != std::string::npos);
        c.picture(pics[0], 0, 0, 0);
        b.clock.utc_ms += 30000;
        b.frame(c);
        CHECK(pics.size() == 1);
        b.clock.utc_ms += 32000;
        b.frame(c);
        CHECK(pics.size() == 2);
        c.picture(pics[1], 41, 1920, 1080, 42);
        b.clock.utc_ms += 90000;
        b.frame(c);
        CHECK(pics.size() == 2);
    }
    {
        FixtureHome b;
        Application c(b.config(), no_fonts());
        b.wire(c);
        std::vector<std::string> pics;
        c.request_picture = [&](const std::string &url) { pics.push_back(url); };
        b.settle(c);
        c.open(Page::photos);
        for (int i = 0; i < 4; ++i)
        {
            b.frame(c);
            b.reply(c);
        }
        CHECK(pics.size() == 1);
        c.picture(pics[0], 51, 1600, 900, 52);
        b.press(c, Action::confirm); // paused
        b.sent.clear();
        b.clock.utc_ms += 120000;
        b.frame(c);
        CHECK(count_type(b, "media_source/resolve_media") == 0);
        b.clock.utc_ms += 1000;
        b.frame(c, {}, 16 * 60.0f);
        b.frame(c);
        CHECK(count_type(b, "media_source/resolve_media") == 1);
    }
    // The connection goes during the first look: it looks again on reconnecting, without waiting
    // the five minutes an empty folder would.
    {
        FixtureHome b;
        Application c(b.config(), no_fonts());
        b.wire(c);
        b.settle(c);
        c.open(Page::photos);
        b.frame(c);
        CHECK(count_type(b, "media_source/browse_media") == 1);
        b.sent.clear();
        ++b.snapshot.generation;
        b.reply(c);
        b.frame(c);
        CHECK(count_type(b, "media_source/browse_media") == 1);
    }
    // A picture that can't be read doesn't stop the frame: the next is tried soon after.
    {
        FixtureHome b;
        Application c(b.config(), no_fonts());
        b.wire(c);
        std::vector<std::string> pics;
        c.request_picture = [&](const std::string &url) { pics.push_back(url); };
        b.settle(c);
        c.open(Page::photos);
        for (int i = 0; i < 4; ++i)
        {
            b.frame(c);
            b.reply(c);
        }
        CHECK(pics.size() == 1);
        c.picture(pics[0], 0, 0, 0);
        c.draw(list);
        b.sent.clear();
        b.clock.utc_ms += 1000;
        b.frame(c);
        CHECK(count_type(b, "media_source/resolve_media") == 0);
        b.clock.utc_ms += 3000;
        b.frame(c);
        CHECK(count_type(b, "media_source/resolve_media") == 1);
        // The same address again is fetched again: one picture in the folder isn't left blank.
        b.reply(c);
        CHECK(pics.size() == 2 && pics[1] == pics[0]);
    }
    // A long interval: back to the slideshow is a picture at once, and one slower than the
    // interval isn't passed over; its time starts when it shows.
    {
        FixtureHome b;
        b.fixture["config"]["photo_seconds"] = 300;
        Application c(b.config(), no_fonts());
        b.wire(c);
        std::vector<std::string> pics;
        c.request_picture = [&](const std::string &url) { pics.push_back(url); };
        b.settle(c);
        c.open(Page::photos);
        for (int i = 0; i < 4; ++i)
        {
            b.frame(c);
            b.reply(c);
        }
        CHECK(pics.size() == 1);
        c.picture(pics[0], 61, 1600, 900, 62);
        b.press(c, Action::back);
        CHECK(c.page() == Page::home);
        CHECK(c.take_released().size() >= 2);
        b.sent.clear();
        c.open(Page::photos);
        b.frame(c);
        CHECK(count_type(b, "media_source/resolve_media") == 1);
        b.reply(c);
        CHECK(pics.size() == 2);
        b.sent.clear();
        b.clock.utc_ms += 400000;
        b.frame(c);
        CHECK(count_type(b, "media_source/resolve_media") == 0);
        c.picture(pics[1], 63, 1600, 900, 64);
        b.clock.utc_ms += 299000;
        b.frame(c);
        CHECK(count_type(b, "media_source/resolve_media") == 0);
        b.clock.utc_ms += 2000;
        b.frame(c);
        CHECK(count_type(b, "media_source/resolve_media") == 1);
    }
    // A pause belongs to the slideshow: the resting clock's photos go on changing.
    {
        FixtureHome b;
        b.fixture["config"]["background"] = "photos";
        Application c(b.config(), no_fonts());
        b.wire(c);
        std::vector<std::string> pics;
        c.request_picture = [&](const std::string &url) { pics.push_back(url); };
        b.settle(c);
        c.open(Page::photos);
        for (int i = 0; i < 4; ++i)
        {
            b.frame(c);
            b.reply(c);
        }
        c.picture(pics[0], 71, 1600, 900, 72);
        b.press(c, Action::confirm);
        b.press(c, Action::back);
        CHECK(c.page() == Page::home);
        b.sent.clear();
        b.clock.utc_ms += 120000;
        b.frame(c);
        CHECK(count_type(b, "media_source/resolve_media") == 1);
    }
    // Setup's chooser: down through a folder, up with Circle, and choosing one.
    {
        FixtureHome b;
        b.fixture["config"]["photos"] = "";
        Application c(b.config(), no_fonts());
        b.wire(c);
        bool saved_folder = false;
        c.save = [&](const Config &) { saved_folder = true; };
        b.settle(c);
        // Triangle offers nothing with no folder chosen.
        b.press(c, Action::north);
        CHECK(c.page() == Page::home);
        // Photos as the background needs a folder first.
        c.open(Page::setup);
        for (int i = 0; i < 5; ++i)
            b.move(c, Direction::down);
        b.sent.clear();
        b.press(c, Action::confirm);
        // No folder, but the View Assist device has a background: the room's photo.
        CHECK(c.config().background == "room");
        b.press(c, Action::confirm);
        CHECK(c.config().background == "sky");
        b.move(c, Direction::down);
        b.press(c, Action::confirm);
        CHECK(c.page() == Page::pick);
        CHECK(count_type(b, "media_source/browse_media") == 1);
        CHECK(!field(b.sent.back().second, "media_content_id").is_string());
        c.draw(list);
        b.reply(c);
        c.draw(list);
        b.move(c, Direction::down); // the 2025 folder
        b.press(c, Action::confirm);
        CHECK(text(b.sent.back().second, "media_content_id").ends_with("/2025"));
        // Circle before 2025 answers: its late reply doesn't stand in for the folder now open, so
        // the first row still waits for that folder's own.
        b.press(c, Action::back);
        CHECK(b.sent.size() >= 2);
        {
            const auto top = b.sent.back();
            b.sent.pop_back();
            b.reply(c);
            b.press(c, Action::confirm);
            CHECK(c.page() == Page::pick);
            b.sent.push_back(top);
            b.reply(c);
        }
        b.move(c, Direction::down);
        b.press(c, Action::confirm);
        CHECK(text(b.sent.back().second, "media_content_id").ends_with("/2025"));
        b.reply(c);
        // Circle goes up a level before it goes back to Setup.
        b.press(c, Action::back);
        CHECK(c.page() == Page::pick);
        b.reply(c);
        b.move(c, Direction::down);
        b.press(c, Action::confirm);
        b.reply(c);
        b.press(c, Action::confirm); // Use this folder
        CHECK(c.page() == Page::setup && saved_folder);
        CHECK(c.config().photos.ends_with("/2025"));
        // Now the folder's there, Background offers Photos; No photos at the top undoes it.
        b.move(c, Direction::up);
        c.open(Page::setup);
        for (int i = 0; i < 5; ++i)
            b.move(c, Direction::down);
        b.press(c, Action::confirm);
        CHECK(c.config().background == "photos");
        b.move(c, Direction::down);
        b.press(c, Action::confirm);
        b.reply(c);
        // With a folder in use the chooser opens on the first folder: a stray press can't clear it.
        const auto in_use = c.config().photos;
        b.press(c, Action::confirm);
        CHECK(c.page() == Page::pick && c.config().photos == in_use);
        b.reply(c);
        b.press(c, Action::back);
        b.reply(c);
        b.move(c, Direction::up); // No photos
        b.press(c, Action::confirm);
        CHECK(c.config().photos.empty() && c.config().background == "sky");
        // Folder names, from either form of id.
        CHECK(photo_folder_name("media-source://media_source/local/photos/2025") == "2025");
        CHECK(photo_folder_name("media-source://immich/abc|albums|Holidays") == "Holidays");
        // Photo every steps round its choices.
        b.move(c, Direction::down);
        const auto before = c.config().photo_seconds;
        b.press(c, Action::confirm);
        CHECK(c.config().photo_seconds != before);
        // A listing that fails leaves the folder in use out of a stray press's reach.
        {
            FixtureHome f;
            Application g(f.config(), no_fonts());
            f.wire(g);
            f.settle(g);
            const auto folder = g.config().photos;
            CHECK(!folder.empty());
            g.open(Page::setup);
            for (int i = 0; i < 6; ++i)
                f.move(g, Direction::down);
            f.press(g, Action::confirm);
            CHECK(g.page() == Page::pick);
            f.snapshot.ready = false; // the connection goes before the reply comes
            f.sent.clear();
            f.reply(g);
            f.press(g, Action::confirm);
            CHECK(g.config().photos == folder);
        }
        // Offline, Triangle on Help says so at once rather than after the typing.
        {
            FixtureHome f;
            Application g(f.config(), no_fonts());
            f.wire(g);
            f.settle(g);
            g.open(Page::help);
            f.snapshot.ready = false;
            f.reply(g);
            f.press(g, Action::north);
            CHECK(!g.take_keyboard().has_value());
            CHECK(g.toast_text() == "Not connected to Home Assistant");
        }
        // A single-choice list opens on what is chosen now, so Cross straight away changes nothing.
        const auto device = c.config().view_assist;
        CHECK(!device.empty());
        c.open(Page::setup);
        for (int i = 0; i < 10; ++i)
            b.move(c, Direction::down);
        b.press(c, Action::confirm);
        CHECK(c.page() == Page::pick);
        b.press(c, Action::confirm);
        CHECK(c.page() == Page::setup && c.config().view_assist == device);
    }
}
int integration(int port, bool rejected)
{
    Config c;
    c.host = "127.0.0.1";
    c.port = port;
    c.token = "fixture-token";
    Client client;
    CHECK(client.start(c, {"weather.home", "sensor.viewassist_living_room"}));
    const auto until = hui::sys::monotonic_us() + 15000000;
    Snapshot s;
    auto wait = [&](auto done)
    {
        do
        {
            s = client.snapshot();
            if (done())
                return true;
            hui::sys::sleep_us(20000);
        } while (hui::sys::monotonic_us() < until);
        return false;
    };
    if (rejected)
    {
        CHECK(wait([&] { return s.status.find("rejected") != std::string::npos; }));
        hui::sys::sleep_us(1200000);
        CHECK(client.snapshot().generation == s.generation);
        client.stop();
        return 0;
    }
    CHECK(wait([&] { return s.ready; }));
    CHECK(s.states.get("weather.home").state == "partlycloudy");
    CHECK(s.states.has("sun.sun"));
    CHECK(s.clock_known && s.clock_skew_ms > 3600000 - 2500 && s.clock_skew_ms < 3600000 + 2500);
    CHECK(!s.catalog.empty());
    CHECK(std::any_of(s.catalog.begin(), s.catalog.end(),
                      [](const CatalogEntry &e) { return e.has_timers; }));
    CHECK(std::none_of(s.catalog.begin(), s.catalog.end(),
                       [](const CatalogEntry &e) { return e.id == "light.living_room"; }));
    auto ticket = client.send(
        service("weather", "get_forecasts", {{"type", "hourly"}}, "weather.home", true));
    CHECK(ticket);
    const Result *found = nullptr;
    CHECK(wait(
        [&]
        {
            for (const auto &r : s.results)
                if (r.ticket == ticket)
                    return (found = &r) != nullptr;
            return false;
        }));
    std::vector<Hour> hours;
    CHECK(found->ok &&
          parse_hours(field(found->response, "response"), "weather.home", Zone{}, hours) &&
          hours.size() == 14);
    ticket = client.send(ask("what time is it", "conversation.home_assistant", ""));
    CHECK(wait(
        [&]
        {
            for (const auto &r : s.results)
                if (r.ticket == ticket && r.ok)
                    return text(field(field(field(r.response, "response"), "speech"), "plain"),
                                "speech") == "Turned on the lights in the living room.";
            return false;
        }));
    CHECK(!client.send(service("lock", "unlock", {}, "lock.front_door")));
    // A chart: what comes back is small by the time it reaches a frame, and only a watched
    // entity's.
    CHECK(!client.send(history_request("sensor.not_watched", "", 0, Zone{})));
    ticket = client.send(history_request("sensor.viewassist_living_room", "", 0, Zone{}));
    CHECK(ticket);
    CHECK(wait(
        [&]
        {
            for (const auto &r : s.results)
                if (r.ticket == ticket && r.ok)
                {
                    const auto points = history_points(r.response);
                    return points.size() >= 288 && points.size() <= 300 &&
                           r.response.dump().size() < 12000;
                }
            return false;
        }));
    CHECK(s.country == "US");
    // Push-to-talk: a second of audio, sent as it arrives, then the end; heard and answered.
    const auto run = client.listen("", "");
    CHECK(run != 0);
    std::vector<std::int16_t> block(256, 1200);
    for (int i = 0; i < 16000 / 256; ++i)
        client.audio(block.data(), block.size());
    client.audio(block.data(), 16000 % 256);
    client.end_audio();
    CHECK(wait([&] { return s.assist.run == run && s.assist.stage == Assist::Stage::done; }));
    CHECK(s.assist.heard == "what time is it" && s.assist.reply == "It's five past two." &&
          s.assist.conversation == "talk-1");
    // A new run while the last still takes audio ends the last one first (Home Assistant would
    // otherwise wait on it): the server has ended two runs before it answers the third.
    const auto cut = client.listen("", "");
    client.audio(block.data(), block.size());
    CHECK(wait([&] { return s.assist.run == cut && s.assist.stage == Assist::Stage::listening; }));
    const auto again = client.listen("", "");
    for (int i = 0; i < 16000 / 256; ++i)
        client.audio(block.data(), block.size());
    client.audio(block.data(), 16000 % 256);
    client.end_audio();
    CHECK(wait([&] { return s.assist.run == again && s.assist.stage == Assist::Stage::done; }));
    CHECK(s.assist.conversation == "talk-3");
    // Replaced before Home Assistant says it started: it is ended as soon as it does, so the
    // server has ended five runs before it answers this one.
    const auto slow = client.listen("", "slow");
    CHECK(wait([&] { return s.assist.run == slow && s.assist.stage == Assist::Stage::starting; }));
    const auto after = client.listen("", "");
    for (int i = 0; i < 16000 / 256; ++i)
        client.audio(block.data(), block.size());
    client.audio(block.data(), 16000 % 256);
    client.end_audio();
    CHECK(wait([&] { return s.assist.run == after && s.assist.stage == Assist::Stage::done; }));
    CHECK(s.assist.conversation == "talk-5");
    // Half a second heard before the run starts (the app waits for speech) is its beginning:
    // 8000 samples before, 8000 after, and the server hears the whole second.
    std::vector<std::int16_t> half(8000, 1200);
    client.audio(half.data(), half.size());
    const auto spoken = client.listen("", "");
    client.audio(half.data(), half.size());
    client.end_audio();
    CHECK(wait([&] { return s.assist.run == spoken && s.assist.stage == Assist::Stage::done; }));
    CHECK(s.assist.conversation == "talk-6");
    // Watching another entity resubscribes without dropping the session.
    client.watch(c, {"weather.home", "timer.washing_machine"});
    CHECK(wait([&] { return s.states.has("timer.washing_machine"); }));
    CHECK(s.ready);
    // The server drops this one before answering: it fails and is never sent again.
    ticket = client.send(service("view_assist", "cancel_timer", {{"timer_id", "drop"}}));
    CHECK(wait(
        [&]
        {
            for (const auto &r : s.results)
                if (r.ticket == ticket)
                    return !r.ok;
            return false;
        }));
    CHECK(wait([&] { return s.ready && s.generation > 1; }));
    client.stop();
    return 0;
}
// What came after 1.0: rooms, the 12-hour clock, settings from Home Assistant, Now Playing, the
// doorbell, the photo frame and push-to-talk.
void test_features()
{
    // The clock in 12 hours, by setting or by Home Assistant's country.
    Zone twelve;
    twelve.twelve_hour = true;
    const auto afternoon = parse_iso_ms("2026-11-02T14:05:00+00:00", Zone{});
    const auto midnight = parse_iso_ms("2026-11-02T00:30:00+00:00", Zone{});
    std::string period;
    CHECK(hhmm(afternoon, twelve) == "2:05 PM" && hhmm(midnight, twelve) == "12:30 AM");
    CHECK(clock_digits(afternoon, twelve, period) == "2:05" && period == "PM");
    CHECK(clock_digits(afternoon, Zone{}, period) == "14:05" && period.empty());
    CHECK(twelve_hour_country("US") && !twelve_hour_country("GB") && !twelve_hour_country(""));

    // Rooms: a list, or 1.0's single room_page; Home Assistant's settings never take the
    // connection.
    auto base = Config::parse({{"host", "192.0.2.20"}, {"token", "mine"}});
    auto legacy = Config::parse({{"room_page", {{"name", "Den"}, {"media", "media_player.den"}}}});
    CHECK(legacy.rooms.size() == 1 && legacy.rooms[0].name == "Den");
    const Json remote{{"host", "192.0.2.99"},
                      {"token", "theirs"},
                      {"settings_entity", ""},
                      {"clock", "12h"},
                      {"rooms", {{{"name", "Kitchen"}, {"tiles", {{{"entity", "light.k"}}}}}}}};
    auto applied = merged(base, remote, false);
    CHECK(applied.host == "192.0.2.20" && applied.token == "mine" &&
          applied.settings_entity == "sensor.lamplight_ps5");
    CHECK(applied.clock == "12h" && applied.rooms.size() == 1 &&
          applied.rooms[0].name == "Kitchen");
    CHECK(merged(base, remote, true).token == "theirs");
    CHECK(Config::parse(applied.json()).json() == applied.json());
    CHECK(Config::parse({{"photos", "/media/local"}}).photos.empty());
    // 1.0's view_assist_sensor still works through a merge, under an empty view_assist.
    CHECK(merged(base, {{"view_assist_sensor", "sensor.va"}}, true).view_assist == "sensor.va");

    // A param.sfo's TITLE; lengths and offsets that point outside it are refused.
    {
        std::vector<std::uint8_t> sfo(64, 0);
        const std::uint8_t header[] = {0, 'P', 'S', 'F', 1, 1, 0, 0, 36, 0,
                                       0, 0,   42,  0,   0, 0, 1, 0, 0,  0};
        std::copy(std::begin(header), std::end(header), sfo.begin());
        const std::uint8_t entry[] = {0, 0, 4, 2, 7, 0, 0, 0, 7, 0, 0, 0, 0, 0, 0, 0};
        std::copy(std::begin(entry), std::end(entry), sfo.begin() + 20);
        std::memcpy(sfo.data() + 36, "TITLE", 6);
        std::memcpy(sfo.data() + 42, "Broforce", 8);
        sfo.resize(50);
        CHECK(console::sfo_title(sfo.data(), sfo.size()) == "Broforc");
        sfo[24] = 0xf0, sfo[25] = 0xff, sfo[26] = 0xff, sfo[27] = 0xff; // length 0xfffffff0
        CHECK(console::sfo_title(sfo.data(), sfo.size()).empty());
    }

    // Requests: Now Playing's services on a room's player, photos only inside the folder.
    FixtureHome home;
    auto config = home.config();
    CHECK(allowed(service("media_player", "volume_set", {{"volume_level", 0.4}},
                          "media_player.living_room_speaker")
                      .message,
                  config));
    CHECK(allowed(
        service("media_player", "media_previous_track", {}, "media_player.kitchen_speaker").message,
        config));
    CHECK(!allowed(service("media_player", "volume_set", {}, "media_player.bedroom").message,
                   config));
    CHECK(allowed({{"type", "media_source/browse_media"},
                   {"media_content_id", "media-source://media_source/local/photos/2025"}},
                  config));
    // Setup's folder chooser lists from the top; a picture's address only comes from inside the
    // chosen folder.
    CHECK(allowed({{"type", "media_source/browse_media"}}, config));
    CHECK(allowed({{"type", "media_source/browse_media"},
                   {"media_content_id", "media-source://media_source/local/private"}},
                  config));
    CHECK(!allowed({{"type", "media_source/browse_media"}, {"media_content_id", "/etc/passwd"}},
                   config));
    CHECK(allowed({{"type", "media_source/resolve_media"},
                   {"media_content_id", "media-source://media_source/local/photos/a.jpg"}},
                  config));
    CHECK(!allowed({{"type", "media_source/resolve_media"},
                    {"media_content_id", "media-source://media_source/local/private/a.jpg"}},
                   config));
    CHECK(!allowed({{"type", "media_source/resolve_media"},
                    {"media_content_id", "media-source://media_source/local/photos_old/a.jpg"}},
                   config));
    {
        auto immich = config;
        immich.photos = "media-source://immich/u|albums|1";
        CHECK(allowed({{"type", "media_source/resolve_media"},
                       {"media_content_id", "media-source://immich/u|albums|1|a|b.jpg|image/jpeg"}},
                      immich));
        CHECK(!allowed({{"type", "media_source/resolve_media"},
                        {"media_content_id", "media-source://immich/u|albums|12|a|b.jpg"}},
                       immich));
    }

    // The doorbell: a binary sensor turning on, or an event entity's new time; not the first
    // reading, nor coming back from unavailable.
    CHECK(doorbell_rang("binary_sensor.door", "off", "on"));
    CHECK(!doorbell_rang("binary_sensor.door", "on", "off"));
    CHECK(!doorbell_rang("binary_sensor.door", "", "on"));
    CHECK(doorbell_rang("event.door", "2026-11-02T14:00:00+00:00", "2026-11-02T14:05:00+00:00"));
    CHECK(!doorbell_rang("event.door", "unavailable", "2026-11-02T14:05:00+00:00"));
    CHECK(photo_ids(home.fixture["media"]["browse"]).size() == 2);

    Application app(config, no_fonts());
    home.wire(app);
    std::vector<std::string> pictures;
    app.request_picture = [&](const std::string &url) { pictures.push_back(url); };
    std::set<std::string> watched;
    app.watch = [&](const Config &, std::set<std::string> w) { watched = std::move(w); };
    home.settle(app);
    CHECK(app.watched().contains("camera.front_door") &&
          app.watched().contains("sensor.lamplight_ps5"));

    // Square steps through the rooms.
    app.open(Page::room);
    CHECK(app.page_title() == "Living room");
    home.press(app, Action::west);
    CHECK(app.page_title() == "Kitchen");
    home.press(app, Action::west);
    CHECK(app.page_title() == "Living room");

    // Now Playing: the cover is asked for, Cross plays or pauses, Up sets the volume.
    app.open(Page::music);
    home.frame(app);
    CHECK(std::count(pictures.begin(), pictures.end(),
                     "http://127.0.0.1:8123/api/media_player_proxy/"
                     "media_player.living_room_speaker?token=fixture&cache=1") == 1);
    home.sent.clear();
    home.press(app, Action::confirm);
    CHECK(home.sent.size() == 1 &&
          sent_call(home.sent[0].second) == "media_player.media_play_pause");
    home.reply(app);
    home.move(app, Direction::up);
    home.frame(app);
    CHECK(home.sent.size() == 1 && sent_call(home.sent[0].second) == "media_player.volume_set" &&
          std::fabs(number(field(field(home.sent[0].second, "service_data"), "volume_level")) -
                    0.35) < 0.001);
    home.reply(app);
    // The last step is sent even after leaving the page, once; nothing more while Home
    // Assistant doesn't move (the fixture's player stays at 32%).
    auto volume_sends = [&]
    {
        std::vector<double> levels;
        for (const auto &[t, m] : home.sent)
            if (sent_call(m) == "media_player.volume_set")
                levels.push_back(number(field(field(m, "service_data"), "volume_level")));
        return levels;
    };
    home.sent.clear();
    home.clock.utc_ms += 400;
    home.move(app, Direction::up);
    home.move(app, Direction::up);
    home.press(app, Action::back);
    CHECK(app.page() == Page::home && volume_sends() == std::vector<double>{0.4});
    home.clock.utc_ms += 400;
    home.frame(app);
    CHECK(volume_sends().size() == 2 && std::fabs(volume_sends()[1] - 0.45) < 0.001);
    home.reply(app); // answers (and forgets) what was sent
    for (int i = 0; i < 10; ++i)
    {
        home.clock.utc_ms += 500;
        home.frame(app);
    }
    CHECK(volume_sends().empty());
    // The same level again after Home Assistant didn't take it is sent again: the player is at
    // 40% now, so Up asks for 45% once more.
    home.snapshot.states.apply(Json::parse(
        R"({"c": {"media_player.living_room_speaker": {"+": {"a": {"volume_level": 0.4}}}}})"));
    home.reply(app);
    app.open(Page::music);
    home.move(app, Direction::up);
    home.frame(app);
    CHECK(volume_sends().size() == 1 && std::fabs(volume_sends()[0] - 0.45) < 0.001);
    home.reply(app);
    app.open(Page::home);

    // The doorbell rings: the camera takes the screen, refreshing, until Circle.
    app.open(Page::home);
    home.snapshot.states.apply(
        Json::parse(R"({"c": {"binary_sensor.front_door_ding": {"+": {"s": "on"}}}})"));
    home.reply(app);
    home.frame(app);
    CHECK(app.camera_showing() && app.alerting());
    CHECK(!pictures.empty() &&
          pictures.back().starts_with("http://127.0.0.1:8123/api/camera_proxy/camera.front_door"
                                      "?token=fixture&lamplight="));
    // A slow frame is waited for, not replaced; once it is back, the next is asked for.
    const auto frames = pictures.size();
    const auto first = pictures.back();
    home.clock.utc_ms += 6000;
    home.frame(app);
    CHECK(pictures.size() == frames);
    app.picture(first, 4242, 16, 9);
    home.clock.utc_ms += 1000;
    home.frame(app);
    CHECK(pictures.size() == frames + 1 && pictures.back() != first);
    home.press(app, Action::back);
    CHECK(!app.camera_showing() && app.page() == Page::home);

    // The photo frame: once the clock rests, the folder is read and a picture asked for.
    home.sent.clear();
    for (int i = 0; i < 130 * 4; ++i)
        home.frame(app, {}, 0.25f);
    CHECK(std::any_of(home.sent.begin(), home.sent.end(), [](const auto &s)
                      { return text(s.second, "type") == "media_source/browse_media"; }));
    // The folder, then the folder inside it, then a picture.
    for (int i = 0; i < 3; ++i)
    {
        home.reply(app);
        home.frame(app);
    }
    home.reply(app);
    CHECK(std::count(pictures.begin(), pictures.end(),
                     "http://127.0.0.1:8123/media/local/photos/harbour.jpg?authSig=fixture" +
                         std::string(kSoftPicture)) == 1);

    // Settings from Home Assistant: its rooms replace these, and the client is told.
    home.snapshot.states.apply(Json::parse(R"({"a": {"sensor.lamplight_ps5": {"s": "ready",
        "a": {"lamplight": {"rooms": [{"name": "Studio", "tiles": [{"entity": "light.studio"}]}]}}}}})"));
    home.reply(app);
    CHECK(app.config().rooms.size() == 1 && app.config().rooms[0].name == "Studio" &&
          app.config().host == "127.0.0.1" && watched.contains("light.studio"));
    // A new weather entity from there forgets the old forecast and fetches the new one.
    home.sent.clear();
    home.snapshot.states.apply(Json::parse(R"({"a": {"sensor.lamplight_ps5": {"s": "ready",
        "a": {"lamplight": {"weather": "weather.cabin"}}}}})"));
    home.reply(app);
    home.frame(app);
    CHECK(std::any_of(home.sent.begin(), home.sent.end(),
                      [](const auto &s)
                      {
                          return sent_call(s.second) == "weather.get_forecasts" &&
                                 text(field(s.second, "target"), "entity_id") == "weather.cabin";
                      }));

    // The house's notices: newest first, levels kept, and dismissing sends only its event.
    {
        const auto list = parse_notices(
            field(home.fixture["states"]["a"]["sensor.house_notices"]["a"], "items"), Zone{});
        CHECK(list.size() == 2 && list[0].title == "Front door open" && list[0].level == "warn" &&
              list[1].rooms == std::vector<std::string>{"kitchen"});
        CHECK(ago(list[0].at, list[0].at + 30000, Zone{}) == "just now" &&
              ago(list[0].at, list[0].at + 5 * 60000, Zone{}) == "5 min ago" &&
              ago(list[0].at, list[0].at + 3 * 3600000, Zone{}) == "3 h ago");
        CHECK(
            parse_notices(
                Json::parse(R"([{"message": "no title"}, {"title": "T", "level": "odd"}])"), Zone{})
                .size() == 1);
        CHECK(allowed({{"type", "fire_event"}, {"event_type", "house_notice_dismiss"}}, config));
        CHECK(!allowed({{"type", "fire_event"}, {"event_type", "house_notice"}}, config));
        // The moon: new at the reference, full half a month later.
        const auto new_moon = parse_iso_ms("2000-01-06T18:14:00Z", Zone{});
        CHECK(moon_phase(new_moon) < 0.001 || moon_phase(new_moon) > 0.999);
        CHECK(std::fabs(moon_phase(new_moon + static_cast<std::int64_t>(14.765 * 86400000)) - 0.5) <
              0.001);
    }
    app.open(Page::house);
    CHECK(app.page_title() == "House");
    for (int i = 0; i < 60; ++i) // the clock rested a moment ago: let it wake
        home.frame(app);
    home.sent.clear();
    home.press(app, Action::confirm);
    CHECK(home.sent.size() == 1 && text(home.sent[0].second, "type") == "fire_event" &&
          text(field(home.sent[0].second, "event_data"), "key") == "door:front");
    home.reply(app);
    // A new notice pops up; an alert one takes the screen.
    home.snapshot.states.apply(Json::parse(R"({"c": {"sensor.house_notices": {"+": {"a": {"items": [
        {"id": "n3", "title": "Max needs help", "message": "Stuck under the sofa", "level": "info",
         "at": "2026-11-02T14:04:00+00:00"}]}}}}})"));
    home.reply(app);
    CHECK(app.toast_text() == "Max needs help \xC2\xB7 Stuck under the sofa" && !app.alerting());
    home.snapshot.states.apply(Json::parse(R"({"c": {"sensor.house_notices": {"+": {"a": {"items": [
        {"id": "n4", "title": "Water leak", "message": "Under the sink", "level": "alert",
         "at": "2026-11-02T14:04:30+00:00"}]}}}}})"));
    home.reply(app);
    CHECK(app.alerting());
    home.press(app, Action::back);
    CHECK(!app.alerting());
    // The list missing for a moment, then back: nothing pops up again. The same notice raised
    // again (a new id, the same key) neither; a warning that becomes an alert takes the screen.
    home.snapshot.states.apply(
        Json::parse(R"({"c": {"sensor.house_notices": {"+": {"a": {"items": null}}}}})"));
    home.reply(app);
    home.snapshot.states.apply(Json::parse(R"({"c": {"sensor.house_notices": {"+": {"a": {"items": [
        {"id": "n4", "title": "Water leak", "message": "Under the sink", "level": "alert",
         "at": "2026-11-02T14:04:30+00:00"}]}}}}})"));
    home.reply(app);
    CHECK(!app.alerting());
    home.snapshot.states.apply(Json::parse(R"({"c": {"sensor.house_notices": {"+": {"a": {"items": [
        {"id": "n5", "key": "door:front", "title": "Front door open", "level": "warn",
         "at": "2026-11-02T14:04:40+00:00"}]}}}}})"));
    home.reply(app);
    home.snapshot.states.apply(Json::parse(R"({"c": {"sensor.house_notices": {"+": {"a": {"items": [
        {"id": "n6", "key": "door:front", "title": "Front door open", "level": "warn",
         "at": "2026-11-02T14:04:50+00:00"}]}}}}})"));
    home.reply(app);
    CHECK(!app.alerting());
    home.snapshot.states.apply(Json::parse(R"({"c": {"sensor.house_notices": {"+": {"a": {"items": [
        {"id": "n7", "key": "door:front", "title": "Front door still open", "level": "alert",
         "at": "2026-11-02T14:04:55+00:00"}]}}}}})"));
    home.reply(app);
    CHECK(app.alerting());
    home.press(app, Action::back);
    app.open(Page::home);

    // Push-to-talk: R2 held opens the microphone; letting go ends it; the answer is shown.
    Application talk(home.config(), no_fonts());
    home.wire(talk);
    std::uint64_t runs = 0;
    bool open = false;
    talk.listen = [&](const std::string &, const std::string &) { return ++runs; };
    talk.open_microphone = [&] { return open = true; };
    talk.close_microphone = [&] { open = false; };
    float level = 0.3f;
    talk.microphone_level = [&] { return level; };
    home.settle(talk);
    hui::InputFrame hold;
    hold.pressed = hold.held = hui::action_bit(Action::jump_next);
    home.frame(talk, hold);
    hold.pressed = 0;
    home.frame(talk, hold);
    CHECK(open && runs == 1);
    home.frame(talk);
    CHECK(!open);
    home.snapshot.assist = {
        1, Assist::Stage::done, "what time is it", "It's five past two.", "talk-1", ""};
    home.reply(talk);
    home.frame(talk);
    CHECK(talk.toast_text().find("It's five past two.") != std::string::npos);
    // An answer's picture (View Assist's) goes when a spoken answer replaces it, and a picture
    // two slots wait on reaches both.
    std::vector<std::string> asked;
    talk.request_picture = [&](const std::string &url) { asked.push_back(url); };
    home.snapshot.states.apply(Json::parse(R"({"c": {"sensor.viewassist_living_room": {"+": {"a":
        {"message": "Here's the garden", "image": "/local/garden.jpg"}}}}})"));
    home.reply(talk);
    home.frame(talk);
    CHECK(asked == std::vector<std::string>{"http://127.0.0.1:8123/local/garden.jpg"});
    talk.picture(asked[0], 601, 16, 9);
    hold.pressed = hold.held;
    home.frame(talk, hold);
    hold.pressed = 0;
    home.frame(talk, hold);
    home.frame(talk);
    home.snapshot.assist = {2, Assist::Stage::done, "turn on the fireplace", "Done.", "", ""};
    home.reply(talk);
    home.frame(talk); // the answer arrives
    home.frame(talk); // and its picture slot follows
    const auto released = talk.take_released();
    CHECK(std::find(released.begin(), released.end(), 601u) != released.end());
    // The same picture for the answer and the cover: each slot gets its own copy.
    const std::string cover =
        "http://127.0.0.1:8123/api/media_player_proxy/media_player.living_room_speaker"
        "?token=fixture&cache=1";
    talk.open(Page::music);
    home.snapshot.states.apply(Json::parse(R"({"c": {"sensor.viewassist_living_room": {"+": {"a":
        {"message": "Now playing", "image": "/api/media_player_proxy/media_player.living_room_speaker?token=fixture&cache=1"}}}}})"));
    home.reply(talk);
    home.frame(talk);
    asked.clear();
    talk.picture(cover, 701, 16, 16);
    CHECK(asked == std::vector<std::string>{cover});
    talk.picture(cover, 702, 16, 16);
    CHECK(talk.take_released().empty());
    // Silence all the way through: Home Assistant is never asked, so speech-to-text can't make a
    // command out of it.
    level = 0.004f;
    const auto asked_before = runs;
    hold.pressed = hold.held;
    home.frame(talk, hold);
    hold.pressed = 0;
    home.frame(talk, hold);
    home.frame(talk, hold);
    home.frame(talk);
    CHECK(runs == asked_before && !open);
    CHECK(talk.toast_text().starts_with("The microphone heard nothing"));
    level = 0.3f;
    // No microphone: it says so.
    talk.open_microphone = [] { return false; };
    hold.pressed = hold.held;
    home.frame(talk, hold);
    CHECK(talk.toast_text().starts_with("No microphone"));
    home.snapshot.assist = {};
}
void set_state(FixtureHome &h, Application &a, const std::string &entity, const std::string &state)
{
    h.snapshot.states.apply({{"c", {{entity, {{"+", {{"s", state}}}}}}}});
    h.reply(a);
}
void test_scene_row()
{
    FixtureHome h;
    const auto config = h.config();
    const auto &room = config.rooms[0];
    CHECK(room_light(room) == "light.living_room");
    CHECK(room_light(config.rooms[1]) == "light.kitchen"); // no light named: its first
    auto row = scene_row(room, h.snapshot.states);
    CHECK(row && row->options.size() == 4 && row->kind == Control::Kind::options);
    CHECK(row->selected == 1); // 55% and warm: within 10 of Relax, on its side of warm and cool
    h.snapshot.states.apply(Json::parse(
        R"({"c": {"light.living_room": {"+": {"a": {"brightness": 255, "color_temp_kelvin": 2700}}}}})"));
    CHECK(scene_row(room, h.snapshot.states)->selected == -1); // bright, but warm: not Bright
    h.snapshot.states.apply(
        Json::parse(R"({"c": {"light.living_room": {"+": {"a": {"color_temp_kelvin": 4000}}}}})"));
    CHECK(scene_row(room, h.snapshot.states)->selected == 0);
    h.snapshot.states.apply(Json::parse(R"({"c": {"light.living_room": {"+": {"s": "off"}}}})"));
    CHECK(scene_row(room, h.snapshot.states)->selected == 3);
    // A mode holding beats how the light looks, but not the lamps being off: a lit pill means lamps
    // are on.
    h.snapshot.states.apply(
        Json::parse(R"({"c": {"input_boolean.cinema_mode": {"+": {"s": "on"}}}})"));
    CHECK(scene_row(room, h.snapshot.states)->selected == 3);
    h.snapshot.states.apply(Json::parse(R"({"c": {"light.living_room": {"+": {"s": "on"}}}})"));
    row = scene_row(room, h.snapshot.states);
    CHECK(row->selected == 2);
    const auto bright = option_request(*row, 0);
    CHECK(bright && sent_call(bright->message) == "light.turn_on");
    CHECK(field(bright->message, "service_data") ==
          Json({{"brightness_pct", 100}, {"color_temp_kelvin", 4000}}));
    CHECK(text(field(bright->message, "target"), "entity_id") == "light.living_room");
    const auto cinema = option_request(*row, 2);
    CHECK(cinema && sent_call(cinema->message) == "script.cinema_mode_toggle");
    const auto off = option_request(*row, 3);
    CHECK(off && sent_call(off->message) == "light.turn_off");
    // No scenes, no row.
    CHECK(!scene_row(config.rooms[1], h.snapshot.states));
}
void test_history()
{
    const Json rows = {{"sensor.a",
                        {{{"s", "20.5"}, {"lu", 1000.0}},
                         {{"s", "unavailable"}, {"lu", 1100.0}},
                         {{"s", "21"}, {"lu", 1200.5}},
                         {{"s", "22"}, {"lu", 1250.0}}, // the same five minutes: the last stands
                         {{"s", "23"}, {"lc", 1600.0}}}}};
    auto points = history_points(compact_history(rows, "sensor.a", ""));
    CHECK(points.size() == 3);
    CHECK(points[0].at == 1000000 && points[0].value == 20.5);
    CHECK(points[1].at == 1250000 && points[1].value == 22);
    CHECK(points[2].at == 1600000); // only lc says when
    // An attribute instead of the state, and a row without it is skipped.
    const Json climate = {{"climate.a",
                           {{{"s", "heat"}, {"a", {{"current_temperature", 19.5}}}, {"lu", 3000.0}},
                            {{"s", "heat"}, {"a", Json::object()}, {"lu", 3600.0}}}}};
    points = history_points(compact_history(climate, "climate.a", "current_temperature"));
    CHECK(points.size() == 1 && points[0].value == 19.5);
    // Never mind what comes back.
    CHECK(history_points(compact_history(Json(), "sensor.a", "")).empty());
    CHECK(history_points(compact_history(rows, "sensor.other", "")).empty());
    CHECK(history_points(Json("nonsense")).empty());
    // A long day stays small.
    auto many = Json::array();
    for (int i = 0; i < 20000; ++i)
        many.push_back({{"s", "1"}, {"lu", 1000.0 + i * 7}});
    const auto small = compact_history({{"sensor.a", many}}, "sensor.a", "");
    CHECK(field(small, "points").size() <= 300);
    // The axis: whole numbers, at least two apart, with the target in range.
    auto scale = chart_scale({{0, 20.2}, {1, 20.4}}, std::nan(""));
    CHECK(scale.low == 19 && scale.high == 22);
    scale = chart_scale({{0, 18.5}, {1, 19.5}}, 21);
    CHECK(scale.low == 18 && scale.high == 21);
    scale = chart_scale({}, std::nan(""));
    CHECK(scale.low == 0 && scale.high == 1);
    // The ask: a sensor's own state, small; a thermostat's reading needs attributes.
    const auto own = history_request("sensor.a", "", 86400000 * 2, Zone{});
    CHECK(text(own.message, "type") == "history/history_during_period");
    CHECK(field(own.message, "no_attributes") == true &&
          field(own.message, "minimal_response") == true);
    CHECK(field(own.message, "significant_changes_only") == false);
    CHECK(text(own.message, "start_time").starts_with("1970-01-02T00:00:00"));
    const auto attribute = history_request("climate.a", "current_temperature", 0, Zone{});
    CHECK(field(attribute.message, "no_attributes") == false);
    CHECK(attribute.history_attribute == "current_temperature");
}
void test_allowed_calls()
{
    FixtureHome h;
    const auto c = h.config();
    const std::set<std::string> watched{"sensor.living_room_temperature", "light.fireplace"};
    auto ok = [&](const Request &r) { return allowed(r.message, c, watched); };
    // A scene sets the room's main light, and its own service exactly as settings name it.
    CHECK(ok(service("light", "turn_on", {{"brightness_pct", 45}}, "light.living_room")));
    CHECK(ok(service("script", "cinema_mode_toggle", Json::object())));
    CHECK(!ok(service("script", "cinema_mode_toggle", {{"anything", 1}})));
    CHECK(!ok(service("script", "something_else", Json::object())));
    // A boost: that temperature, for that time, and no other.
    const Json warm{{"room", "living_room"}, {"temperature", 21}, {"minutes", 120}, {"by", "PS5"}};
    CHECK(ok(service("script", "heating_set_room", warm)));
    auto hotter = warm;
    hotter["temperature"] = 30;
    CHECK(!ok(service("script", "heating_set_room", hotter)));
    // The house: its scripts, and its modes' switches; not any other switch.
    CHECK(ok(service("script", "house_everything_off", Json::object())));
    CHECK(ok(service("script", "good_night", {{"bedside", "off"}})));
    CHECK(!ok(service("script", "good_night", Json::object())));
    CHECK(ok(service("input_boolean", "toggle", Json::object(), "input_boolean.house_away")));
    CHECK(!ok(service("input_boolean", "toggle", Json::object(), "input_boolean.other")));
    // A chart reads what is watched, one at a time.
    auto history = history_request("sensor.living_room_temperature", "", 0, Zone{});
    CHECK(ok(history));
    CHECK(!ok(history_request("sensor.secret", "", 0, Zone{})));
    history.message["entity_ids"] = {"sensor.living_room_temperature", "light.fireplace"};
    CHECK(!ok(history));
    // Speakers: moved and joined among themselves, and their sleep timers.
    const auto move = [](const std::string &from, const std::string &to)
    {
        return service("music_assistant", "transfer_queue",
                       {{"source_player", from}, {"auto_play", true}}, to);
    };
    CHECK(ok(move("media_player.kitchen_speaker", "media_player.living_room_speaker")));
    CHECK(ok(move("media_player.home_speakers", "media_player.bedroom_speaker")));
    CHECK(!ok(move("media_player.kitchen_speaker", "media_player.neighbour")));
    CHECK(!ok(move("media_player.neighbour", "media_player.kitchen_speaker")));
    const auto join = [](const std::string &who, const std::string &group)
    { return service("media_player", "join", {{"group_members", Json::array({who})}}, group); };
    CHECK(ok(join("media_player.kitchen_speaker", "media_player.home_speakers")));
    CHECK(!ok(join("media_player.neighbour", "media_player.home_speakers")));
    CHECK(!ok(join("media_player.kitchen_speaker", "media_player.kitchen_speaker")));
    CHECK(ok(service("media_player", "unjoin", Json::object(), "media_player.bedroom_speaker")));
    CHECK(ok(service("media_player", "volume_set", {{"volume_level", 0.3}},
                     "media_player.home_speakers")));
    const auto sleep = [](const std::string &player, int minutes)
    { return service("script", "music_sleep_timer", {{"player", player}, {"minutes", minutes}}); };
    CHECK(ok(sleep("media_player.bedroom_speaker", 30)));
    CHECK(ok(sleep("media_player.bedroom_speaker", 0)));
    CHECK(!ok(sleep("media_player.neighbour", 30)));
    CHECK(!ok(sleep("media_player.bedroom_speaker", 100000)));
    // Undo: its own scene, of lamps it is watching.
    const auto note = [](const std::string &id, const std::string &lamp)
    {
        return service("scene", "create",
                       {{"scene_id", id}, {"snapshot_entities", Json::array({lamp})}});
    };
    CHECK(ok(note(kUndoSceneId, "light.fireplace")));
    CHECK(!ok(note(kUndoSceneId, "light.not_watched")));
    CHECK(!ok(note("other", "light.fireplace")));
    CHECK(!ok(service("scene", "create", {{"scene_id", kUndoSceneId}})));
    CHECK(ok(service("scene", "turn_on", Json::object(), std::string("scene.") + kUndoSceneId)));
    CHECK(!ok(service("scene", "turn_on", Json::object(), "scene.other")));
    // A room's own speaker, though settings list others: it can still be moved to.
    auto listed = c;
    listed.speakers.rooms.resize(1);
    CHECK(allowed(move("media_player.living_room_speaker", "media_player.kitchen_speaker").message,
                  listed, watched));
    // Without a group, nothing can be joined to nothing.
    auto alone = c;
    alone.speakers = {};
    CHECK(!allowed(service("media_player", "join",
                           {{"group_members", Json::array({"media_player.kitchen_speaker"})}}, "")
                       .message,
                   alone, watched));
}
void test_settings_round_trip()
{
    FixtureHome h;
    const auto c = h.config();
    const auto again = Config::parse(c.json());
    CHECK(again.rooms[0].light == "light.living_room" && again.rooms[0].scenes.size() == 4);
    CHECK(again.rooms[0].scenes[0].brightness == 100 && again.rooms[0].scenes[0].kelvin == 4000);
    CHECK(again.rooms[0].scenes[2].call.service == "script.cinema_mode_toggle");
    CHECK(again.rooms[0].scenes[2].entity == "input_boolean.cinema_mode");
    CHECK(again.rooms[0].scenes[3].off);
    const auto &radiator = again.rooms[0].tiles[3];
    CHECK(radiator.sensor == "sensor.living_room_temperature" && radiator.boosts.size() == 2);
    CHECK(radiator.boosts[0].call.response &&
          radiator.boosts[0].call.data.value("temperature", 0) == 21);
    CHECK(again.house.actions.size() == 2 && again.house.actions[0].confirm &&
          again.house.actions[0].undo_lights);
    CHECK(again.house.undo_lights == "light.all_house_lights");
    CHECK(again.house.modes.size() == 4 && again.house.modes[0].confirm_off);
    CHECK(again.house.modes[0].on.service == "script.house_guests_on");
    CHECK(again.house.modes[1].off.service == "script.cinema_mode_toggle");
    CHECK(again.house.actions[1].call.data == Json({{"bedside", "off"}}));
    CHECK(again.speakers.group == "media_player.home_speakers" &&
          again.speakers.group_name == "Everywhere");
    CHECK(again.speakers.rooms.size() == 3 &&
          again.speakers.rooms[2].sleep_timer == "timer.music_sleep_bedroom");
    CHECK(again.speakers.sleep_script == "script.music_sleep_timer");
    CHECK(again.speakers.sleep_minutes == std::vector<int>({15, 30, 60}));
    // Over Home Assistant's settings, what wasn't mentioned stays.
    const auto over = merged(c, {{"clock", "24h"}}, false);
    CHECK(over.house.modes.size() == 4 && over.speakers.rooms.size() == 3 &&
          over.rooms[0].scenes.size() == 4);
    // Nonsense is left out, not half-kept.
    const auto bad = Config::parse(Json::parse(R"({
        "house": {"actions": [{"name": "No service"}, {"name": "Bad", "service": "nodot"}],
                  "modes": [{"name": "No entity"}]},
        "speakers": {"sleep_script": "light.nope", "rooms": [{"name": "No player"}],
                     "sleep_minutes": [0, 5000, 20]},
        "rooms": [{"name": "R", "tiles": [{"entity": "light.a", "boosts": [{"name": "x"}]}],
                   "scenes": [{"icon": "mdi:x"}, {"name": "Too bright", "brightness": 900}]}]})"));
    CHECK(bad.house.empty());
    CHECK(bad.speakers.rooms.empty() && bad.speakers.sleep_script.empty());
    CHECK(bad.speakers.sleep_minutes == std::vector<int>({20}));
    CHECK(bad.rooms[0].tiles[0].boosts.empty());
    CHECK(bad.rooms[0].scenes.size() == 1 && bad.rooms[0].scenes[0].brightness == 0);
    // watched: what these pages need.
    Application a(c, no_fonts());
    const auto ids = a.watched();
    for (const char *id : {"sensor.living_room_temperature", "input_boolean.cinema_mode",
                           "input_boolean.house_away", "media_player.home_speakers",
                           "media_player.bedroom_speaker", "timer.music_sleep_bedroom"})
        CHECK(ids.contains(id));
}
void test_scenes_in_sheets()
{
    {
        // All lamps' levels still waiting to go don't undo the scene row's Off.
        FixtureHome f;
        Application b(f.config(), no_fonts());
        f.wire(b);
        f.settle(b);
        b.open(Page::room);
        f.move(b, Direction::left);
        f.move(b, Direction::left);
        f.move(b, Direction::down);
        f.press(b, Action::north);
        CHECK(b.sheet_showing());
        f.move(b, Direction::right);
        f.frame(b);
        CHECK(!calls_sent(f).empty());
        f.reply(b);
        f.move(b, Direction::right); // the lamp already at the first step's level goes now
        f.frame(b);
        f.reply(b);
        f.move(b, Direction::right); // these wait their 300 ms
        f.frame(b);
        CHECK(calls_sent(f).empty());
        f.move(b, Direction::up);
        f.move(b, Direction::right);
        f.move(b, Direction::right);
        f.press(b, Action::confirm);
        auto off = calls_sent(f);
        CHECK(off.size() == 1 && sent_call(off[0]) == "light.turn_off");
        f.reply(b);
        f.clock.utc_ms += 1000;
        f.frame(b);
        f.frame(b);
        CHECK(calls_sent(f).empty());
    }
    FixtureHome h;
    Application a(h.config(), no_fonts());
    h.wire(a);
    h.settle(a);
    a.open(Page::room);
    // Main lights' sheet opens on Brightness; Scene is one up, and Power one more.
    h.press(a, Action::north);
    CHECK(a.sheet_showing());
    h.move(a, Direction::up);
    h.move(a, Direction::left); // Relax to Bright
    h.press(a, Action::confirm);
    auto calls = calls_sent(h);
    CHECK(calls.size() == 1 && sent_call(calls[0]) == "light.turn_on");
    CHECK(field(calls[0], "service_data") ==
          Json({{"brightness_pct", 100}, {"color_temp_kelvin", 4000}}));
    h.reply(a);
    // Cinema: the house's own script.
    h.move(a, Direction::right);
    h.move(a, Direction::right);
    h.press(a, Action::confirm);
    calls = calls_sent(h);
    CHECK(calls.size() == 1 && sent_call(calls[0]) == "script.cinema_mode_toggle");
    h.reply(a);
    // A level still waiting to go doesn't undo the scene chosen after it.
    h.move(a, Direction::down);
    hui::InputFrame right;
    right.nav = Direction::right;
    h.frame(a, right); // Brightness, not yet sent: the throttle holds it
    h.move(a, Direction::up);
    h.move(a, Direction::right); // Off
    h.press(a, Action::confirm);
    calls = calls_sent(h);
    CHECK(!calls.empty() && sent_call(calls.back()) == "light.turn_off");
    h.reply(a);
    h.clock.utc_ms += 1000;
    h.frame(a);
    CHECK(calls_sent(h).empty());
    h.press(a, Action::back);
    // Another lamp's sheet has no scenes: it isn't the room's main light.
    h.move(a, Direction::right);
    h.press(a, Action::north);
    h.move(a, Direction::up);
    h.move(a, Direction::up);
    h.press(a, Action::confirm);
    calls = calls_sent(h);
    CHECK(calls.size() == 1 && sent_call(calls[0]) == "light.toggle");
    h.reply(a);
    h.press(a, Action::back);
    // All lamps, from an action: scenes at the top of it too.
    h.move(a, Direction::left);
    h.move(a, Direction::left);
    h.move(a, Direction::down);
    h.press(a, Action::north);
    CHECK(a.sheet_showing());
    h.move(a, Direction::up);
    h.press(a, Action::confirm);
    calls = calls_sent(h);
    CHECK(calls.size() == 1 && sent_call(calls[0]) == "light.turn_on" &&
          text(field(calls[0], "target"), "entity_id") == "light.living_room");
}
// A group whose lamps are tiles too isn't a lamp of its own on All lamps: they'd be counted, and
// sent, twice.
void test_lamp_groups()
{
    FixtureHome h;
    h.snapshot.states.apply(Json::parse(
        R"({"c": {"light.living_room": {"+": {"a": {"entity_id": ["light.living_room_lamp", "light.fireplace"]}}}}})"));
    Application a(h.config(), no_fonts());
    h.wire(a);
    h.settle(a);
    a.open(Page::room);
    h.move(a, Direction::left);
    h.move(a, Direction::down);
    h.press(a, Action::north);
    CHECK(a.sheet_showing());
    h.sent.clear();
    h.move(a, Direction::right);
    for (int i = 0; i < 40; ++i) // the levels go out once they settle
        h.frame(a);
    std::set<std::string> lamps;
    for (const auto &call : calls_sent(h))
        lamps.insert(text(field(call, "target"), "entity_id"));
    CHECK(lamps == std::set<std::string>({"light.living_room_lamp", "light.fireplace"}));
}
// With nothing playing here, the Speakers sheet opens past Move here: a stray press mustn't take
// another room's music.
void test_speakers_focus()
{
    FixtureHome h;
    Application a(h.config(), no_fonts());
    h.wire(a);
    h.settle(a);
    set_state(h, a, "media_player.living_room_speaker", "idle");
    a.open(Page::music);
    h.press(a, Action::west);
    CHECK(a.sheet_showing());
    h.sent.clear();
    h.press(a, Action::confirm);
    const auto calls = calls_sent(h);
    CHECK(calls.size() == 1 && sent_call(calls[0]) == "script.music_sleep_timer");
}
void test_charts_and_boosts()
{
    // A thermostat reads its room sensor when it has one; the plan's doesn't show its target.
    const auto head = entity_of("heat", R"({"current_temperature": 18.4, "temperature": 25})");
    Entity room;
    room.state = "19.56";
    CHECK(thing_words("climate.x", head, room, false) == "19.6\xC2\xB0 \xC2\xB7 Set 25\xC2\xB0");
    CHECK(thing_words("climate.x", head, room, true) == "19.6\xC2\xB0");
    CHECK(thing_words("climate.x", head, Entity{}, true) == "18\xC2\xB0");
    CHECK(thing_words("climate.x", head, Entity{}, false) == state_words("climate.x", head));
    NamedCall warm;
    warm.name = "Warm";
    warm.call.service = "script.warm";
    CHECK(plan_runs("climate.x", {warm}) && !plan_runs("climate.x", {}) &&
          !plan_runs("light.x", {warm}));
    FixtureHome h;
    Application a(h.config(), no_fonts());
    h.wire(a);
    h.settle(a);
    a.open(Page::room);
    h.move(a, Direction::down);
    // The thermostat: its chart reads the room's sensor, and is asked for once.
    h.press(a, Action::confirm);
    CHECK(a.sheet_showing());
    CHECK(h.sent.size() == 1);
    CHECK(text(h.sent[0].second, "type") == "history/history_during_period");
    CHECK(strings(field(h.sent[0].second, "entity_ids")).at(0) == "sensor.living_room_temperature");
    CHECK(field(h.sent[0].second, "no_attributes") == true);
    h.reply(a);
    h.frame(a);
    CHECK(h.sent.empty());
    // Five minutes on, again.
    h.clock.utc_ms += 5 * 60000 + 1000;
    h.frame(a);
    CHECK(h.sent.size() == 1);
    h.reply(a);
    // The plan runs a radiator with warm-ups: the sheet is its chart and the warm-ups, and opens on
    // them; no target or mode to press below, and the chart can't take the focus.
    h.move(a, Direction::down);
    h.move(a, Direction::right);
    h.press(a, Action::confirm);
    {
        const auto stop = calls_sent(h);
        CHECK(stop.size() == 1 && field(stop[0], "service_data")["temperature"] == 0);
        h.reply(a);
        h.sent.clear();
    }
    h.move(a, Direction::left);
    h.move(a, Direction::up);
    h.answers["script.heating_set_room"] = {{"ok", true},
                                            {"message", "Warming the living room to 21\xC2\xB0"}};
    h.press(a, Action::confirm);
    auto calls = calls_sent(h);
    CHECK(calls.size() == 1 && sent_call(calls[0]) == "script.heating_set_room");
    CHECK(field(calls[0], "return_response") == true);
    CHECK(field(calls[0], "service_data") ==
          Json({{"room", "living_room"}, {"temperature", 21}, {"minutes", 120}, {"by", "PS5"}}));
    h.reply(a);
    CHECK(a.toast_text() == "Warming the living room to 21\xC2\xB0");
    // The house can say no, in its own words.
    h.answers["script.heating_set_room"] = {{"ok", false}, {"message", "The radiator is offline"}};
    h.press(a, Action::confirm);
    h.reply(a);
    CHECK(a.toast_text() == "The radiator is offline");
    // No words of its own: the button's.
    h.answers = Json::object();
    h.press(a, Action::confirm);
    h.reply(a);
    CHECK(a.toast_text() == "21\xC2\xB0 for 2 hours");
    h.press(a, Action::back);
    // A sensor: Cross and Triangle both show its history, and there is nothing else to press.
    a.open(Page::room);
    h.press(a, Action::west);
    h.move(a, Direction::right);
    h.move(a, Direction::right);
    h.press(a, Action::confirm);
    CHECK(a.sheet_showing());
    CHECK(h.sent.size() == 1 &&
          strings(field(h.sent[0].second, "entity_ids")).at(0) == "sensor.kitchen_temperature");
    h.reply(a);
    h.move(a, Direction::down);
    h.press(a, Action::confirm);
    CHECK(calls_sent(h).empty());
    h.press(a, Action::north);
    CHECK(!a.sheet_showing());
    h.press(a, Action::north);
    CHECK(a.sheet_showing());
    h.press(a, Action::back);
    // A sensor that isn't a number has no chart.
    Entity words;
    words.state = "dry";
    CHECK(controls_for("sensor.floor", words).empty());
    Entity reading;
    reading.state = "21.5";
    CHECK(controls_for("sensor.floor", reading).size() == 1);
    // It failed: not asked again at once, and not forever.
    FixtureHome f;
    f.fail_history = true;
    Application b(f.config(), no_fonts());
    f.wire(b);
    f.settle(b);
    b.open(Page::room);
    f.move(b, Direction::down);
    f.press(b, Action::confirm);
    CHECK(f.sent.size() == 1);
    f.reply(b);
    f.frame(b);
    CHECK(f.sent.empty());
    f.clock.utc_ms += 61000;
    f.frame(b);
    CHECK(f.sent.size() == 1);
    // Offline, nothing goes, and a press says why.
    f.reply(b);
    f.snapshot.ready = f.snapshot.connected = false;
    f.reply(b);
    f.move(b, Direction::up);
    f.press(b, Action::confirm);
    CHECK(b.toast_text() == "Not connected to Home Assistant");
}
// A cover that couldn't be fetched is asked for again, not given up on while the track plays.
void test_cover_retry()
{
    FixtureHome h;
    Application a(h.config(), no_fonts());
    h.wire(a);
    std::vector<std::string> asked;
    a.request_picture = [&](const std::string &url)
    {
        // Not its blurred copy, which is the backdrop's to ask for.
        if (url.find("media_player_proxy") != std::string::npos && !url.ends_with(kSoftPicture))
            asked.push_back(url);
    };
    h.settle(a);
    a.open(Page::music);
    h.frame(a);
    CHECK(asked.size() == 1);
    a.picture(asked[0], 0, 0, 0);
    h.frame(a);
    h.clock.utc_ms += 30000;
    h.frame(a);
    CHECK(asked.size() == 1);
    h.clock.utc_ms += 31000;
    h.frame(a);
    CHECK(asked.size() == 2 && asked[0] == asked[1]);
    // Once it has come, that's that.
    a.picture(asked[1], 9, 900, 900);
    h.clock.utc_ms += 120000;
    h.frame(a);
    CHECK(asked.size() == 2);
}
// Settings from Home Assistant arriving under a page in use.
void test_settings_arriving_over_help()
{
    FixtureHome h;
    Application a(h.config(), no_fonts());
    h.wire(a);
    h.settle(a);
    a.open(Page::help);
    // A confirm example's first press, then its place taken by another command: that one asks too.
    h.move(a, Direction::right);
    for (int i = 0; i < 3; ++i)
        h.move(a, Direction::down);
    h.press(a, Action::confirm);
    CHECK(h.sent.empty());
    h.snapshot.states.apply(Json::parse(R"({"a": {"sensor.lamplight_ps5": {"s": "ready",
        "a": {"lamplight": {"categories": [{"name": "One", "examples": [
            {"say": "a", "tap": true}, {"say": "b", "tap": true}, {"say": "c", "tap": true},
            {"say": "heat everything", "tap": true, "confirm": true}]}]}}}}})"));
    h.reply(a);
    h.frame(a);
    h.press(a, Action::confirm);
    CHECK(h.sent.empty());
    h.press(a, Action::confirm);
    CHECK(h.sent.size() == 1 && text(h.sent[0].second, "text") == "heat everything");
    h.reply(a);
    // The chosen category gone, with no controller to move: drawn from what's left.
    FixtureHome f;
    Application b(f.config(), no_fonts());
    f.wire(b);
    f.settle(b);
    b.open(Page::help);
    for (int i = 0; i < 4; ++i)
        f.move(b, Direction::down);
    f.snapshot.states.apply(Json::parse(R"({"a": {"sensor.lamplight_ps5": {"s": "ready",
        "a": {"lamplight": {"categories": [{"name": "One", "examples": []},
                                           {"name": "Two", "examples": []}]}}}}})"));
    f.reply(b);
    hui::ui::Feedback feedback;
    b.update({}, 1.0f / 60, f.clock, feedback);
    hui::gfx::DrawList list;
    b.draw(list);
    CHECK(b.config().help().size() == 2);
}
void test_house_sheet()
{
    FixtureHome h;
    Application a(h.config(), no_fonts());
    h.wire(a);
    h.settle(a);
    // L2, from any page; not from Setup, which has its own business.
    for (auto page : {Page::home, Page::room, Page::weather, Page::photos, Page::music})
    {
        a.open(page);
        h.press(a, Action::jump_prev);
        CHECK(a.sheet_showing());
        h.press(a, Action::jump_prev);
        CHECK(!a.sheet_showing());
    }
    a.open(Page::setup);
    h.press(a, Action::jump_prev);
    CHECK(!a.sheet_showing());
    a.open(Page::weather);
    h.press(a, Action::jump_prev);
    // It opens on Guests, the first mode: on is its script, and off asks twice.
    h.press(a, Action::confirm);
    auto calls = calls_sent(h);
    CHECK(calls.size() == 1 && sent_call(calls[0]) == "script.house_guests_on");
    h.reply(a);
    CHECK(a.toast_text() == "Guests on");
    set_state(h, a, "input_boolean.visitors", "on");
    h.press(a, Action::confirm);
    CHECK(calls_sent(h).empty());
    h.press(a, Action::confirm);
    calls = calls_sent(h);
    CHECK(calls.size() == 1 && sent_call(calls[0]) == "script.house_guests_off");
    h.reply(a);
    // Visitors mode: nothing else runs from here, Guests still can be ended.
    h.move(a, Direction::down);
    h.press(a, Action::confirm);
    CHECK(calls_sent(h).empty());
    CHECK(a.toast_text() == "Visitors mode is on, so nothing runs from here");
    set_state(h, a, "input_boolean.visitors", "off");
    // Cinema goes through its script both ways; Away is just its switch.
    h.press(a, Action::confirm);
    calls = calls_sent(h);
    CHECK(calls.size() == 1 && sent_call(calls[0]) == "script.cinema_mode_toggle");
    h.reply(a);
    h.move(a, Direction::down);
    h.press(a, Action::confirm);
    calls = calls_sent(h);
    CHECK(calls.size() == 1 && sent_call(calls[0]) == "input_boolean.toggle" &&
          text(field(calls[0], "target"), "entity_id") == "input_boolean.house_away");
    h.reply(a);
    // A mode going away from the settings' list of states drops out rather than failing.
    h.snapshot.states.apply(Json::parse(R"({"r": ["input_boolean.night_mode"]})"));
    h.reply(a);
    CHECK(house_rows(h.config().house, h.snapshot.states).size() == 4);
    h.press(a, Action::back);
    // The words on the clock's chip follow the modes.
    set_state(h, a, "input_boolean.cinema_mode", "on");
    CHECK(modes_words(h.config().house, h.snapshot.states) == "Cinema on");
    set_state(h, a, "input_boolean.house_away", "on");
    CHECK(modes_words(h.config().house, h.snapshot.states) == "Cinema and Away on");
    set_state(h, a, "input_boolean.cinema_mode", "off");
    set_state(h, a, "input_boolean.house_away", "off");
    // Everything off: up from the modes, two presses, the lamps noted, then the script, with Undo.
    h.press(a, Action::jump_prev);
    h.move(a, Direction::up);
    h.press(a, Action::confirm);
    CHECK(calls_sent(h).empty() && a.sheet_showing());
    h.press(a, Action::confirm);
    calls = calls_sent(h);
    CHECK(calls.size() == 1 && sent_call(calls[0]) == "scene.create");
    CHECK(text(field(calls[0], "service_data"), "scene_id") == kUndoSceneId);
    const auto lamps = strings(field(field(calls[0], "service_data"), "snapshot_entities"));
    CHECK(std::find(lamps.begin(), lamps.end(), "light.fireplace") != lamps.end());
    // A group isn't a lamp: its own lamps are what get noted.
    CHECK(std::find(lamps.begin(), lamps.end(), "light.all_house_lights") == lamps.end());
    CHECK(std::find(lamps.begin(), lamps.end(), "light.living_room") == lamps.end());
    CHECK(std::find(lamps.begin(), lamps.end(), "light.living_room_ceiling") != lamps.end());
    CHECK(!a.sheet_showing());
    h.reply(a);
    calls = calls_sent(h);
    CHECK(calls.size() == 1 && sent_call(calls[0]) == "script.house_everything_off");
    h.reply(a);
    CHECK(a.toast_text() == "Everything off");
    h.press(a, Action::north);
    calls = calls_sent(h);
    CHECK(calls.size() == 1 && sent_call(calls[0]) == "scene.turn_on");
    CHECK(text(field(calls[0], "target"), "entity_id") == std::string("scene.") + kUndoSceneId);
    h.reply(a);
    CHECK(a.toast_text() == "Lights back as they were");
    // Undo waits for the action: a script still running would turn the lamps off again. From Help
    // too, where Triangle otherwise types a question.
    a.open(Page::help);
    h.press(a, Action::jump_prev);
    h.move(a, Direction::up);
    h.press(a, Action::confirm);
    h.press(a, Action::confirm);
    h.reply(a);
    CHECK(a.toast_text() == "Everything off");
    h.press(a, Action::north);
    calls = calls_sent(h);
    CHECK(calls.size() == 1 && sent_call(calls[0]) == "script.house_everything_off");
    h.reply(a);
    h.press(a, Action::north);
    calls = calls_sent(h);
    CHECK(calls.size() == 1 && sent_call(calls[0]) == "scene.turn_on");
    h.reply(a);
    a.open(Page::home);
    // Good night sends its data, and a press with the sheet's arm gone starts again.
    h.press(a, Action::jump_prev);
    h.move(a, Direction::up);
    h.move(a, Direction::right);
    h.press(a, Action::confirm);
    h.clock.utc_ms += 6000;
    h.frame(a);
    h.press(a, Action::confirm);
    CHECK(calls_sent(h).empty());
    h.press(a, Action::back); // backs out of the arm
    CHECK(a.sheet_showing());
    h.press(a, Action::back);
    CHECK(!a.sheet_showing());
    // Snapshot refused: the action still goes, without an Undo it couldn't keep.
    FixtureHome f;
    Application b(f.config(), no_fonts());
    f.wire(b);
    f.settle(b);
    f.press(b, Action::jump_prev);
    f.move(b, Direction::up);
    f.press(b, Action::confirm);
    f.press(b, Action::confirm);
    CHECK(f.sent.size() == 1);
    b.send = [&f](Request r)
    {
        f.sent.push_back({++f.ticket, r.message});
        return f.ticket;
    };
    f.snapshot.results.push_back({f.sent[0].first, false, "Nope", {}});
    f.sent.clear();
    ++f.snapshot.revision;
    b.receive(f.snapshot);
    CHECK(f.sent.size() == 1 && sent_call(f.sent[0].second) == "script.house_everything_off");
    CHECK(b.toast_text() == "Everything off");
    f.frame(b);
    f.reply(b);
    f.press(b, Action::north); // no Undo to run: Triangle is the clock's Photos
    CHECK(b.page() == Page::photos);
    // A house with no modes or actions: L2 does nothing.
    auto none = f.config();
    none.house = {};
    Application c(none, no_fonts());
    FixtureHome g;
    g.wire(c);
    g.settle(c);
    g.press(c, Action::jump_prev);
    CHECK(!c.sheet_showing());
}
void test_speakers()
{
    FixtureHome h;
    const auto config = h.config();
    Application a(config, no_fonts());
    h.wire(a);
    h.settle(a);
    // Nobody together: the group is idle and the speakers play their own.
    CHECK(!group_active(config.speakers, h.snapshot.states));
    CHECK(!in_session(config.speakers, h.snapshot.states, "media_player.living_room_speaker"));
    CHECK(speakers_words(config, h.snapshot.states) == "Playing in 1 room");
    auto rows = speaker_rows(config, h.snapshot.states, "media_player.living_room_speaker", false,
                             parse_iso_ms(text(h.fixture, "now"), Zone{}), Zone{});
    // Where it plays, the kitchen's paused music, the sleep timer, then four volumes... three: no
    // group yet.
    CHECK(rows.size() == 6);
    CHECK(rows[0].id == "where" && rows[0].options[0].label == "Play everywhere" &&
          !rows[0].options[0].disabled);
    CHECK(rows[1].id == "move" && rows[1].heading == "Playing elsewhere" &&
          rows[1].label == "Kitchen \xC2\xB7 Morning Phase");
    CHECK(rows[1].options[0].label == "Move here");
    CHECK(rows[2].id == "sleep" && rows[2].selected == 0 && rows[2].options.size() == 4);
    CHECK(rows[2].label == "Sleep timer \xC2\xB7 Living room");
    // A running timer keeps the choice that started it, beside how long is left.
    {
        auto states = h.snapshot.states;
        states.apply(Json::parse(
            R"({"c": {"timer.music_sleep_living_room": {"+": {"s": "active", "a": {"duration": "0:30:00", "finishes_at": "2026-11-02T14:20:00+00:00"}}}}})"));
        const auto running = speaker_rows(config, states, "media_player.living_room_speaker", false,
                                          parse_iso_ms(text(h.fixture, "now"), Zone{}), Zone{});
        CHECK(running[2].selected == 2 && running[2].options[2].label == "30 min");
        CHECK(running[2].detail == "Stops in 15 min");
    }
    CHECK(rows[3].id == "volume" && rows[3].heading == "Volume" && rows[3].label == "Living room");
    CHECK(rows[3].detail == "Joins Everywhere" && rows[3].press_words == "Leave Everywhere");
    // Now Playing: Square.
    a.open(Page::music);
    h.press(a, Action::west);
    CHECK(a.sheet_showing());
    // Play everywhere: moves the queue, and says so only once it plays there.
    h.press(a, Action::confirm);
    auto calls = calls_sent(h);
    CHECK(calls.size() == 1 && sent_call(calls[0]) == "music_assistant.transfer_queue");
    CHECK(text(field(calls[0], "target"), "entity_id") == "media_player.home_speakers");
    CHECK(text(field(calls[0], "service_data"), "source_player") ==
          "media_player.living_room_speaker");
    h.reply(a);
    CHECK(a.toast_text().empty());
    h.press(a, Action::confirm); // already moving: nothing starts another
    CHECK(calls_sent(h).empty());
    h.snapshot.states.apply(Json::parse(
        R"({"c": {"media_player.home_speakers": {"+": {"s": "playing", "a": {"media_title": "Harvest Moon", "active_queue": "queue.group"}}},
                  "media_player.living_room_speaker": {"+": {"a": {"active_queue": "queue.group"}}}}})"));
    h.reply(a);
    h.frame(a);
    CHECK(a.toast_text() == "Playing in Everywhere");
    // Together: the group's the thing driven and shown, here and on the room page.
    CHECK(in_session(config.speakers, h.snapshot.states, "media_player.living_room_speaker"));
    CHECK(!in_session(config.speakers, h.snapshot.states, "media_player.kitchen_speaker"));
    CHECK(speakers_words(config, h.snapshot.states) == "Everywhere \xC2\xB7 1 room");
    rows = speaker_rows(config, h.snapshot.states, "media_player.living_room_speaker", false, 0,
                        Zone{});
    CHECK(rows[0].options[0].label == "Only Living room");
    const auto volumes =
        std::count_if(rows.begin(), rows.end(), [](const Control &r) { return r.id == "volume"; });
    CHECK(volumes == 4); // the group's own, above the rooms'
    h.press(a, Action::back);
    h.press(a, Action::confirm); // Pause, on the group
    calls = calls_sent(h);
    CHECK(calls.size() == 1 && sent_call(calls[0]) == "media_player.media_play_pause" &&
          text(field(calls[0], "target"), "entity_id") == "media_player.home_speakers");
    h.reply(a);
    h.move(a, Direction::up); // the volume, on the group
    h.clock.utc_ms += 400;
    h.frame(a);
    calls = calls_sent(h);
    CHECK(calls.size() == 1 && sent_call(calls[0]) == "media_player.volume_set" &&
          text(field(calls[0], "target"), "entity_id") == "media_player.home_speakers");
    h.reply(a);
    // The group stuck on idle while a member plays its queue still counts as playing.
    h.snapshot.states.apply(Json::parse(
        R"({"c": {"media_player.home_speakers": {"+": {"s": "idle"}},
                  "media_player.living_room_speaker": {"+": {"s": "playing"}}}})"));
    CHECK(group_active(config.speakers, h.snapshot.states));
    const auto session =
        music_session(config.speakers, h.snapshot.states, "media_player.living_room_speaker");
    CHECK(session.together && session.target == "media_player.home_speakers");
    CHECK(session.state.state == "playing" &&
          text(session.state.attributes, "media_title") == "Harvest Moon");
    CHECK(number(field(session.state.attributes, "volume_level")) == 0.3);
    // After "Only Kitchen" the group can sit paused while the kitchen plays its own.
    h.snapshot.states.apply(Json::parse(
        R"({"c": {"media_player.home_speakers": {"+": {"s": "paused"}},
                  "media_player.living_room_speaker": {"+": {"a": {"active_queue": "queue.living_room"}}}}})"));
    CHECK(!in_session(config.speakers, h.snapshot.states, "media_player.living_room_speaker"));
    // Join: a room that isn't playing the group's music may join it, never move it here.
    rows =
        speaker_rows(config, h.snapshot.states, "media_player.bedroom_speaker", false, 0, Zone{});
    const auto join =
        std::find_if(rows.begin(), rows.end(), [](const Control &r) { return r.id == "move"; });
    CHECK(join != rows.end() && join->options[0].label == "Join" &&
          join->label.starts_with("Everywhere"));
    CHECK(sent_call(join->options[0].press->message) == "media_player.join");
    // Why not: each with a reason, and the slot stays.
    h.snapshot.states.apply(
        Json::parse(R"({"c": {"media_player.home_speakers": {"+": {"s": "idle"}},
        "media_player.living_room_speaker": {"+": {"s": "idle"}},
        "media_player.kitchen_speaker": {"+": {"s": "idle"}}}})"));
    rows = speaker_rows(config, h.snapshot.states, "media_player.living_room_speaker", false, 0,
                        Zone{});
    CHECK(rows[0].options[0].disabled && rows[0].detail == "Start some music first");
    h.snapshot.states.apply(
        Json::parse(R"({"c": {"media_player.home_speakers": {"+": {"s": "unavailable"}}}})"));
    rows = speaker_rows(config, h.snapshot.states, "media_player.living_room_speaker", false, 0,
                        Zone{});
    CHECK(rows[0].options[0].disabled && rows[0].detail == "Everywhere is offline");
    h.snapshot.states.apply(Json::parse(
        R"({"c": {"media_player.home_speakers": {"+": {"s": "idle", "a": {"group_members": ["media_player.kitchen_speaker"]}}}}})"));
    rows = speaker_rows(config, h.snapshot.states, "media_player.living_room_speaker", false, 0,
                        Zone{});
    CHECK(rows[0].detail == "Living room isn't in Everywhere \xC2\xB7 add it below");
    const auto living = std::find_if(rows.begin(), rows.end(), [](const Control &r)
                                     { return r.id == "volume" && r.label == "Living room"; });
    CHECK(living != rows.end() && living->press_words == "Join Everywhere" &&
          living->detail.empty());
    // An offline speaker keeps its slot, and can't be moved.
    h.snapshot.states.apply(
        Json::parse(R"({"c": {"media_player.bedroom_speaker": {"+": {"s": "unavailable"}}}})"));
    rows = speaker_rows(config, h.snapshot.states, "media_player.living_room_speaker", false, 0,
                        Zone{});
    const auto gone = std::find_if(rows.begin(), rows.end(), [](const Control &r)
                                   { return r.id == "volume" && r.label == "Bedroom"; });
    CHECK(gone != rows.end() && gone->disabled && gone->disabled_words == "Offline" &&
          !gone->press);
    // Sleep: how long is left, and Off to stop it.
    const auto at = parse_iso_ms("2026-11-02T14:05:00Z", Zone{});
    h.snapshot.states.apply(Json::parse(
        R"({"c": {"timer.music_sleep_living_room": {"+": {"s": "active", "a": {"finishes_at": "2026-11-02T14:28:30Z"}}}}})"));
    rows = speaker_rows(config, h.snapshot.states, "media_player.living_room_speaker", false, at,
                        Zone{});
    const auto sleep =
        std::find_if(rows.begin(), rows.end(), [](const Control &r) { return r.id == "sleep"; });
    CHECK(sleep != rows.end() && sleep->detail == "Stops in 24 min" && sleep->selected == -1);
    CHECK(sent_call(sleep->options[0].press->message) == "script.music_sleep_timer");
    CHECK(field(sleep->options[0].press->message, "service_data") ==
          Json({{"player", "media_player.living_room_speaker"}, {"minutes", 0}}));
    CHECK(field(sleep->options[2].press->message, "service_data").value("minutes", 0) == 30);
    CHECK(sleep->options[2].done == "Music in Living room stops in 30 minutes");
    // A move that never starts says so.
    FixtureHome f;
    Application b(f.config(), no_fonts());
    f.wire(b);
    f.settle(b);
    b.open(Page::music);
    f.press(b, Action::west);
    f.press(b, Action::confirm);
    f.reply(b);
    f.clock.utc_ms += 9000;
    f.frame(b);
    CHECK(b.toast_text().starts_with("Music Assistant couldn't move the music"));
    // Only this room, with the whole group still playing: that isn't the move arriving.
    FixtureHome t;
    Application c(t.config(), no_fonts());
    t.wire(c);
    t.settle(c);
    const auto together = Json::parse(
        R"({"c": {"media_player.home_speakers": {"+": {"s": "playing", "a": {"media_title": "Harvest Moon", "active_queue": "queue.group"}}},
                  "media_player.living_room_speaker": {"+": {"a": {"active_queue": "queue.group"}}},
                  "media_player.kitchen_speaker": {"+": {"s": "playing", "a": {"active_queue": "queue.group"}}}}})");
    t.snapshot.states.apply(together);
    t.reply(c);
    // The room's speaker controls reach the group, as its Pause does.
    c.open(Page::room);
    t.move(c, Direction::left);
    t.press(c, Action::north);
    CHECK(c.sheet_showing());
    t.move(c, Direction::right);
    t.frame(c);
    auto volume = calls_sent(t);
    CHECK(volume.size() == 1 && sent_call(volume[0]) == "media_player.volume_set" &&
          text(field(volume[0], "target"), "entity_id") == "media_player.home_speakers");
    t.reply(c);
    t.press(c, Action::back);
    c.open(Page::music);
    t.press(c, Action::west);
    t.press(c, Action::confirm);
    t.reply(c);
    t.frame(c);
    CHECK(c.toast_text().empty());
    t.clock.utc_ms += 9000;
    t.frame(c);
    CHECK(c.toast_text().starts_with("Music Assistant couldn't move the music"));
    t.clock.utc_ms += 9000;
    t.frame(c);
    t.press(c, Action::confirm);
    t.reply(c);
    t.snapshot.states.apply(Json::parse(
        R"({"c": {"media_player.home_speakers": {"+": {"s": "idle"}},
                  "media_player.kitchen_speaker": {"+": {"s": "idle"}},
                  "media_player.living_room_speaker": {"+": {"s": "playing", "a": {"active_queue": "queue.own"}}}}})"));
    t.reply(c);
    t.frame(c);
    CHECK(!c.toast_text().empty() && !c.toast_text().starts_with("Music Assistant"));
    // From a speaker's own sheet: the way to the others.
    f.press(b, Action::back);
    f.press(b, Action::north);
    CHECK(b.sheet_showing());
    f.move(b, Direction::up);
    f.press(b, Action::confirm);
    CHECK(calls_sent(f).empty() && b.sheet_showing());
    f.press(b, Action::west); // closes Speakers, so that was the Speakers sheet
    CHECK(!b.sheet_showing());
    // No settings for them: the rooms' own speakers still get a sheet, with nothing to move to.
    auto plain = f.config();
    plain.speakers = {};
    const auto list = speaker_list(plain);
    CHECK(list.size() == 2 && list[0].player == "media_player.living_room_speaker" &&
          list[0].name == "Living room");
    const auto simple = speaker_rows(plain, f.snapshot.states, "media_player.living_room_speaker",
                                     false, 0, Zone{});
    CHECK(std::none_of(simple.begin(), simple.end(),
                       [](const Control &r) { return r.id == "where" || r.id == "sleep"; }));
    CHECK(std::all_of(simple.begin(), simple.end(), [](const Control &r) { return !r.press; }));
}
} // namespace

int main(int argc, char **argv)
{
    if (argc == 3 && std::string(argv[1]) == "--integration")
        return integration(std::atoi(argv[2]), false);
    if (argc == 3 && std::string(argv[1]) == "--auth-rejected")
        return integration(std::atoi(argv[2]), true);
    FixtureHome home;
    CHECK(home.fixture.is_object());
    test_clock();
    test_models(home.fixture);
    test_app(home.fixture);
    test_features();
    test_controls();
    test_sheets();
    test_pictures();
    test_photos();
    test_night_and_counts();
    test_scene_row();
    test_history();
    test_allowed_calls();
    test_settings_round_trip();
    test_scenes_in_sheets();
    test_charts_and_boosts();
    test_cover_retry();
    test_settings_arriving_over_help();
    test_house_sheet();
    test_speakers();
    test_lamp_groups();
    test_speakers_focus();
    std::printf("%d checks passed\n", checks);
    return 0;
}

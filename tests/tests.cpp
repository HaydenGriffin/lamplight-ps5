// Lamplight for PS5 - Host tests: the models, the app driven by the fixture, and the client.
// Copyright (C) 2026 Hayden Griffin
// SPDX-License-Identifier: GPL-3.0-or-later
#include "../host/fixture_home.hpp"
#include "lamplight/assistant.hpp"
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
        CHECK(merged.room_page.name == "Den" && merged.room_page.tiles.size() == 1);
        std::filesystem::remove(dir / "lamplight.conf");
        merged = load_config(dir.string(), (dir / "lamplight.conf").string(), error, bootstrapped);
        CHECK(!bootstrapped && merged.room_page.empty());
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
    CHECK(!allowed(service("light", "toggle", {}, "light.kitchen").message, config));
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
    CHECK(app.page() == Page::room); // a room page comes first
    home.press(app, Action::page_next);
    CHECK(app.page() == Page::weather);
    home.press(app, Action::page_prev);
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
        for (int i = 0; i < 5; ++i)
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
        h.press(a, Action::confirm); // Play or Pause, first
        CHECK(h.sent.size() == 1 && sent_call(h.sent[0].second) == "media_player.media_play_pause");
        h.reply(a);
        h.move(a, Direction::right); // Next
        h.move(a, Direction::right); // Movie time, below and to the right
        h.move(a, Direction::right); // the radiator tile, which only reports
        h.press(a, Action::confirm);
        CHECK(h.sent.empty());
        h.move(a, Direction::up); // Main lights, above it
        h.press(a, Action::confirm);
        CHECK(h.sent.size() == 1 && sent_call(h.sent[0].second) == "light.toggle");
        CHECK(text(field(h.sent[0].second, "target"), "entity_id") == "light.living_room");
        h.reply(a);
        // Down from the hero's buttons reaches the actions; Good night is the fourth.
        a.open(Page::room);
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
    s.watch = [&](std::set<std::string> w) { watched = std::move(w); };
    bool saved = false;
    s.save = [&](const Config &) { saved = true; };
    setup.settle(s);
    s.open(Page::setup);
    for (int i = 0; i < 5; ++i)
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
    // Watching another entity resubscribes without dropping the session.
    client.watch({"weather.home", "timer.washing_machine"});
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
    std::printf("%d checks passed\n", checks);
    return 0;
}

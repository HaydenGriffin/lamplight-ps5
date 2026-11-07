// Lamplight for PS5 - Offscreen pictures of the real screens, driven by tests/fixture.json.
// Copyright (C) 2026 Hayden Griffin
// SPDX-License-Identifier: GPL-3.0-or-later
#include "fixture_home.hpp"
#include "offscreen.hpp"
#include <cstdlib>
#include <filesystem>
#include <functional>
#define STB_IMAGE_WRITE_IMPLEMENTATION
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wmissing-field-initializers"
#include "../third_party/stb/stb_image_write.h"
#pragma clang diagnostic pop

using namespace lamplight;
int main(int argc, char **argv)
{
    Offscreen screen;
    if (const int failed = screen.init())
        return failed;
    const std::string out = argc > 1 ? argv[1] : "build/preview/";
    std::filesystem::create_directories(out);
    auto shoot = [&](Application &app, const std::string &name)
    {
        const auto &pixels = screen.capture(app);
        stbi_flip_vertically_on_write(1);
        const bool ok =
            stbi_write_png((out + name + ".png").c_str(), 1920, 1080, 4, pixels.data(), 1920 * 4);
        std::printf("%s%s.png %s gl=0x%x\n", out.c_str(), name.c_str(), ok ? "written" : "FAILED",
                    glGetError());
        return ok;
    };
    using hui::Action;
    using hui::Direction;
    auto deliver = [&](Application &app) { screen.deliver(app); };
    auto run_frames = [&](FixtureHome &h, Application &app, int count, float dt)
    {
        for (int i = 0; i < count; ++i)
        {
            h.reply(app);
            h.frame(app, {}, dt);
            deliver(app);
        }
    };
    // A jump in the clock sets off the timers it passes, one alert after another.
    auto dismiss_alerts = [](FixtureHome &h, Application &app)
    {
        while (app.alerting())
            h.press(app, Action::back);
    };
    auto to_main_lights = [](FixtureHome &, Application &app) { app.open(Page::room); };
    // One scene per picture: a fresh app, the fixture's house, then whatever the scene does.
    struct Scene
    {
        const char *name;
        std::function<void(FixtureHome &, Application &)> run;
        std::uint32_t held = 0; // buttons still held while the scene settles
    };
    const std::vector<Scene> scenes = {
        {"home", [](FixtureHome &, Application &) {}},
        {"weather", [](FixtureHome &, Application &app) { app.open(Page::weather); }},
        {"room", [](FixtureHome &, Application &app) { app.open(Page::room); }},
        {"room-kitchen",
         [](FixtureHome &h, Application &app)
         {
             app.open(Page::room);
             h.press(app, Action::west);
         }},
        {"music", [](FixtureHome &, Application &app) { app.open(Page::music); }},
        {"camera-tile",
         [](FixtureHome &h, Application &app)
         {
             app.open(Page::room);
             h.press(app, Action::west);
             h.move(app, Direction::down);
             h.press(app, Action::confirm);
         }},
        {"room-hold",
         [&](FixtureHome &h, Application &app)
         {
             to_main_lights(h, app);
             hui::InputFrame down;
             down.pressed = down.held = hui::action_bit(Action::confirm);
             h.frame(app, down);
             hui::InputFrame held;
             held.held = down.held;
             h.frame(app, held, 0.25f);
         },
         hui::action_bit(Action::confirm)},
        {"room-adjust",
         [&](FixtureHome &h, Application &app)
         {
             to_main_lights(h, app);
             hui::InputFrame stick;
             stick.stick2_y = -1;
             h.frame(app, stick, 0.15f);
         }},
        {"sheet-light",
         [&](FixtureHome &h, Application &app)
         {
             to_main_lights(h, app);
             h.press(app, Action::north);
         }},
        {"sheet-light-colour",
         [&](FixtureHome &h, Application &app)
         {
             to_main_lights(h, app);
             h.press(app, Action::north);
             for (int i = 0; i < 2; ++i)
                 h.move(app, Direction::down);
             h.move(app, Direction::right);
         }},
        {"sheet-light-lamps",
         [&](FixtureHome &h, Application &app)
         {
             to_main_lights(h, app);
             h.press(app, Action::north);
             for (int i = 0; i < 5; ++i)
                 h.move(app, Direction::down);
         }},
        {"sheet-light-scene",
         [&](FixtureHome &h, Application &app)
         {
             to_main_lights(h, app);
             h.press(app, Action::north);
             h.move(app, Direction::up);
         }},
        {"sheet-climate",
         [&](FixtureHome &h, Application &app)
         {
             app.open(Page::room);
             h.move(app, Direction::down);
             h.press(app, Action::confirm);
             h.reply(app);
         }},
        {"sheet-climate-boost",
         [&](FixtureHome &h, Application &app)
         {
             app.open(Page::room);
             h.move(app, Direction::down);
             h.press(app, Action::confirm);
             h.reply(app);
             h.move(app, Direction::up);
         }},
        {"sheet-climate-loading",
         [&](FixtureHome &h, Application &app)
         {
             app.open(Page::room);
             h.move(app, Direction::down);
             h.press(app, Action::confirm);
         }},
        {"sheet-sensor",
         [&](FixtureHome &h, Application &app)
         {
             app.open(Page::room);
             h.press(app, Action::west);
             h.move(app, Direction::right);
             h.move(app, Direction::right);
             h.press(app, Action::confirm);
             h.reply(app);
         }},
        {"sheet-house",
         [&](FixtureHome &h, Application &app)
         {
             app.open(Page::weather);
             h.press(app, Action::jump_prev);
         }},
        {"sheet-house-on",
         [&](FixtureHome &h, Application &app)
         {
             app.open(Page::room);
             h.snapshot.states.apply(Json::parse(
                 R"({"c": {"input_boolean.cinema_mode": {"+": {"s": "on"}}, "input_boolean.night_mode": {"+": {"s": "on"}}}})"));
             h.reply(app);
             h.press(app, Action::jump_prev);
             h.move(app, Direction::down);
         }},
        {"sheet-house-armed",
         [&](FixtureHome &h, Application &app)
         {
             h.press(app, Action::jump_prev);
             h.move(app, Direction::up);
             h.press(app, Action::confirm);
         }},
        {"house-undo",
         [&](FixtureHome &h, Application &app)
         {
             app.open(Page::house);
             h.press(app, Action::jump_prev);
             h.move(app, Direction::up);
             h.press(app, Action::confirm);
             h.press(app, Action::confirm);
             h.reply(app);
             h.reply(app);
         }},
        {"home-modes",
         [&](FixtureHome &h, Application &app)
         {
             h.snapshot.states.apply(
                 Json::parse(R"({"c": {"input_boolean.cinema_mode": {"+": {"s": "on"}}}})"));
             h.reply(app);
             h.move(app, Direction::right);
             h.move(app, Direction::right);
         }},
        {"sheet-speakers",
         [&](FixtureHome &h, Application &app)
         {
             app.open(Page::music);
             h.press(app, Action::west);
         }},
        {"sheet-speakers-volume",
         [&](FixtureHome &h, Application &app)
         {
             h.snapshot.states.apply(Json::parse(
                 R"({"c": {"timer.music_sleep_living_room": {"+": {"s": "active", "a": {"finishes_at": "2026-11-02T14:28:00+00:00"}}}}})"));
             h.reply(app);
             app.open(Page::music);
             h.press(app, Action::west);
             for (int i = 0; i < 4; ++i)
                 h.move(app, Direction::down);
         }},
        {"sheet-speakers-together",
         [&](FixtureHome &h, Application &app)
         {
             h.snapshot.states.apply(Json::parse(
                 R"({"c": {"media_player.home_speakers": {"+": {"s": "playing", "a": {"media_title": "Harvest Moon", "media_artist": "Neil Young"}}},
                           "media_player.living_room_speaker": {"+": {"a": {"active_queue": "queue.group"}}},
                           "media_player.kitchen_speaker": {"+": {"s": "playing", "a": {"active_queue": "queue.group"}}}}})"));
             h.reply(app);
             app.open(Page::music);
             h.press(app, Action::west);
         }},
        {"music-together",
         [&](FixtureHome &h, Application &app)
         {
             h.snapshot.states.apply(Json::parse(
                 R"({"c": {"media_player.home_speakers": {"+": {"s": "playing", "a": {"media_title": "Harvest Moon", "media_artist": "Neil Young"}}},
                           "media_player.living_room_speaker": {"+": {"a": {"active_queue": "queue.group"}}},
                           "media_player.kitchen_speaker": {"+": {"s": "playing", "a": {"active_queue": "queue.group"}}}}})"));
             h.reply(app);
             app.open(Page::music);
             run_frames(h, app, 6, 1.0f / 60);
         }},
        {"sheet-cover",
         [&](FixtureHome &h, Application &app)
         {
             app.open(Page::room);
             h.move(app, Direction::down);
             h.move(app, Direction::right);
             h.press(app, Action::north);
         }},
        {"sheet-all-lamps",
         [&](FixtureHome &h, Application &app)
         {
             app.open(Page::room);
             h.move(app, Direction::left);
             h.move(app, Direction::down);
             h.press(app, Action::north);
         }},
        {"sheet-music",
         [](FixtureHome &h, Application &app)
         {
             app.open(Page::music);
             h.press(app, Action::north);
         }},
        {"slideshow",
         [&](FixtureHome &h, Application &app)
         {
             app.open(Page::photos);
             run_frames(h, app, 6, 1.0f / 60);
         }},
        {"slideshow-portrait",
         [&](FixtureHome &h, Application &app)
         {
             h.fixture["media"]["resolve"]["url"] = "/media/local/photos/portrait.jpg?authSig=x";
             app.open(Page::photos);
             run_frames(h, app, 6, 1.0f / 60);
         }},
        {"slideshow-bright",
         [&](FixtureHome &h, Application &app)
         {
             h.fixture["media"]["resolve"]["url"] = "/media/local/photos/snow.jpg?authSig=x";
             app.open(Page::photos);
             run_frames(h, app, 6, 1.0f / 60);
         }},
        {"slideshow-night",
         [&](FixtureHome &h, Application &app)
         {
             h.fixture["media"]["resolve"]["url"] = "/media/local/photos/snow.jpg?authSig=x";
             h.clock.utc_ms = parse_iso_ms("2026-11-02T23:12:00Z", Zone{});
             h.snapshot.states.apply({{"c", {{"sun.sun", {{"+", {{"s", "below_horizon"}}}}}}}});
             h.reply(app);
             h.frame(app);
             dismiss_alerts(h, app);
             app.open(Page::photos);
             run_frames(h, app, 6, 1.0f / 60);
         }},
        {"music-bright",
         [&](FixtureHome &h, Application &app)
         {
             h.snapshot.states.apply(Json::parse(R"({"c": {"media_player.living_room_speaker":
                 {"+": {"a": {"entity_picture": "/local/snow.jpg"}}}}})"));
             h.reply(app);
             app.open(Page::music);
             run_frames(h, app, 6, 1.0f / 60);
         }},
        {"room-background-bright",
         [&](FixtureHome &h, Application &app)
         {
             h.fixture["media"]["resolve"]["url"] = "/media/local/photos/snow.jpg?authSig=x";
             h.snapshot.states.apply(Json::parse(R"({"a": {"sensor.lamplight_ps5": {"s": "ready",
                 "a": {"lamplight": {"background": "photos"}}}}})"));
             run_frames(h, app, 6, 1.0f / 60);
             app.open(Page::room);
         }},
        {"home-background-bright",
         [&](FixtureHome &h, Application &app)
         {
             h.fixture["media"]["resolve"]["url"] = "/media/local/photos/snow.jpg?authSig=x";
             h.snapshot.states.apply(Json::parse(R"({"a": {"sensor.lamplight_ps5": {"s": "ready",
                 "a": {"lamplight": {"background": "photos"}}}}})"));
             run_frames(h, app, 6, 1.0f / 60);
         }},
        {"home-photo-bright",
         [&](FixtureHome &h, Application &app)
         {
             h.fixture["media"]["resolve"]["url"] = "/media/local/photos/snow.jpg?authSig=x";
             run_frames(h, app, 200, 1.0f);
         }},
        {"home-background",
         [&](FixtureHome &h, Application &app)
         {
             h.snapshot.states.apply(Json::parse(R"({"a": {"sensor.lamplight_ps5": {"s": "ready",
                 "a": {"lamplight": {"background": "photos"}}}}})"));
             run_frames(h, app, 6, 1.0f / 60);
         }},
        {"room-background",
         [&](FixtureHome &h, Application &app)
         {
             h.snapshot.states.apply(Json::parse(R"({"a": {"sensor.lamplight_ps5": {"s": "ready",
                 "a": {"lamplight": {"background": "photos"}}}}})"));
             run_frames(h, app, 6, 1.0f / 60);
             app.open(Page::room);
         }},
        {"setup-photo-folder",
         [&](FixtureHome &h, Application &app)
         {
             app.open(Page::setup);
             for (int i = 0; i < 6; ++i)
                 h.move(app, Direction::down);
             h.press(app, Action::confirm);
             h.reply(app);
         }},
        {"music-volume",
         [](FixtureHome &h, Application &app)
         {
             app.open(Page::music);
             h.move(app, Direction::up);
             h.move(app, Direction::up);
         }},
        {"doorbell",
         [](FixtureHome &h, Application &app)
         {
             h.snapshot.states.apply(
                 Json::parse(R"({"c": {"binary_sensor.front_door_ding": {"+": {"s": "on"}}}})"));
             h.reply(app);
         }},
        {"home-photo", [&](FixtureHome &h, Application &app) { run_frames(h, app, 200, 1.0f); }},
        {"house", [](FixtureHome &, Application &app) { app.open(Page::house); }},
        {"home-mood-night",
         [&](FixtureHome &h, Application &app)
         {
             // Without photos: the sky's own mood behind the resting clock.
             h.snapshot.states.apply(Json::parse(R"({"a": {"sensor.lamplight_ps5": {"s": "ready",
                 "a": {"lamplight": {"photos": ""}}}}})"));
             h.clock.utc_ms = parse_iso_ms("2026-11-02T21:40:00Z", Zone{});
             h.snapshot.states.apply(Json::parse(
                 R"({"c": {"sun.sun": {"+": {"s": "below_horizon"}}, "weather.home": {"+": {"s": "clear-night"}}}})"));
             h.reply(app);
             h.frame(app);
             dismiss_alerts(h, app);
             run_frames(h, app, 200, 1.0f);
         }},
        {"home-mood-rain",
         [&](FixtureHome &h, Application &app)
         {
             h.snapshot.states.apply(Json::parse(R"({"a": {"sensor.lamplight_ps5": {"s": "ready",
                 "a": {"lamplight": {"photos": ""}}}}})"));
             h.snapshot.states.apply(
                 Json::parse(R"({"c": {"weather.home": {"+": {"s": "rainy"}}}})"));
             run_frames(h, app, 200, 1.0f);
         }},
        {"home-12h",
         [](FixtureHome &h, Application &app)
         {
             h.snapshot.country = "US";
             h.reply(app);
         }},
        {"talk",
         [](FixtureHome &h, Application &app)
         {
             app.listen = [](const std::string &, const std::string &) { return 1; };
             app.open_microphone = [] { return true; };
             app.close_microphone = [] {};
             app.microphone_level = [] { return 0.18f; };
             hui::InputFrame hold;
             hold.pressed = hold.held = hui::action_bit(Action::jump_next);
             h.frame(app, hold);
             h.snapshot.assist = {1, Assist::Stage::listening, "", "", "", ""};
             h.reply(app);
         },
         hui::action_bit(Action::jump_next)},
        {"talk-thinking",
         [](FixtureHome &h, Application &app)
         {
             app.listen = [](const std::string &, const std::string &) { return 1; };
             app.open_microphone = [] { return true; };
             app.close_microphone = [] {};
             app.microphone_level = [] { return 0.18f; };
             hui::InputFrame hold;
             hold.pressed = hold.held = hui::action_bit(Action::jump_next);
             h.frame(app, hold);
             h.frame(app);
             h.snapshot.assist = {1, Assist::Stage::thinking, "turn on the fireplace", "", "", ""};
             h.reply(app);
         }},
        {"room-armed",
         [](FixtureHome &h, Application &app)
         {
             app.open(Page::room);
             h.move(app, Direction::left);
             h.move(app, Direction::down);
             h.move(app, Direction::down);
             h.move(app, Direction::right);
             h.press(app, Action::confirm);
         }},
        {"alert",
         [](FixtureHome &h, Application &app)
         {
             h.clock.utc_ms += 12 * 60000 + 30000;
             h.frame(app);
         }},
        {"home-ambient",
         [](FixtureHome &h, Application &app)
         {
             for (int i = 0; i < 200; ++i)
                 h.frame(app, {}, 1.0f);
         }},
        {"home-night",
         [&](FixtureHome &h, Application &app)
         {
             h.clock.utc_ms = parse_iso_ms("2026-11-02T23:12:00Z", Zone{});
             h.snapshot.states.apply({{"c", {{"sun.sun", {{"+", {{"s", "below_horizon"}}}}}}}});
             h.reply(app);
             h.frame(app);
             dismiss_alerts(h, app);
             for (int i = 0; i < 200; ++i)
                 h.frame(app, {}, 1.0f);
         }},
        {"agenda", [](FixtureHome &, Application &app) { app.open(Page::agenda); }},
        {"timers", [](FixtureHome &, Application &app) { app.open(Page::timers); }},
        {"timers-undo",
         [](FixtureHome &h, Application &app)
         {
             app.open(Page::timers);
             h.move(app, Direction::left);
             h.press(app, Action::confirm);
         }},
        {"timers-armed",
         [](FixtureHome &h, Application &app)
         {
             app.open(Page::timers);
             h.move(app, Direction::left);
             h.move(app, Direction::down);
             h.move(app, Direction::down);
             h.press(app, Action::confirm);
         }},
        {"wifi", [](FixtureHome &, Application &app) { app.open(Page::wifi); }},
        {"help", [](FixtureHome &, Application &app) { app.open(Page::help); }},
        {"help-reply",
         [](FixtureHome &h, Application &app)
         {
             app.open(Page::help);
             h.move(app, Direction::right);
             h.press(app, Action::confirm);
             h.reply(app);
             h.frame(app);
         }},
        {"info", [](FixtureHome &, Application &app) { app.open(Page::info); }},
        {"setup", [](FixtureHome &, Application &app) { app.open(Page::setup); }},
        {"setup-calendars",
         [](FixtureHome &h, Application &app)
         {
             app.open(Page::setup);
             for (int i = 0; i < 9; ++i)
                 h.move(app, Direction::down);
             h.press(app, Action::confirm);
         }},
        {"quit", [](FixtureHome &, Application &app) { app.open(Page::quit); }},
        {"weather-failed", [](FixtureHome &, Application &app) { app.open(Page::weather); }},
        {"weather-offline",
         [](FixtureHome &h, Application &app)
         {
             h.snapshot.ready = h.snapshot.connected = false;
             h.snapshot.states.clear();
             h.snapshot.status = "Connection lost. Retrying automatically.";
             h.reply(app);
             app.open(Page::weather);
         }},
        {"offline",
         [](FixtureHome &h, Application &app)
         {
             h.snapshot.ready = h.snapshot.connected = false;
             h.snapshot.states.clear();
             h.snapshot.status = "Connection lost. Retrying automatically.";
             h.reply(app);
         }},
    };
    for (const auto &scene : scenes)
    {
        FixtureHome home;
        home.fail_forecast = std::string(scene.name) == "weather-failed";
        Application app(home.config(), screen.fonts);
        home.wire(app);
        app.request_picture = [&](const std::string &url) { screen.ask(url); };
        home.settle(app);
        scene.run(home, app);
        // Half a second of frames, so springs and lists settle as they would on screen.
        for (int i = 0; i < 30; ++i)
        {
            hui::InputFrame input;
            input.held = scene.held;
            home.frame(app, input, scene.held ? 1.0f / 600 : 1.0f / 60);
            deliver(app);
        }
        if (!shoot(app, scene.name))
            return 5;
    }
    return 0;
}

// Lamplight for PS5 - A tour for the README and the site: the app's own code and drawing, driven
// by button presses against the made-up house in tests/fixture.json. Raw 1920x1080 RGBA frames,
// bottom row first, go to stdout (tools/tour-video.sh makes the videos).
// Copyright (C) 2026 Hayden Griffin
// SPDX-License-Identifier: GPL-3.0-or-later
#include "fixture_home.hpp"
#include "offscreen.hpp"
#include <cstdio>
#include <functional>

using namespace lamplight;
using hui::Action;
using hui::Direction;

namespace
{
constexpr int kFps = 30;

class Tour
{
  public:
    Tour(FixtureHome &home, Application &app, Offscreen &screen)
        : home_(home), app_(app), screen_(screen)
    {
    }
    void wait(float seconds)
    {
        for (int i = 0; i < frames(seconds); ++i)
            step({});
    }
    void press(Action action, float then = 0.9f)
    {
        hui::InputFrame down;
        down.pressed = down.held = hui::action_bit(action);
        step(down);
        wait(then);
    }
    void move(Direction direction, float then = 0.5f)
    {
        hui::InputFrame in;
        in.nav = direction;
        step(in);
        wait(then);
    }
    void right_stick(float y, float seconds)
    {
        hui::InputFrame in;
        in.stick2_y = y;
        for (int i = 0; i < frames(seconds); ++i)
            step(in);
    }
    // R2 held down while `during` plays, then let go.
    void hold_to_talk(const std::function<void(const hui::InputFrame &)> &during)
    {
        hui::InputFrame held;
        held.pressed = held.held = hui::action_bit(Action::jump_next);
        step(held);
        held.pressed = 0;
        during(held);
        hui::InputFrame up;
        up.released = hui::action_bit(Action::jump_next);
        step(up);
    }
    void held_for(const hui::InputFrame &held, float seconds)
    {
        for (int i = 0; i < frames(seconds); ++i)
            step(held);
    }
    void fade_out(float seconds)
    {
        fade(seconds, 1.0f, 0.0f);
    }
    void fade_in(float seconds)
    {
        fade(seconds, 0.0f, 1.0f);
    }

  private:
    static int frames(float seconds)
    {
        return static_cast<int>(seconds * kFps);
    }
    void fade(float seconds, float from, float to)
    {
        const int count = frames(seconds);
        for (int i = 0; i < count; ++i)
        {
            shade_ = from + (to - from) * static_cast<float>(i + 1) / static_cast<float>(count);
            step({});
        }
        shade_ = 1;
    }
    void step(const hui::InputFrame &input)
    {
        house_obeys();
        home_.reply(app_);
        home_.clock.utc_ms += 1000 / kFps;
        home_.frame(app_, input, 1.0f / kFps);
        screen_.deliver(app_);
        write(screen_.capture(app_));
    }
    void write(const std::vector<unsigned char> &pixels)
    {
        if (shade_ >= 1)
        {
            std::fwrite(pixels.data(), 1, pixels.size(), stdout);
            return;
        }
        shaded_ = pixels;
        for (std::size_t i = 0; i < shaded_.size(); ++i)
            if (i % 4 != 3)
                shaded_[i] = static_cast<unsigned char>(static_cast<float>(shaded_[i]) * shade_);
        std::fwrite(shaded_.data(), 1, shaded_.size(), stdout);
    }
    // The made-up house does as it's told, so a slider's lamp and its tile follow.
    void house_obeys()
    {
        for (const auto &[ticket, message] : home_.sent)
        {
            const auto id = text(field(message, "target"), "entity_id");
            const auto domain = text(message, "domain");
            const auto service = text(message, "service");
            if (id.empty() || (domain != "light" && domain != "homeassistant"))
                continue;
            Json attributes = Json::object();
            const auto &data = field(message, "service_data");
            if (data.contains("brightness_pct"))
                attributes["brightness"] = data["brightness_pct"].get<int>() * 255 / 100;
            if (data.contains("brightness"))
                attributes["brightness"] = data["brightness"];
            if (data.contains("color_temp_kelvin"))
                attributes["color_temp_kelvin"] = data["color_temp_kelvin"];
            if (data.contains("rgb_color"))
                attributes["rgb_color"] = data["rgb_color"];
            const char *state = service == "turn_off" ? "off" : "on";
            if (service != "turn_on" && service != "turn_off")
                continue;
            Json change;
            change["c"][id]["+"] = {{"s", state}, {"a", attributes}};
            home_.snapshot.states.apply(change);
        }
    }
    FixtureHome &home_;
    Application &app_;
    Offscreen &screen_;
    float shade_ = 1;
    std::vector<unsigned char> shaded_;
};

void set_states(FixtureHome &home, const char *json)
{
    home.snapshot.states.apply(Json::parse(json));
}
// The resting clock under the night sky: hours later, in a blink.
void to_the_night(FixtureHome &home, Application &app)
{
    set_states(
        home,
        R"({"a": {"sensor.lamplight_ps5": {"s": "ready", "a": {"lamplight": {"photos": ""}}}}})");
    home.clock.utc_ms = parse_iso_ms("2026-11-02T21:40:00Z", Zone{});
    set_states(
        home,
        R"({"c": {"sun.sun": {"+": {"s": "below_horizon"}}, "weather.home": {"+": {"s": "clear-night"}}}})");
    home.reply(app);
    home.frame(app);
    while (app.alerting())
        home.press(app, Action::back);
    for (int i = 0; i < 200; ++i)
    {
        home.reply(app);
        home.frame(app, {}, 1.0f);
    }
}
void play(Tour &tour, FixtureHome &home, Application &app)
{
    // The clock; Cross opens the Living room, and the right stick sets a light from the page.
    tour.fade_in(0.4f);
    tour.wait(1.8f);
    tour.press(Action::confirm, 0.9f);
    tour.right_stick(-1, 0.9f);
    tour.wait(0.5f);
    // Triangle: the light's own controls.
    tour.press(Action::north, 0.8f);
    for (int i = 0; i < 3; ++i)
        tour.move(Direction::left, 0.22f);
    tour.move(Direction::down, 0.4f);
    for (int i = 0; i < 3; ++i)
        tour.move(Direction::right, 0.22f);
    tour.move(Direction::up, 0.25f);
    tour.move(Direction::up, 0.4f);
    tour.move(Direction::left, 0.35f);
    tour.press(Action::confirm, 1.0f);
    tour.press(Action::back, 0.5f);
    // The radiator's day, and a warm-up.
    tour.move(Direction::down, 0.35f);
    tour.press(Action::confirm, 2.0f);
    tour.press(Action::back, 0.4f);
    // Music, and every room's speaker.
    tour.press(Action::page_next, 1.5f);
    tour.press(Action::west, 1.8f);
    tour.press(Action::back, 0.3f);
    // The other pages, a look at each.
    for (const float look : {1.3f, 1.5f, 1.2f, 1.2f})
        tour.press(Action::page_next, look);
    // House controls, from any page.
    tour.press(Action::jump_prev, 1.8f);
    tour.press(Action::back, 0.3f);
    tour.press(Action::back, 0.7f);
    // The doorbell takes the screen.
    set_states(home, R"({"c": {"binary_sensor.front_door_ding": {"+": {"s": "on"}}}})");
    tour.wait(2.2f);
    set_states(home, R"({"c": {"binary_sensor.front_door_ding": {"+": {"s": "off"}}}})");
    tour.press(Action::back, 0.6f);
    // Triangle on the clock: the slideshow, fading to the next photo.
    tour.press(Action::north, 2.6f);
    home.fixture["media"]["resolve"]["url"] = "/media/local/photos/portrait.jpg?authSig=x";
    tour.move(Direction::right, 3.0f);
    tour.press(Action::back, 0.6f);
    // Hold R2 and speak.
    app.listen = [](const std::string &, const std::string &) { return 1; };
    app.open_microphone = [] { return true; };
    app.close_microphone = [] {};
    app.microphone_level = [] { return 0.18f; };
    tour.hold_to_talk(
        [&](const hui::InputFrame &held)
        {
            home.snapshot.assist = {1, Assist::Stage::listening, "", "", "", ""};
            tour.held_for(held, 1.2f);
            home.snapshot.assist = {1, Assist::Stage::thinking, "turn on the floor lamp", "", "",
                                    ""};
            tour.held_for(held, 0.2f);
        });
    tour.wait(0.8f);
    home.snapshot.assist = {
        1, Assist::Stage::done, "turn on the floor lamp", "Turned on the floor lamp.", "", ""};
    set_states(home, R"({"c": {"light.living_room_lamp": {"+": {"s": "on"}}}})");
    tour.wait(2.4f);
    tour.fade_out(0.5f);
    to_the_night(home, app);
    tour.fade_in(0.8f);
    tour.wait(2.8f);
    tour.fade_out(0.6f);
}
} // namespace

int main()
{
    Offscreen screen;
    if (const int failed = screen.init())
        return failed;
    FixtureHome home;
    Application app(home.config(), screen.fonts);
    home.wire(app);
    app.request_picture = [&](const std::string &url) { screen.ask(url); };
    home.settle(app);
    Tour tour(home, app, screen);
    play(tour, home, app);
    return 0;
}

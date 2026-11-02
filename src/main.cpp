// Lamplight for PS5 - Native entry point: the frame loop, and the workers' results into the app.
// Copyright (C) 2026 Hayden Griffin
// SPDX-License-Identifier: GPL-3.0-or-later
#include "core/save_file.hpp"
#include "gfx/renderer.hpp"
#include "lamplight/application.hpp"
#include "lamplight/resources.hpp"
#include "platform/ps5/display_egl.hpp"
#include "platform/ps5/pad.hpp"
#include "platform/ps5/system.hpp"
#include <algorithm>
#include <array>
#include <cmath>

namespace
{
GLuint upload(const lamplight::Picture &picture)
{
    GLuint texture = 0;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, picture.width, picture.height, 0, GL_RGBA,
                 GL_UNSIGNED_BYTE, picture.rgba.data());
    return texture;
}
} // namespace

int main()
{
    using namespace hui;
    const char *storage = "/download0/lamplight";
    save::ensure_directory(storage);
    std::string error;
    bool bootstrapped = false;
    auto config = lamplight::load_config(storage, "/app0/lamplight.conf", error, bootstrapped);
    ps5::Display display;
    if (!display.open())
    {
        sys::log("[Lamplight] Display could not open");
        sys::quit();
    }
    gfx::Renderer renderer;
    if (!renderer.init())
        sys::quit();
    gfx::Font faces[5];
    lamplight::Fonts fonts;
    const char *names[5] = {"text-regular", "text-semibold", "lexend-semibold", "lexend-regular",
                            "icons"};
    ui::FontRef *refs[5] = {&fonts.text, &fonts.bold, &fonts.num, &fonts.num_light, &fonts.icons};
    for (int i = 0; i < 5; ++i)
    {
        std::string bytes;
        if (!save::read_file(std::string("/app0/assets/fonts/") + names[i] + ".huifont", &bytes) ||
            !faces[i].load(bytes))
        {
            sys::log("[Lamplight] Font %s missing", names[i]);
            sys::quit();
        }
        *refs[i] = {&faces[i], renderer.batch().create_font_texture(faces[i])};
    }
    fonts.kit.regular = fonts.text;
    fonts.kit.semibold = fonts.bold;
    fonts.kit.display = fonts.num;
    fonts.kit.mono = fonts.num;
    fonts.kit.pixel = fonts.text;
    fonts.kit.hand = fonts.text;
    ps5::Pad pad;
    pad.open();
    InputTracker tracker;
    InputSettings input_settings;
    input_settings.repeat_interval_us = 150000;
    tracker.set_settings(input_settings);
    auto ime = ps5::make_ime_backend(pad.user());
    ps5::Keyboard keyboard(*ime);
    lamplight::Client client;
    lamplight::Resources resources;
    resources.start(storage);
    // What lamplight.conf added is kept, so the file can be deleted (it may hold the token).
    if (bootstrapped)
        resources.save(config);
    lamplight::Application app(config, fonts);
    app.send = [&](lamplight::Request r) { return client.send(std::move(r)); };
    app.save = [&](const lamplight::Config &c) { resources.save(c); };
    app.reconnect = [&](const lamplight::Config &c, std::set<std::string> watched)
    { client.start(c, std::move(watched)); };
    app.watch = [&](std::set<std::string> watched) { client.watch(std::move(watched)); };
    app.refresh_catalog = [&] { client.refresh_catalog(); };
    app.request_picture = [&](const std::string &url) { resources.request_picture(url); };
    client.start(config, app.watched());
    std::int64_t previous = sys::monotonic_us(), mirror = 0;
    std::int64_t previous_wall = lamplight::wall_clock().utc_ms;
    bool light_bar = false;
    std::uint64_t revision = ~std::uint64_t{0};
    bool splash = false, was_keyboard = false;
    gfx::DrawList list;
    ui::Feedback feedback;
    std::vector<GLuint> textures;
    while (!app.quitting())
    {
        const auto now = sys::monotonic_us();
        const float dt = std::clamp(static_cast<float>(now - previous) / 1000000.0f, 0.0f, 0.1f);
        previous = now;
        std::array<PadSample, 64> samples;
        const auto count = pad.read(samples);
        auto input = tracker.update(std::span<const PadSample>(samples.data(), count),
                                    static_cast<std::uint64_t>(now));
        if (now >= mirror)
        {
            auto state = client.snapshot();
            if (state.revision != revision)
            {
                revision = state.revision;
                app.receive(std::move(state));
            }
            // A save attempt is newer news than a settings file that could not be read.
            const auto saved = resources.save_status();
            if (!saved.empty())
                error.clear();
            app.storage_status(error.empty() ? saved : error);
            mirror = now + 100000;
        }
        ps5::KeyboardOutcome outcome;
        const bool completed = keyboard.poll(outcome);
        if (completed)
        {
            app.keyboard_result(outcome);
            std::fill(outcome.text.begin(), outcome.text.end(), '\0');
        }
        if (keyboard.active() || was_keyboard || completed)
        {
            input = InputFrame{};
            input.focus_lost = true;
        }
        was_keyboard = keyboard.active();
        // Rest mode stops the app: the wall clock then jumps by the time it was away (a clock set
        // by hand jumps too; reconnecting then costs nothing). The old connection is gone, so
        // reconnect at once rather than waiting for its ping to time out.
        const auto wall = lamplight::wall_clock();
        if (wall.utc_ms - previous_wall > 15000 || previous_wall - wall.utc_ms > 15000)
        {
            sys::log("[Lamplight] resumed after %lld s",
                     static_cast<long long>((wall.utc_ms - previous_wall) / 1000));
            app.resumed();
            client.start(app.config(), app.watched());
        }
        previous_wall = wall.utc_ms;
        feedback.clear();
        app.update(input, dt, wall, feedback);
        if (auto request = app.take_keyboard())
        {
            keyboard.open(*request);
            std::fill(request->initial.begin(), request->initial.end(), '\0');
        }
        if (app.config().vibration)
        {
            if (feedback.rumble_strength > 0)
                pad.rumble(feedback.rumble_strength, feedback.rumble_seconds);
            else if (!feedback.cues.empty())
                pad.rumble(0.12f, 0.035f);
        }
        // A finished timer pulses the light bar warm (and rumbles every two seconds, if vibration
        // is on), so it is noticed across the room.
        if (app.alerting())
        {
            const float pulse =
                0.5f + 0.5f * std::sin(static_cast<float>(now) / 1000000.0f * 3.1416f);
            pad.set_light_bar(static_cast<std::uint8_t>(80 + 175 * pulse),
                              static_cast<std::uint8_t>(50 + 136 * pulse), 20);
            light_bar = true;
            if (app.config().vibration &&
                (now / 2000000) != ((now - static_cast<std::int64_t>(dt * 1000000)) / 2000000))
                pad.rumble(0.5f, 0.25f);
        }
        else if (light_bar)
        {
            pad.reset_light_bar();
            light_bar = false;
        }
        pad.tick(dt);
        lamplight::Picture picture;
        while (resources.take_picture(picture))
        {
            GLuint texture = 0;
            if (picture.ok)
            {
                texture = upload(picture);
                textures.push_back(texture);
            }
            app.picture(picture.url, texture, picture.width, picture.height);
        }
        for (auto texture : app.take_released())
        {
            glDeleteTextures(1, &texture);
            std::erase(textures, texture);
        }
        list.clear();
        app.draw(list);
        renderer.begin();
        renderer.draw(list);
        renderer.present(0, display.width(), display.height());
        if (!display.swap())
            break;
        if (!splash)
        {
            sys::hide_splash_screen();
            splash = true;
        }
    }
    keyboard.abort();
    client.stop();
    resources.stop();
    pad.reset_light_bar();
    pad.close();
    for (auto texture : textures)
        glDeleteTextures(1, &texture);
    renderer.release();
    display.close();
    sys::quit();
}

// Lamplight for PS5 - Offscreen pictures of the real screens, driven by tests/fixture.json.
// Copyright (C) 2026 Hayden Griffin
// SPDX-License-Identifier: GPL-3.0-or-later
#include "core/save_file.hpp"
#include "fixture_home.hpp"
#include "gfx/canvas.hpp"
#include "gfx/gl_program.hpp"
#include "gfx/renderer.hpp"
#include <EGL/egl.h>
#include <EGL/eglext.h>
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
    auto display =
        eglGetPlatformDisplay(EGL_PLATFORM_SURFACELESS_MESA, EGL_DEFAULT_DISPLAY, nullptr);
    EGLint major, minor;
    if (!eglInitialize(display, &major, &minor) || !eglBindAPI(EGL_OPENGL_API))
        return 1;
    const EGLint attr[] = {EGL_CONTEXT_MAJOR_VERSION,
                           4,
                           EGL_CONTEXT_MINOR_VERSION,
                           5,
                           EGL_CONTEXT_OPENGL_PROFILE_MASK,
                           EGL_CONTEXT_OPENGL_CORE_PROFILE_BIT,
                           EGL_NONE};
    auto context = eglCreateContext(display, EGL_NO_CONFIG_KHR, EGL_NO_CONTEXT, attr);
    if (!eglMakeCurrent(display, EGL_NO_SURFACE, EGL_NO_SURFACE, context))
        return 2;
    hui::gfx::set_glsl_prefix("#version 450 core");
    hui::gfx::Renderer renderer;
    if (!renderer.init())
        return 3;
    hui::gfx::Font faces[5];
    Fonts fonts;
    const char *names[5] = {"text-regular", "text-semibold", "lexend-semibold", "lexend-regular",
                            "icons"};
    hui::ui::FontRef *refs[5] = {&fonts.text, &fonts.bold, &fonts.num, &fonts.num_light,
                                 &fonts.icons};
    for (int i = 0; i < 5; ++i)
    {
        std::string bytes;
        if (!hui::save::read_file(std::string("assets/fonts/") + names[i] + ".huifont", &bytes) ||
            !faces[i].load(bytes))
            return 4;
        *refs[i] = {&faces[i], renderer.batch().create_font_texture(faces[i])};
    }
    fonts.kit.regular = fonts.text;
    fonts.kit.semibold = fonts.bold;
    fonts.kit.display = fonts.kit.mono = fonts.num;
    fonts.kit.pixel = fonts.kit.hand = fonts.text;
    const std::string out = argc > 1 ? argv[1] : "build/preview/";
    std::filesystem::create_directories(out);
    hui::gfx::Canvas target;
    target.create(1920, 1080, 1);
    auto shoot = [&](Application &app, const std::string &name)
    {
        hui::gfx::DrawList list;
        app.draw(list);
        renderer.begin();
        renderer.draw(list);
        renderer.present(target.framebuffer(), 1920, 1080);
        glBindFramebuffer(GL_FRAMEBUFFER, target.framebuffer());
        std::vector<unsigned char> pixels(1920 * 1080 * 4);
        glReadPixels(0, 0, 1920, 1080, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
        stbi_flip_vertically_on_write(1);
        const bool ok =
            stbi_write_png((out + name + ".png").c_str(), 1920, 1080, 4, pixels.data(), 1920 * 4);
        std::printf("%s%s.png %s gl=0x%x\n", out.c_str(), name.c_str(), ok ? "written" : "FAILED",
                    glGetError());
        return ok;
    };
    using hui::Action;
    using hui::Direction;
    // One scene per picture: a fresh app, the fixture's house, then whatever the scene does.
    struct Scene
    {
        const char *name;
        std::function<void(FixtureHome &, Application &)> run;
    };
    const std::vector<Scene> scenes = {
        {"home", [](FixtureHome &, Application &) {}},
        {"weather", [](FixtureHome &, Application &app) { app.open(Page::weather); }},
        {"room", [](FixtureHome &, Application &app) { app.open(Page::room); }},
        {"room-armed",
         [](FixtureHome &h, Application &app)
         {
             app.open(Page::room);
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
         [](FixtureHome &h, Application &app)
         {
             h.clock.utc_ms = parse_iso_ms("2026-11-02T23:12:00Z", Zone{});
             h.snapshot.states.apply({{"c", {{"sun.sun", {{"+", {{"s", "below_horizon"}}}}}}}});
             h.reply(app);
             h.frame(app);
             h.press(app, Action::back); // the pasta timer's alert, which the jump set off
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
             for (int i = 0; i < 5; ++i)
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
        Application app(home.config(), fonts);
        home.wire(app);
        home.settle(app);
        scene.run(home, app);
        // Half a second of frames, so springs and lists settle as they would on screen.
        for (int i = 0; i < 30; ++i)
            home.frame(app);
        if (!shoot(app, scene.name))
            return 5;
    }
    target.destroy();
    renderer.release();
    eglMakeCurrent(display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
    eglDestroyContext(display, context);
    eglTerminate(display);
    return 0;
}

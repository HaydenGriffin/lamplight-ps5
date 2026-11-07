// Lamplight for PS5 - The app's own drawing with no window, and the pictures it asks for from
// tests/media. For the preview and the tour.
// Copyright (C) 2026 Hayden Griffin
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "afterglow/artwork.hpp"
#include "core/save_file.hpp"
#include "gfx/canvas.hpp"
#include "gfx/gl_program.hpp"
#include "gfx/renderer.hpp"
#include "lamplight/application.hpp"
#include "lamplight/resources.hpp"
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <string>
#include <utility>
#include <vector>

namespace lamplight
{
class Offscreen
{
  public:
    static constexpr int kWidth = 1920, kHeight = 1080;
    Fonts fonts;

    // 0 when ready, else the step that failed.
    int init()
    {
        display_ =
            eglGetPlatformDisplay(EGL_PLATFORM_SURFACELESS_MESA, EGL_DEFAULT_DISPLAY, nullptr);
        EGLint major, minor;
        if (!eglInitialize(display_, &major, &minor) || !eglBindAPI(EGL_OPENGL_API))
            return 1;
        const EGLint attr[] = {EGL_CONTEXT_MAJOR_VERSION,
                               4,
                               EGL_CONTEXT_MINOR_VERSION,
                               5,
                               EGL_CONTEXT_OPENGL_PROFILE_MASK,
                               EGL_CONTEXT_OPENGL_CORE_PROFILE_BIT,
                               EGL_NONE};
        context_ = eglCreateContext(display_, EGL_NO_CONFIG_KHR, EGL_NO_CONTEXT, attr);
        if (!eglMakeCurrent(display_, EGL_NO_SURFACE, EGL_NO_SURFACE, context_))
            return 2;
        hui::gfx::set_glsl_prefix("#version 450 core");
        if (!renderer_.init())
            return 3;
        const char *names[5] = {"text-regular", "text-semibold", "lexend-semibold",
                                "lexend-regular", "icons"};
        hui::ui::FontRef *refs[5] = {&fonts.text, &fonts.bold, &fonts.num, &fonts.num_light,
                                     &fonts.icons};
        for (int i = 0; i < 5; ++i)
        {
            std::string bytes;
            if (!hui::save::read_file(std::string("assets/fonts/") + names[i] + ".huifont",
                                      &bytes) ||
                !faces_[i].load(bytes))
                return 4;
            *refs[i] = {&faces_[i], renderer_.batch().create_font_texture(faces_[i])};
        }
        fonts.kit.regular = fonts.text;
        fonts.kit.semibold = fonts.bold;
        fonts.kit.display = fonts.kit.mono = fonts.num;
        fonts.kit.pixel = fonts.kit.hand = fonts.text;
        target_.create(kWidth, kHeight, 1);
        return 0;
    }
    ~Offscreen()
    {
        for (auto texture : textures_)
            glDeleteTextures(1, &texture);
        target_.destroy();
        renderer_.release();
        eglMakeCurrent(display_, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
        eglDestroyContext(display_, context_);
        eglTerminate(display_);
    }
    // What the app asks for, by the same rule every time: cover, camera, or a photo by name.
    void ask(const std::string &url)
    {
        const char *file = url.find("media_player_proxy") != std::string::npos ? "cover"
                           : url.find("camera_proxy") != std::string::npos     ? "camera"
                           : url.find("portrait") != std::string::npos         ? "portrait"
                           : url.find("snow") != std::string::npos             ? "snow"
                                                                               : "photo";
        asked_.push_back({url, std::string("tests/media/") + file + ".jpg"});
    }
    // Hands the app what it asked for, as the loader would on the next frame.
    void deliver(Application &app)
    {
        for (const auto &[url, file] : std::exchange(asked_, {}))
        {
            std::string bytes;
            std::vector<std::uint8_t> rgba;
            int w = 0, h = 0;
            if (!hui::save::read_file(file, &bytes, 8 * 1024 * 1024) ||
                !hui::afterglow::decode_image({bytes.begin(), bytes.end()}, rgba, w, h, 4096))
            {
                app.picture(url, 0, 0, 0);
                continue;
            }
            GLuint soft = 0;
            float brightness = 0.5f;
            if (url.ends_with(kSoftPicture))
            {
                std::vector<std::uint8_t> blurred;
                int sw = 0, sh = 0;
                soften(rgba, w, h, blurred, sw, sh);
                soft = upload(sw, sh, blurred);
                brightness = mean_brightness(blurred);
            }
            app.picture(url, upload(w, h, rgba), w, h, soft, brightness);
        }
    }
    // The app's next frame as RGBA, bottom row first.
    const std::vector<unsigned char> &capture(Application &app)
    {
        hui::gfx::DrawList list;
        app.draw(list);
        renderer_.begin();
        renderer_.draw(list);
        renderer_.present(target_.framebuffer(), kWidth, kHeight);
        glBindFramebuffer(GL_FRAMEBUFFER, target_.framebuffer());
        pixels_.resize(kWidth * kHeight * 4);
        glReadPixels(0, 0, kWidth, kHeight, GL_RGBA, GL_UNSIGNED_BYTE, pixels_.data());
        return pixels_;
    }

  private:
    GLuint upload(int width, int height, const std::vector<std::uint8_t> &pixels)
    {
        GLuint texture = 0;
        glGenTextures(1, &texture);
        glBindTexture(GL_TEXTURE_2D, texture);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                     pixels.data());
        textures_.push_back(texture);
        return texture;
    }
    EGLDisplay display_ = EGL_NO_DISPLAY;
    EGLContext context_ = EGL_NO_CONTEXT;
    hui::gfx::Renderer renderer_;
    hui::gfx::Font faces_[5];
    hui::gfx::Canvas target_;
    std::vector<std::pair<std::string, std::string>> asked_;
    std::vector<GLuint> textures_;
    std::vector<unsigned char> pixels_;
};
} // namespace lamplight

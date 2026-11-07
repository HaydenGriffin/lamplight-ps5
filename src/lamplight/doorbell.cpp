// Lamplight for PS5 - The full-screen camera: the doorbell's when it rings, or a room's camera
// tile. Copyright (C) 2026 Hayden Griffin SPDX-License-Identifier: GPL-3.0-or-later
#include "lamplight/application.hpp"
#include "lamplight/paint.hpp"
#include <algorithm>

namespace lamplight
{
using hui::Action;
using hui::gfx::Align;
namespace
{
// A frame on its way is never replaced (a slow camera would then show nothing); one the loader
// never answered is given up after this long.
constexpr std::int64_t kShowMs = 60000, kWatchMs = 5 * 60000, kFrameMs = 1000, kFrameWaitMs = 20000;
} // namespace

bool doorbell_rang(const std::string &trigger, const std::string &before, const std::string &now)
{
    if (before.empty() || now == before || now == "unavailable" || now == "unknown")
        return false;
    // A binary sensor (or a helper) rings by turning on; an event entity, a button or a
    // timestamp sensor by changing to a new time.
    const auto domain = domain_of(trigger);
    if (domain == "binary_sensor" || domain == "input_boolean" || domain == "switch")
        return now == "on";
    return before != "unavailable" && before != "unknown";
}
void Application::check_doorbell()
{
    if (config_.doorbell.empty() || !ready())
        return;
    const auto &state = snapshot_.states.get(config_.doorbell.trigger);
    if (!snapshot_.states.has(config_.doorbell.trigger))
        return;
    const auto before = std::exchange(doorbell_seen_, state.state);
    if (!doorbell_rang(config_.doorbell.trigger, before, state.state))
        return;
    if (!watching_.empty())
    {
        watching_.clear();
        want_picture(Pic::camera, "", true);
    }
    camera_asked_ = 0;
    doorbell_at_ = now();
    camera_until_ = now() + kShowMs;
    // It wakes the resting clock, and any page shows it.
    idle_s_ = 0;
    ambient_ = 0;
}
void Application::update_camera(const hui::InputFrame &input)
{
    if (input.is_pressed(Action::back) || input.is_pressed(Action::confirm))
        close_camera();
}
void Application::watch_camera(const std::string &entity, const std::string &name)
{
    watching_ = entity;
    watching_name_ = name;
    camera_asked_ = 0;
    camera_until_ = now() + kWatchMs;
}
void Application::close_camera()
{
    camera_until_ = 0;
    watching_.clear();
    want_picture(Pic::camera, "", true);
}
// While it shows: the camera's picture again each second, once the last one is back.
void Application::refresh_camera()
{
    if (!camera_showing())
    {
        if (!slot(Pic::camera).wanted.empty() || !watching_.empty())
            close_camera();
        return;
    }
    const auto &s = slot(Pic::camera);
    const auto wait = s.waiting ? kFrameWaitMs : kFrameMs;
    if (camera_asked_ && now() - camera_asked_ < wait)
        return;
    const auto &camera = watching_.empty() ? config_.doorbell.camera : watching_;
    const auto picture = text(snapshot_.states.get(camera).attributes, "entity_picture");
    if (picture.empty())
        return;
    // A new URL each time, so each frame is fetched; the camera's token changes by itself.
    want_picture(Pic::camera,
                 ha_url(picture) + (picture.find('?') == std::string::npos ? "?" : "&") +
                     "lamplight=" + std::to_string(++camera_frame_),
                 true);
    camera_asked_ = now();
}
void Application::draw_camera(Paint &p) const
{
    p.list.rounded_rect({0, 0, 1920, 1080}, 0, Color::rgb(0x000000));
    if (const auto *frame = shown_picture(Pic::camera))
    {
        const float scale = std::min(1920.0f / static_cast<float>(frame->width),
                                     1080.0f / static_cast<float>(frame->height));
        const float w = static_cast<float>(frame->width) * scale,
                    h = static_cast<float>(frame->height) * scale;
        p.list.image(frame->texture, {960 - w / 2, 540 - h / 2, w, h}, hui::gfx::kFullUv,
                     tone::white, 0);
    }
    else
        p.text("Connecting to the camera\xE2\x80\xA6", 960, 560, size::title, tone::muted, false,
               Align::center);
    // Who's there, or which camera this is: a banner over the top of the picture.
    const Rect banner{size::margin, 64, 820, 132};
    p.list.rounded_rect(banner, size::chip_radius, tone::toast);
    p.wash(banner, size::chip_radius);
    p.disc(banner.x + 70, banner.cy(), size::disc, watching_.empty() ? "bell-ring-outline" : "cctv",
           DiscKind::on);
    const auto line = [&](const std::string &top, const std::string &under)
    {
        p.text(top, banner.x + 130, banner.y + 60, size::title, tone::text, true, Align::left,
               banner.w - 160);
        p.text(under, banner.x + 130, banner.y + 104, size::label, tone::warm, false, Align::left,
               banner.w - 160);
    };
    if (!watching_.empty())
    {
        line(watching_name_, "Live, updates each second");
        return;
    }
    const auto &camera = snapshot_.states.get(config_.doorbell.camera);
    const auto name = config_.doorbell.name.empty() ? camera.name(config_.doorbell.camera)
                                                    : config_.doorbell.name;
    line("Someone's at the door", name + " \xC2\xB7 rang at " + hhmm(doorbell_at_, clock_.zone));
}
} // namespace lamplight

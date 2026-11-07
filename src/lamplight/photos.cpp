// Lamplight for PS5 - Photos: the slideshow page, the resting clock's picture, what goes behind the
// pages, and Setup's folder chooser. Pictures come from Home Assistant's media.
// Copyright (C) 2026 Hayden Griffin
// SPDX-License-Identifier: GPL-3.0-or-later
#include "lamplight/application.hpp"
#include "lamplight/paint.hpp"
#include "lamplight/resources.hpp"
#include <algorithm>
#include <cmath>
#include <functional>
#include <random>
#include <utility>

namespace lamplight
{
using hui::Action;
using hui::Direction;
using hui::gfx::Align;
namespace
{
constexpr std::int64_t kListMs = 30 * 60000, kRetryMs = 5 * 60000, kAfterFailureMs = 3000;
constexpr std::int64_t kHintsMs = 6000;
// Just past the minute the loader won't refetch a URL that failed for.
constexpr std::int64_t kBackdropRetryMs = 61000;
// A paused picture would sit on an OLED for as long as nobody touches the controller.
constexpr float kPausedForS = 15 * 60;
constexpr float kClockQuietAfterS = 20 * 60, kClockQuietOverS = 60;
constexpr float kFadeS = 1.6f, kZoom = 0.10f;
// Narrower than this, a picture is shown whole over its own blur rather than cropped to the screen
// (a phone's portrait photo would lose its top and bottom).
constexpr float kWholeBelowRatio = 1.2f;
constexpr Rect kScreen{0, 0, 1920, 1080};
// What the slideshow's left and right bumpers step through, in seconds.
constexpr int kIntervals[] = {10, 20, 30, 60, 120, 300, 900};

bool is_image(const Json &child)
{
    return text(child, "media_class") == "image" ||
           text(child, "media_content_type").starts_with("image/");
}
// The middle of a picture that fills the screen.
Rect cover_uv(int width, int height)
{
    const float ratio = static_cast<float>(width) / static_cast<float>(height);
    Rect uv = hui::gfx::kFullUv;
    if (ratio > 16.0f / 9.0f)
    {
        uv.w = (16.0f / 9.0f) / ratio;
        uv.x = (1 - uv.w) / 2;
    }
    else
    {
        uv.h = ratio / (16.0f / 9.0f);
        uv.y = (1 - uv.h) / 2;
    }
    return uv;
}
// A slow move across it, each picture its own way: in or out, towards one corner.
Rect drifting_uv(Rect cover, float t, std::size_t seed)
{
    const float z = 1 + kZoom * ((seed & 1) ? 1 - t : t);
    const float w = cover.w / z, h = cover.h / z;
    const float fx = (seed & 2) ? t : 1 - t, fy = (seed & 4) ? t : 1 - t;
    return {cover.x + (cover.w - w) * fx, cover.y + (cover.h - h) * fy, w, h};
}
std::string every_words(int seconds)
{
    if (seconds < 60)
        return std::to_string(seconds) + " s";
    return std::to_string(seconds / 60) + " min";
}
} // namespace

std::vector<std::string> photo_ids(const Json &browsed)
{
    std::vector<std::string> ids;
    const auto &children = field(browsed, "children");
    if (children.is_array())
        for (const auto &c : children)
        {
            const auto id = text(c, "media_content_id");
            if (!id.empty() && is_image(c))
                ids.push_back(id);
            if (ids.size() >= 1000)
                break;
        }
    return ids;
}
std::string photo_folder_name(const std::string &id)
{
    const auto end = id.find_last_of("/|");
    return end == std::string::npos ? id : id.substr(end + 1);
}
std::vector<std::pair<std::string, std::string>> photo_folders(const Json &browsed)
{
    std::vector<std::pair<std::string, std::string>> folders;
    const auto &children = field(browsed, "children");
    if (children.is_array())
        for (const auto &c : children)
        {
            const auto id = text(c, "media_content_id");
            if (id.empty() || is_image(c) || folders.size() >= 200)
                continue;
            if (field(c, "can_expand") == true || text(c, "media_class") == "directory")
                folders.emplace_back(id, text(c, "title", id));
        }
    return folders;
}
bool Application::photos_wanted() const
{
    return !config_.photos.empty() && (page_ == Page::photos || config_.background == "photos" ||
                                       (page_ == Page::home && ambient_ > 0));
}
void Application::update_photos()
{
    photo_age_s_ += frame_dt_;
    if (previous_photo_.texture && (photo_age_s_ >= kFadeS || config_.reduced_motion))
        release(previous_photo_);
    if (!photos_wanted())
    {
        if (!slot(Pic::photo).wanted.empty())
            want_picture(Pic::photo, "", true);
        release(pics_[static_cast<std::size_t>(Pic::photo)].shown);
        // Back again, a picture straight away, not when this one's time would have been up.
        photo_at_ = 0;
        return;
    }
    if (photo_paused_ && (page_ != Page::photos || idle_s_ > kPausedForS))
        photo_paused_ = false;
    if (photo_stepped_ && !photo_busy_ && !photo_queued_ && !slot(Pic::photo).waiting)
        photo_stepped_ = false;
    if (!ready() || photo_busy_ || alert_ || camera_showing())
        return;
    if (photo_queued_)
    {
        step_photo(std::exchange(photo_queued_, 0));
        return;
    }
    if (!photo_walk_.empty())
    {
        continue_photo_walk();
        return;
    }
    if (photo_list_.empty() || now() - photo_listed_at_ > kListMs)
    {
        if (photo_listed_at_ && now() - photo_listed_at_ < kRetryMs && photo_list_.empty())
            return;
        start_photo_walk();
        return;
    }
    if (photo_paused_ && shown_picture(Pic::photo))
        return;
    // Not before the last one is up (the loader always answers): one slower than a short interval
    // would be passed over before it ever showed.
    if (slot(Pic::photo).waiting)
        return;
    if (photo_at_ && now() - photo_at_ < config_.photo_seconds * std::int64_t{1000})
        return;
    step_photo(1);
}
void Application::step_photo(int by)
{
    if (photo_list_.empty() || photo_busy_)
        return;
    const auto count = static_cast<std::int64_t>(photo_list_.size());
    photo_next_ =
        static_cast<std::size_t>((static_cast<std::int64_t>(photo_next_) + by + count) % count);
    photo_at_ = now();
    photo_busy_ = dispatch({{{"type", "media_source/resolve_media"},
                             {"media_content_id", photo_list_[photo_next_]}}},
                           Job::photo_url) != 0;
}
void Application::start_photo_walk()
{
    photo_listed_at_ = now();
    photo_walk_failed_ = false;
    photo_found_.clear();
    photo_walked_ = {config_.photos};
    photo_walk_ = {config_.photos};
    continue_photo_walk();
}
void Application::continue_photo_walk()
{
    const auto id = std::move(photo_walk_.front());
    photo_walk_.pop_front();
    photo_busy_ = dispatch({{{"type", "media_source/browse_media"}, {"media_content_id", id}}},
                           Job::photo_list) != 0;
    if (!photo_busy_)
        photo_walk_.clear();
}
void Application::finish_photo_walk()
{
    if (photo_found_.empty())
    {
        // Every folder read and nothing in them: they've been emptied. A failed read says nothing.
        if (!photo_walk_failed_ && !photo_list_.empty())
        {
            photo_list_.clear();
            want_picture(Pic::photo, "", true);
            release(previous_photo_);
        }
        return;
    }
    const bool first = photo_list_.empty();
    std::minstd_rand order(static_cast<std::uint32_t>(now()));
    std::shuffle(photo_found_.begin(), photo_found_.end(), order);
    photo_list_ = std::exchange(photo_found_, {});
    photo_next_ %= photo_list_.size();
    if (first)
        photo_at_ = 0;
}
// One that can't be read (too large, or not JPEG or PNG) doesn't hold up the next.
void Application::photo_failed()
{
    photo_at_ = now() - config_.photo_seconds * std::int64_t{1000} + kAfterFailureMs;
}
void Application::update_slideshow(const hui::InputFrame &input)
{
    if (input.pressed || input.nav != Direction::none)
        photo_hints_until_ = now() + kHintsMs;
    if ((input.nav == Direction::left || input.nav == Direction::right) && !input.nav_repeat &&
        !photo_list_.empty())
    {
        const int by = input.nav == Direction::right ? 1 : -1;
        if (photo_busy_)
            photo_queued_ = by;
        else
            step_photo(by);
        photo_stepped_ = true;
    }
    if (input.is_pressed(Action::confirm))
    {
        photo_paused_ = !photo_paused_;
        // Playing again starts this picture's time afresh.
        photo_at_ = now();
    }
    if (input.is_pressed(Action::west))
        photo_clock_ = !photo_clock_;
    if (input.is_pressed(Action::page_prev) || input.is_pressed(Action::page_next))
    {
        const int by = input.is_pressed(Action::page_next) ? 1 : -1;
        const int count = static_cast<int>(std::size(kIntervals));
        int at = 0;
        while (at < count - 1 && kIntervals[at] < config_.photo_seconds)
            ++at;
        at = std::clamp(at + by, 0, count - 1);
        config_.photo_seconds = kIntervals[at];
        persist();
        toast("A new photo every " + every_words(config_.photo_seconds));
    }
}
std::string Application::picture_path(const std::string &source) const
{
    if (source == "room")
        return config_.view_assist.empty()
                   ? ""
                   : text(snapshot_.states.get(config_.view_assist).attributes, "background");
    // An entity's background attribute, as a View Assist device's.
    if (source.find('.') != std::string::npos && source.find('/') == std::string::npos)
        return text(snapshot_.states.get(source).attributes, "background");
    return source.starts_with("/") || source.starts_with("http://") ? source : "";
}
std::string Application::backdrop_url() const
{
    // The cover behind Now Playing; the room's own picture on its page; otherwise the background
    // setting's fixed picture.
    if (const auto cover = cover_url(); !cover.empty())
        return cover + kSoftPicture;
    std::string path;
    if (page_ == Page::room)
        path = picture_path(current_room().photo);
    if (path.empty())
        path = picture_path(config_.background);
    return path.empty() ? "" : ha_url(path) + kSoftPicture;
}
void Application::update_backdrop()
{
    // Not before Home Assistant answers: a picture that fails now would not be asked for again.
    if (!ready())
        return;
    const auto url = backdrop_url();
    auto &s = pics_[static_cast<std::size_t>(Pic::backdrop)];
    if (url != s.wanted)
        backdrop_asked_ = now();
    want_picture(Pic::backdrop, url, true);
    // One that hasn't come (Home Assistant was restarting) is asked for again. The loader ignores
    // a URL it is still fetching.
    if (url.empty() || s.shown.url == url || now() - backdrop_asked_ < kBackdropRetryMs ||
        !request_picture)
        return;
    backdrop_asked_ = now();
    s.waiting = true;
    request_picture(url);
}
// How bright the picture behind the clock is.
float Application::clock_picture_brightness() const
{
    const auto *photo = shown_picture(Pic::photo);
    const auto *fixed = shown_picture(Pic::backdrop);
    const auto *showing = (ambient_ > 0 || !fixed) && photo ? photo : fixed;
    return showing ? showing->brightness : 0.5f;
}
bool Application::has_page_backdrop() const
{
    if (page_ == Page::weather || page_ == Page::home || page_ == Page::photos)
        return false;
    return shown_picture(Pic::backdrop) ||
           (config_.background == "photos" && shown_picture(Pic::photo));
}
bool Application::has_clock_backdrop() const
{
    return shown_picture(Pic::backdrop) ||
           (config_.background == "photos" && shown_picture(Pic::photo));
}
void Application::draw_framed(Paint &p, const Shown &photo, float alpha, float t) const
{
    const auto tint = tone::white.with_alpha(alpha);
    const float ratio = static_cast<float>(photo.width) / static_cast<float>(photo.height);
    const auto cover = cover_uv(photo.width, photo.height);
    if (ratio < kWholeBelowRatio)
    {
        p.list.image(photo.soft ? photo.soft : photo.texture, kScreen, cover,
                     Color::rgb(0x8a8a8a, alpha));
        const float scale = std::min(1920.0f / static_cast<float>(photo.width),
                                     1080.0f / static_cast<float>(photo.height));
        const float w = static_cast<float>(photo.width) * scale,
                    h = static_cast<float>(photo.height) * scale;
        p.list.image(photo.texture, {960 - w / 2, 540 - h / 2, w, h}, hui::gfx::kFullUv, tint);
        return;
    }
    const auto uv = config_.reduced_motion ? cover
                                           : drifting_uv(cover, std::clamp(t, 0.0f, 1.0f),
                                                         std::hash<std::string>{}(photo.url));
    p.list.image(photo.texture, kScreen, uv, tint);
}
void Application::draw_fading_photo(Paint &p, float alpha) const
{
    const auto *photo = shown_picture(Pic::photo);
    if (!photo)
        return;
    const float seconds = static_cast<float>(std::max(config_.photo_seconds, 1));
    const float fade = previous_photo_.texture ? std::min(1.0f, photo_age_s_ / kFadeS) : 1.0f;
    if (previous_photo_.texture && fade < 1)
        draw_framed(p, previous_photo_, alpha, previous_photo_t_ + photo_age_s_ / seconds);
    draw_framed(p, *photo, alpha * fade, photo_age_s_ / seconds);
}
// Behind the clock: while it rests, dim enough that the time stays the thing to read (and much
// dimmer late at night); with a picture as the background, full until then, under a scrim.
void Application::draw_photo(Paint &p) const
{
    const bool night = late_at_night();
    // Dimmer the brighter it is, so the words stay readable and rest stays quieter than awake:
    // a dark photo keeps its strength, a white one does not. And dimmer again in deep rest.
    const float bright = std::max(clock_picture_brightness(), 0.01f);
    const float strength = night ? std::min(0.2f, 0.1f / bright) : std::min(0.35f, 0.2f / bright);
    const float resting = std::min(1.0f, ambient_ * 1.2f) * strength * (1 - 0.5f * deep_rest());
    const bool always = has_clock_backdrop();
    const float alpha = always ? 1 + (resting - 1) * ambient_ : resting;
    if (alpha <= 0)
        return;
    if (ambient_ > 0 && shown_picture(Pic::photo))
        draw_fading_photo(p, alpha);
    else if (const auto *fixed = shown_picture(Pic::backdrop))
        // One picture for hours moves very slowly, there and back, so nothing stays put on an OLED.
        draw_framed(p, *fixed, alpha, 0.5f + 0.5f * std::sin(time_ * 6.2832f / 600));
    else if (always)
        draw_fading_photo(p, alpha);
    if (always && ambient_ < 1)
    {
        // The Photo Rule: words on a photo are white on a dark gradient.
        const float awake = 1 - ambient_;
        // As strong as this picture needs: more for snow or a white wall, little for a dark one.
        const float shade = 0.35f + 0.27f * std::clamp((bright - 0.3f) / 0.6f, 0.0f, 1.0f);
        p.list.rounded_rect(kScreen, 0, tone::page.with_alpha(shade * awake));
        p.list.rounded_rect({0, 0, 1920, 170}, 0, Color::rgb(0x000000, 0.55f * awake));
        p.list.gradient_rect({0, 170, 1920, 130}, 0, Color::rgb(0x000000, 0.55f * awake),
                             Color::rgb(0x000000, 0.0f));
        p.list.gradient_rect({0, 560, 1920, 226}, 0, Color::rgb(0x000000, 0.0f),
                             Color::rgb(0x000000, 0.55f * awake));
        p.list.gradient_rect({0, 786, 1920, 294}, 0, Color::rgb(0x000000, 0.55f * awake),
                             Color::rgb(0x000000, 0.8f * awake));
    }
    // Resting over a picture, the weather and date along the top keep a little shade.
    if (ambient_ > 0 && (always || shown_picture(Pic::photo)))
        p.list.gradient_rect({0, 0, 1920, 260}, 0, Color::rgb(0x000000, 0.4f * ambient_),
                             Color::rgb(0x000000, 0.0f));
}
// Behind a page: the picture's blurred copy, dark enough for the smoked glass on it.
void Application::draw_page_backdrop(Paint &p) const
{
    const auto draw_soft = [&](const Shown &photo, float alpha)
    {
        p.list.image(photo.soft ? photo.soft : photo.texture, kScreen,
                     cover_uv(photo.width, photo.height), tone::white.with_alpha(alpha));
    };
    float brightness = 0.5f;
    if (const auto *fixed = shown_picture(Pic::backdrop))
    {
        draw_soft(*fixed, 1);
        brightness = fixed->brightness;
    }
    else if (const auto *photo = shown_picture(Pic::photo))
    {
        const float fade = previous_photo_.texture ? std::min(1.0f, photo_age_s_ / kFadeS) : 1.0f;
        if (previous_photo_.texture && fade < 1)
            draw_soft(previous_photo_, 1);
        draw_soft(*photo, fade);
        brightness = photo->brightness;
    }
    p.list.rounded_rect(kScreen, 0, tone::page.with_alpha(page_shade(brightness)));
}
float page_shade(float brightness)
{
    return 0.62f + 0.2f * std::clamp((brightness - 0.3f) / 0.6f, 0.0f, 1.0f);
}
float slideshow_strength(bool late, float brightness)
{
    return late ? std::min(0.45f, 0.3f / std::max(brightness, 0.01f)) : 1.0f;
}
void Application::draw_slideshow(Paint &p) const
{
    p.list.rounded_rect(kScreen, 0, Color::rgb(0x000000));
    const bool late = late_at_night();
    if (const auto *shown = shown_picture(Pic::photo))
        draw_fading_photo(p, slideshow_strength(late, shown->brightness));
    else
    {
        const bool looking = photo_busy_ || !photo_walk_.empty() || slot(Pic::photo).waiting;
        p.empty({460, 340, 1000, 400}, "image-multiple-outline",
                !ready()              ? "Connecting to Home Assistant\xE2\x80\xA6"
                : looking             ? "Looking for photos\xE2\x80\xA6"
                : photo_list_.empty() ? "No photos in that folder"
                                      : "This photo couldn't be read",
                "From the folder chosen in Setup: JPEG or PNG, at most 4096 pixels a side.");
    }
    // The shade for the words along the bottom: the clock's, and the hints' while they're up.
    if (photo_clock_ || now() < photo_hints_until_)
    {
        p.list.gradient_rect({0, 520, 1920, 260}, 0, Color::rgb(0x000000, 0.0f),
                             Color::rgb(0x000000, 0.6f));
        p.list.gradient_rect({0, 780, 1920, 300}, 0, Color::rgb(0x000000, 0.6f),
                             Color::rgb(0x000000, 0.85f));
    }
    if (photo_clock_)
    {
        // The time, small, over a dark gradient. This page is left running, so it drifts as far as
        // the resting clock does, and goes quieter late at night or once it has been left alone.
        const float dx = config_.reduced_motion ? 0 : 60 * std::sin(time_ * 6.2832f / 317),
                    dy = config_.reduced_motion ? 0 : 36 * std::sin(time_ * 6.2832f / 211);
        const float left_alone =
            std::clamp((idle_s_ - kClockQuietAfterS) / kClockQuietOverS, 0.0f, 1.0f);
        p.list.push_transform(1, 0, 0, dx, dy);
        p.list.push_opacity(late ? 0.7f : 1 - 0.3f * left_alone);
        std::string period;
        const auto digits = clock_digits(now(), clock_.zone, period);
        const float width = p.number(digits, size::margin, 880, 128, tone::white);
        if (!period.empty())
            p.number(period, size::margin + width + 16, 880, 44, tone::white);
        p.text(long_date(now(), clock_.zone), size::margin, 934, size::label, tone::white);
        const auto &w = snapshot_.states.get(config_.weather);
        if (w.available())
        {
            p.icon(condition_icon(w.state, after_dark()), 1920 - size::margin - 170, 856, 60,
                   tone::white);
            p.number(degrees(number(field(w.attributes, "temperature"))), 1920 - size::margin, 880,
                     72, tone::white, Align::right);
        }
        p.list.pop_opacity();
        p.list.pop_transform();
    }
    // A step by hand shows it was heard until its picture is up.
    if (photo_stepped_)
    {
        const std::string words = "Loading the photo\xE2\x80\xA6";
        const Rect chip{size::margin, 56, p.measure(words, size::label, true) + 72, 72};
        p.list.rounded_rect(chip, 36, tone::toast);
        p.text(words, chip.x + 36, chip.cy() + 11, size::label, tone::white, true);
    }
    if (photo_paused_)
    {
        const Rect chip{1920 - size::margin - 220, 56, 220, 72};
        p.list.rounded_rect(chip, 36, tone::toast);
        p.icon("pause", chip.x + 46, chip.cy(), 36, tone::warm);
        p.text("Paused", chip.x + 76, chip.cy() + 11, size::label, tone::white, true);
    }
}
void Application::browse_folder()
{
    folder_loading_ = true;
    folder_failed_ = false;
    folder_items_.clear();
    folder_pictures_ = 0;
    // The top of Home Assistant's media has no id of its own.
    Json message{{"type", "media_source/browse_media"}};
    if (!folder_path_.back().empty())
        message["media_content_id"] = folder_path_.back();
    if (!dispatch({std::move(message)}, Job::folder, folder_path_.back()))
    {
        folder_loading_ = false;
        folder_failed_ = true;
        toast("Not connected to Home Assistant", true, 5000);
    }
    rebuild_list();
    list_.set_focus(0);
}
void Application::folder_listed(const Result &r, const std::string &folder)
{
    // Left since it was asked for: the folder now open has its own reply coming.
    if (folder_path_.empty() || folder != folder_path_.back())
        return;
    folder_loading_ = false;
    folder_failed_ = !r.ok;
    if (!r.ok)
        toast("Couldn't read that folder: " + r.message, true, 5000);
    else
    {
        folder_items_ = photo_folders(r.response);
        folder_pictures_ = static_cast<int>(photo_ids(r.response).size());
    }
    if (page_ == Page::pick && pick_ == Pick::photos)
    {
        rebuild_list();
        // At the top with a folder in use, the first row clears it: a stray press shouldn't.
        if (folder_path_.size() == 1 && !config_.photos.empty() && !folder_items_.empty())
            list_.set_focus(1);
    }
}
void Application::activate_folder(int index)
{
    if (index < 0 || index >= static_cast<int>(pick_ids_.size()))
        return;
    // The first row: use this folder, or at the top, no photos at all.
    if (index == 0)
    {
        const auto before = config_;
        config_.photos = folder_path_.back();
        if (config_.photos.empty() && config_.background == "photos")
            config_.background = "sky";
        forget_changed(before);
        apply_settings();
        open(Page::setup);
        return;
    }
    folder_path_.push_back(pick_ids_[static_cast<std::size_t>(index)]);
    browse_folder();
}
} // namespace lamplight

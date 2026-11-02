// Lamplight for PS5 - Settings saved off the rendering thread, and pictures fetched and decoded.
// Copyright (C) 2026 Hayden Griffin
// SPDX-License-Identifier: GPL-3.0-or-later
#include "lamplight/resources.hpp"
#include "afterglow/artwork.hpp"
#include "core/save_file.hpp"
#include "platform/ps5/system.hpp"
#include <algorithm>

namespace lamplight
{
Config load_config(const std::string &directory, const std::string &bootstrap, std::string &error,
                   bool &bootstrapped)
{
    // The saved settings, then lamplight.conf beside eboot.bin over them: its keys win, so the
    // settings Setup doesn't offer (a room page, Help's categories, names and icons) can be added
    // at any time with a file holding just those.
    bootstrapped = false;
    Json merged = Json::object();
    std::string data;
    if (hui::save::read_file(directory + "/settings.json", &data, 262144))
    {
        auto j = Json::parse(data, nullptr, false);
        if (j.is_object())
            merged = std::move(j);
        else
            error = "Saved settings could not be read. Enter them again in Setup.";
    }
    data.clear();
    if (hui::save::read_file(bootstrap, &data, 262144))
    {
        auto j = Json::parse(data, nullptr, false);
        if (j.is_object())
        {
            for (auto it = j.begin(); it != j.end(); ++it)
                merged[it.key()] = it.value();
            bootstrapped = true;
        }
        else
            error = "lamplight.conf could not be read; it was left out.";
    }
    return Config::parse(merged);
}
Resources::~Resources()
{
    stop();
}
template <void (Resources::*Work)()> bool Resources::spawn(pthread_t &thread, Resources *owner)
{
    pthread_attr_t attr;
    if (pthread_attr_init(&attr) != 0)
        return false;
    int rc = pthread_attr_setstacksize(&attr, 1024 * 1024);
    if (!rc)
        rc = pthread_create(
            &thread, &attr,
            [](void *p) -> void *
            {
                (static_cast<Resources *>(p)->*Work)();
                return nullptr;
            },
            owner);
    pthread_attr_destroy(&attr);
    return rc == 0;
}
bool Resources::start(std::string directory)
{
    directory_ = std::move(directory);
    stopping_ = false;
    started_ = spawn<&Resources::run>(thread_, this);
    if (!started_)
        status_ = "Could not start the storage worker. Changes cannot be saved.";
    // A slow picture server never holds up a settings save.
    pictures_started_ = spawn<&Resources::fetch_pictures>(picture_thread_, this);
    return started_;
}
void Resources::stop()
{
    stopping_ = true;
    if (started_)
        pthread_join(thread_, nullptr);
    if (pictures_started_)
        pthread_join(picture_thread_, nullptr);
    started_ = false;
    pictures_started_ = false;
}
void Resources::save(const Config &c)
{
    std::lock_guard lock(mutex_);
    save_ = c.json().dump(2);
    dirty_ = true;
    status_ = "Saving…";
}
std::string Resources::save_status() const
{
    std::lock_guard lock(mutex_);
    return status_;
}
void Resources::request_picture(const std::string &url)
{
    std::lock_guard lock(mutex_);
    if (!url.starts_with("http://") || url.size() > 2048 || requested_.size() >= 64 ||
        requested_.contains(url))
        return;
    if (auto failed = failed_.find(url);
        failed != failed_.end() && hui::sys::monotonic_us() < failed->second)
        return;
    requested_.insert(url);
    urls_.push_back(url);
}
bool Resources::take_picture(Picture &picture)
{
    std::lock_guard lock(mutex_);
    if (pictures_.empty())
        return false;
    picture = std::move(pictures_.front());
    pictures_.pop_front();
    return true;
}
void Resources::run()
{
    hui::save::ensure_directory(directory_);
    for (;;)
    {
        std::string save;
        bool dirty = false;
        {
            std::lock_guard lock(mutex_);
            dirty = dirty_;
            if (dirty)
            {
                save = save_;
                dirty_ = false;
            }
        }
        if (dirty)
        {
            // 0600: the token and the Wi-Fi password are in here.
            auto error = hui::save::write_atomic(directory_ + "/settings.json", save, 0600);
            std::lock_guard lock(mutex_);
            status_ =
                error.empty() ? "Saved on this PS5" : "Could not save settings. Check app storage.";
        }
        if (stopping_)
        {
            std::lock_guard lock(mutex_);
            if (!dirty_)
                break;
            continue;
        }
        hui::sys::sleep_us(20000);
    }
}
void Resources::fetch_pictures()
{
    while (!stopping_)
    {
        std::string url;
        {
            std::lock_guard lock(mutex_);
            if (!urls_.empty() && pictures_.size() < 4)
            {
                url = urls_.front();
                urls_.pop_front();
            }
        }
        if (url.empty())
        {
            hui::sys::sleep_us(20000);
            continue;
        }
        Picture picture;
        picture.url = url;
        picture.ok = load(url, picture);
        std::lock_guard lock(mutex_);
        // requested_ holds the URLs in flight; a failed one waits a minute before it is fetched
        // again.
        requested_.erase(url);
        if (!picture.ok)
            failed_[url] = hui::sys::monotonic_us() + 60000000;
        pictures_.push_back(std::move(picture));
    }
}
bool Resources::load(const std::string &url, Picture &picture)
{
    std::vector<std::uint8_t> bytes, rgba;
    int w = 0, h = 0;
    if (!hui::afterglow::fetch_image(
            url, [&] { return stopping_.load(); }, bytes) ||
        !hui::afterglow::decode_image(bytes, rgba, w, h) || w <= 0 || h <= 0)
        return false;
    // At most 1024 a side: the picture is shown beside the answer, never full screen.
    const float scale = std::min(1.0f, 1024.0f / static_cast<float>(std::max(w, h)));
    picture.width = std::max(1, static_cast<int>(static_cast<float>(w) * scale));
    picture.height = std::max(1, static_cast<int>(static_cast<float>(h) * scale));
    picture.rgba.resize(static_cast<std::size_t>(picture.width * picture.height * 4));
    for (int y = 0; y < picture.height; y++)
        for (int x = 0; x < picture.width; x++)
            for (int c = 0; c < 4; c++)
                picture.rgba[static_cast<std::size_t>((y * picture.width + x) * 4 + c)] =
                    rgba[static_cast<std::size_t>(
                        ((y * h / picture.height) * w + x * w / picture.width) * 4 + c)];
    return true;
}
} // namespace lamplight

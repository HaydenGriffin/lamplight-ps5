// Lamplight for PS5 - Settings saved off the rendering thread, and pictures fetched and decoded.
// Copyright (C) 2026 Hayden Griffin
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "lamplight/config.hpp"
#include <atomic>
#include <deque>
#include <map>
#include <mutex>
#include <pthread.h>
#include <set>

namespace lamplight
{
// A picture's URL ending in this also gets a small blurred copy, for behind a page (DESIGN.md's
// Pre-blur: blurred once, then simply scaled up). The fragment never goes to the server.
inline constexpr const char *kSoftPicture = "#soft";
struct Picture
{
    std::string url;
    bool ok = false;
    int width = 0, height = 0;
    std::vector<std::uint8_t> rgba;
    int soft_width = 0, soft_height = 0;
    std::vector<std::uint8_t> soft;
    float brightness = 0.5f; // how bright it is overall, 0 to 1
};
// How bright a picture is overall, 0 (black) to 1 (white).
float mean_brightness(const std::vector<std::uint8_t> &rgba);
// The EXIF orientation of a JPEG (1 to 8; 1 for upright, or when it doesn't say). Phones store a
// portrait photo sideways and say so here.
int jpeg_orientation(const std::vector<std::uint8_t> &bytes);
// RGBA pixels turned the way the orientation says, so they read upright.
void apply_orientation(std::vector<std::uint8_t> &rgba, int &width, int &height, int orientation);
// RGBA pixels averaged down to a smaller size (each no larger than the source's).
std::vector<std::uint8_t> shrunk(const std::vector<std::uint8_t> &rgba, int width, int height,
                                 int to_width, int to_height);
// A small, heavily blurred copy.
void soften(const std::vector<std::uint8_t> &rgba, int width, int height,
            std::vector<std::uint8_t> &soft, int &soft_width, int &soft_height);
class Resources
{
  public:
    ~Resources();
    bool start(std::string directory);
    void save(const Config &config);
    // An http:// picture (the assistant's answer can have one). Each URL is fetched once; a
    // failed one is tried again after a minute.
    void request_picture(const std::string &url);
    bool take_picture(Picture &picture);
    std::string save_status() const;
    void stop();

  private:
    void run();
    void fetch_pictures();
    bool load(const std::string &url, Picture &picture);
    template <void (Resources::*Work)()> static bool spawn(pthread_t &thread, Resources *owner);
    std::string directory_, save_, status_;
    bool dirty_ = false, started_ = false, pictures_started_ = false;
    std::set<std::string> requested_;
    std::map<std::string, std::int64_t> failed_;
    std::deque<std::string> urls_;
    std::deque<Picture> pictures_;
    mutable std::mutex mutex_;
    std::atomic<bool> stopping_{true};
    pthread_t thread_{}, picture_thread_{};
};
// The saved settings, with lamplight.conf beside eboot.bin over them (its keys win). bootstrapped
// says the file was there, so the result should be saved.
Config load_config(const std::string &directory, const std::string &bootstrap, std::string &error,
                   bool &bootstrapped);
} // namespace lamplight

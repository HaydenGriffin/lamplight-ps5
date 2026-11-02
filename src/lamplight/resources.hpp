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
struct Picture
{
    std::string url;
    bool ok = false;
    int width = 0, height = 0;
    std::vector<std::uint8_t> rgba;
};
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

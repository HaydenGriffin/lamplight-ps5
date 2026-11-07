// Lamplight for PS5 - The controller's microphone, read on its own thread while push-to-talk is
// held. Copyright (C) 2026 Hayden Griffin SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <pthread.h>

namespace lamplight
{
// The loudest sample in a block, 0 to 1: the listening meter.
float peak_level(const std::int16_t *samples, std::size_t count);

class Microphone
{
  public:
    using Sink = std::function<void(const std::int16_t *, std::size_t)>;
    ~Microphone()
    {
        stop();
    }
    // Opens the user's microphone (16 kHz, mono, 16-bit) and gives every block to sink, on the
    // microphone's thread, until stop(). False when there is no microphone to open.
    bool start(int user, Sink sink);
    void stop();
    bool running() const
    {
        return running_;
    }
    // The last block's loudest sample, 0 to 1.
    float level() const
    {
        return level_.load();
    }

  private:
    void run();
    Sink sink_;
    int handle_ = -1;
    bool running_ = false;
    std::atomic<bool> stopping_{true};
    std::atomic<float> level_{0};
    pthread_t thread_{};
};
} // namespace lamplight

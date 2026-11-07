// Lamplight for PS5 - The controller's microphone, read on its own thread while push-to-talk is
// held. Copyright (C) 2026 Hayden Griffin SPDX-License-Identifier: GPL-3.0-or-later
#include "lamplight/microphone.hpp"
#include <algorithm>
#include <cstdlib>

#ifdef __PROSPERO__
extern "C"
{
    int sceAudioInOpen(int user, int type, int index, unsigned length, unsigned rate,
                       unsigned format);
    int sceAudioInInput(int handle, void *buffer);
    int sceAudioInClose(int handle);
}
#endif

namespace lamplight
{
#ifdef __PROSPERO__
namespace
{
constexpr unsigned kBlock = 256, kRate = 16000;
} // namespace
#endif

float peak_level(const std::int16_t *samples, std::size_t count)
{
    int peak = 0;
    for (std::size_t i = 0; i < count; ++i)
        peak = std::max(peak, std::abs(static_cast<int>(samples[i])));
    return static_cast<float>(peak) / 32768.0f;
}
bool Microphone::start(int user, Sink sink)
{
    stop();
#ifdef __PROSPERO__
    // Type 0 is the voice-chat microphone (the controller's, or a headset's); type 1 the general
    // input. Both opened in the spike (docs/microphone-spike.md).
    for (int type : {0, 1})
        if ((handle_ = sceAudioInOpen(user, type, 0, kBlock, kRate, 0)) >= 0)
            break;
    if (handle_ < 0)
        return false;
    sink_ = std::move(sink);
    stopping_ = false;
    level_ = 0;
    pthread_attr_t attr;
    if (pthread_attr_init(&attr) != 0)
    {
        sceAudioInClose(handle_);
        handle_ = -1;
        return false;
    }
    int rc = pthread_attr_setstacksize(&attr, 256 * 1024);
    if (!rc)
        rc = pthread_create(
            &thread_, &attr,
            [](void *p) -> void *
            {
                static_cast<Microphone *>(p)->run();
                return nullptr;
            },
            this);
    pthread_attr_destroy(&attr);
    running_ = rc == 0;
    if (!running_)
    {
        sceAudioInClose(handle_);
        handle_ = -1;
    }
    return running_;
#else
    (void)user;
    (void)sink;
    return false;
#endif
}
void Microphone::stop()
{
    stopping_ = true;
    if (running_)
        pthread_join(thread_, nullptr);
    running_ = false;
#ifdef __PROSPERO__
    if (handle_ >= 0)
        sceAudioInClose(handle_);
#endif
    handle_ = -1;
    level_ = 0;
}
void Microphone::run()
{
#ifdef __PROSPERO__
    // sceAudioInInput waits for each block (16 ms), so this thread paces itself.
    std::int16_t block[kBlock];
    while (!stopping_)
    {
        if (sceAudioInInput(handle_, block) < 0)
            break;
        level_ = peak_level(block, kBlock);
        if (sink_)
            sink_(block, kBlock);
    }
#endif
}
} // namespace lamplight

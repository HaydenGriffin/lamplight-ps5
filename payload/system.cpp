// Lamplight for PS5 - The payload's own clock and log, for the shared websocket code.
// Copyright (C) 2026 Hayden Griffin
// SPDX-License-Identifier: GPL-3.0-or-later
#include "platform/ps5/system.hpp"
#include <cstdarg>
#include <cstdio>
#include <ctime>
#include <ps5/klog.h>
#include <unistd.h>

namespace hui::sys
{
std::int64_t monotonic_us()
{
    timespec now{};
    clock_gettime(CLOCK_MONOTONIC, &now);
    return static_cast<std::int64_t>(now.tv_sec) * 1000000 + now.tv_nsec / 1000;
}
void log(const char *format, ...)
{
    char line[512];
    va_list arguments;
    va_start(arguments, format);
    std::vsnprintf(line, sizeof line, format, arguments);
    va_end(arguments);
    klog_printf("%s\n", line);
}
bool hide_splash_screen()
{
    return true;
}
void sleep_us(std::uint32_t microseconds)
{
    usleep(microseconds);
}
void park()
{
    for (;;)
        sleep(60);
}
void quit()
{
    _exit(0);
}
} // namespace hui::sys

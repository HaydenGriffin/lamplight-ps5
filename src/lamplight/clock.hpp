// Lamplight for PS5 - Wall-clock time: ISO 8601 from Home Assistant, local dates and their words.
// Copyright (C) 2026 Hayden Griffin
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <cstdint>
#include <string>

namespace lamplight
{
// The console's time zone: its offset from UTC at any instant, so a day that changes the clocks
// has its real midnights and an event after the change its real time. lookup is the system's
// (the kernel on the console, localtime_r on a PC); without one, the offset is fixed (previews
// and tests).
struct Zone
{
    std::int64_t fixed_s = 0;
    std::int64_t (*lookup)(std::int64_t utc_s) = nullptr;
    std::int64_t offset_s(std::int64_t utc_ms) const;
    // The UTC instant of a local time; on a skipped hour, the instant after the gap.
    std::int64_t utc_ms(std::int64_t local_s) const;
};
Zone system_zone();
// Now, as Unix milliseconds, and the zone to read it in. Everything else is worked out from
// these, so the preview and the tests set them.
struct WallClock
{
    std::int64_t utc_ms = 0;
    Zone zone;
};
WallClock wall_clock();

struct Civil
{
    int year = 1970, month = 1, day = 1, hour = 0, minute = 0, second = 0;
    int weekday = 3; // 0 Monday .. 6 Sunday
};
std::int64_t days_from_civil(int year, int month, int day);
// Breaks a count of seconds (local or UTC) into its date and time.
Civil civil(std::int64_t seconds);
// UTC milliseconds of the local midnight that starts the day holding utc_ms, and of the next
// one (days are 23 to 25 hours long where the clocks change).
std::int64_t day_start(std::int64_t utc_ms, const Zone &zone);
std::int64_t next_day(std::int64_t day_start_ms, const Zone &zone);
// The local date and time of an instant.
Civil local(std::int64_t utc_ms, const Zone &zone);

// "2026-11-02T09:30:00+01:00", "...Z", "...00.123456+00:00" or a plain date. A plain date is an
// all-day item and means local midnight, not UTC midnight. False when it is none of these.
bool parse_iso(const std::string &value, const Zone &zone, std::int64_t &utc_ms, bool &date_only);
// Milliseconds, or INT64_MIN when the text is not a time.
std::int64_t parse_iso_ms(const std::string &value, const Zone &zone);
constexpr std::int64_t kNoTime = INT64_MIN;

std::string two(int value);
// An HTTP date ("Mon, 02 Nov 2026 13:15:51 GMT") as Unix milliseconds, or kNoTime.
std::int64_t parse_http_date(const std::string &value);
// "2026-11-02T00:00:00+00:00": local time with its offset, as Home Assistant's services take it.
std::string iso_local(std::int64_t utc_ms, const Zone &zone);
// "09:05", 24-hour, as Lamplight's en-GB default.
std::string hhmm(std::int64_t utc_ms, const Zone &zone);
const char *weekday_name(int weekday);  // Monday
const char *weekday_short(int weekday); // Mon
const char *month_name(int month);      // November
const char *month_short(int month);     // Nov
// "Monday 2 November"
std::string long_date(std::int64_t utc_ms, const Zone &zone);
// "Mon 2 Nov"
std::string short_date(std::int64_t utc_ms, const Zone &zone);
} // namespace lamplight

// Lamplight for PS5 - Wall-clock time: ISO 8601 from Home Assistant, local dates and their words.
// Copyright (C) 2026 Hayden Griffin
// SPDX-License-Identifier: GPL-3.0-or-later
#include "lamplight/clock.hpp"
#include <ctime>

#ifdef __PROSPERO__
#include "platform/ps5/system.hpp"
#include <atomic>
// Not declared by the payload SDK. On the PS4 it is (time_t, time_t *, struct timesec { time_t;
// uint32 west_sec; uint32 dst_sec } *, unsigned long *): a 16-byte and an 8-byte write. Afterglow
// passed int[2] and int, and on a console every local time came out as 1 January 1970 00:00, the
// overflow overwriting the result. The buffers here are larger than any of those.
extern "C" int sceKernelConvertUtcToLocaltime(time_t utc, time_t *local, void *zone, void *dst);
#endif

namespace lamplight
{
namespace
{
std::int64_t floor_div(std::int64_t a, std::int64_t b)
{
    return (a >= 0 ? a : a - b + 1) / b;
}
std::int64_t system_offset(std::int64_t utc_s)
{
#ifdef __PROSPERO__
    alignas(16) unsigned char zone[64] = {};
    alignas(16) unsigned char dst[64] = {};
    time_t local = 0;
    const int rc = sceKernelConvertUtcToLocaltime(static_cast<time_t>(utc_s), &local, zone, dst);
    const std::int64_t offset = static_cast<std::int64_t>(local) - utc_s;
    // An offset no time zone has means the call didn't work: UTC is shown rather than nonsense.
    const bool sane = rc == 0 && offset >= -14 * 3600 && offset <= 14 * 3600 && offset % 900 == 0;
    static std::atomic<bool> logged{false};
    if (!logged.exchange(true))
    {
        std::uint32_t west = 0, summer = 0;
        __builtin_memcpy(&west, zone + 8, 4);
        __builtin_memcpy(&summer, zone + 12, 4);
        hui::sys::log(
            "[Lamplight] time zone: rc=%d utc=%lld local=%lld offset=%lld west=%u dst=%u%s", rc,
            static_cast<long long>(utc_s), static_cast<long long>(local),
            static_cast<long long>(offset), west, summer, sane ? "" : " (not used)");
    }
    return sane ? offset : 0;
#else
    const time_t at = static_cast<time_t>(utc_s);
    std::tm parts{};
    return localtime_r(&at, &parts) != nullptr ? parts.tm_gmtoff : 0;
#endif
}
} // namespace

std::int64_t Zone::offset_s(std::int64_t utc_ms) const
{
    return lookup ? lookup(floor_div(utc_ms, 1000)) : fixed_s;
}
std::int64_t Zone::utc_ms(std::int64_t local_s) const
{
    // Guess with the offset a day earlier and a day later; one of them is right except in a
    // skipped hour, where the later instant is taken.
    std::int64_t best = 0;
    bool found = false;
    for (const std::int64_t probe : {local_s - 86400, local_s + 86400})
    {
        const std::int64_t utc = local_s - offset_s(probe * 1000);
        if (utc + offset_s(utc * 1000) == local_s && (!found || utc < best))
        {
            best = utc;
            found = true;
        }
    }
    if (!found)
        best = local_s - offset_s((local_s + 86400) * 1000);
    return best * 1000;
}
Zone system_zone()
{
    return {0, &system_offset};
}
WallClock wall_clock()
{
    timespec now{};
    if (clock_gettime(CLOCK_REALTIME, &now) != 0)
        return {0, system_zone()};
    return {static_cast<std::int64_t>(now.tv_sec) * 1000 + now.tv_nsec / 1000000, system_zone()};
}

// Howard Hinnant's days_from_civil and civil_from_days: the console's libc has no gmtime_r.
std::int64_t days_from_civil(int year, int month, int day)
{
    const std::int64_t y = year - (month <= 2 ? 1 : 0);
    const std::int64_t era = (y >= 0 ? y : y - 399) / 400;
    const std::int64_t yoe = y - era * 400;
    const std::int64_t doy = (153 * (month + (month > 2 ? -3 : 9)) + 2) / 5 + day - 1;
    const std::int64_t doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return era * 146097 + doe - 719468;
}
Civil civil(std::int64_t seconds)
{
    Civil out;
    const std::int64_t days = (seconds >= 0 ? seconds : seconds - 86399) / 86400;
    const std::int64_t second = seconds - days * 86400;
    out.hour = static_cast<int>(second / 3600);
    out.minute = static_cast<int>(second % 3600 / 60);
    out.second = static_cast<int>(second % 60);
    // 1970-01-01 was a Thursday.
    out.weekday = static_cast<int>(((days % 7) + 7 + 3) % 7);
    const std::int64_t z = days + 719468;
    const std::int64_t era = (z >= 0 ? z : z - 146096) / 146097;
    const std::int64_t doe = z - era * 146097;
    const std::int64_t yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
    const std::int64_t doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
    const std::int64_t mp = (5 * doy + 2) / 153;
    out.day = static_cast<int>(doy - (153 * mp + 2) / 5 + 1);
    out.month = static_cast<int>(mp < 10 ? mp + 3 : mp - 9);
    out.year = static_cast<int>(yoe + era * 400 + (out.month <= 2 ? 1 : 0));
    return out;
}
Civil local(std::int64_t utc_ms, const Zone &zone)
{
    return civil(floor_div(utc_ms, 1000) + zone.offset_s(utc_ms));
}
std::int64_t day_start(std::int64_t utc_ms, const Zone &zone)
{
    const auto at = local(utc_ms, zone);
    return zone.utc_ms(days_from_civil(at.year, at.month, at.day) * 86400);
}
std::int64_t next_day(std::int64_t day_start_ms, const Zone &zone)
{
    // 36 hours on is always within the next local day, however long this one is.
    return day_start(day_start_ms + 36 * 3600000, zone);
}

namespace
{
bool digits(const std::string &s, std::size_t at, std::size_t count, int &out)
{
    if (at + count > s.size())
        return false;
    int v = 0;
    for (std::size_t i = at; i < at + count; ++i)
    {
        if (s[i] < '0' || s[i] > '9')
            return false;
        v = v * 10 + (s[i] - '0');
    }
    out = v;
    return true;
}
} // namespace

bool parse_iso(const std::string &s, const Zone &zone, std::int64_t &utc_ms, bool &date_only)
{
    int year = 0, month = 0, day = 0;
    if (!digits(s, 0, 4, year) || s.size() < 10 || s[4] != '-' || !digits(s, 5, 2, month) ||
        s[7] != '-' || !digits(s, 8, 2, day) || month < 1 || month > 12 || day < 1 || day > 31)
        return false;
    // 31 February is not a date, however days_from_civil would count it.
    static const int lengths[] = {31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    const bool leap = (year % 4 == 0 && year % 100 != 0) || year % 400 == 0;
    if (day > lengths[month - 1] || (month == 2 && day == 29 && !leap))
        return false;
    const std::int64_t days = days_from_civil(year, month, day);
    if (s.size() == 10)
    {
        date_only = true;
        utc_ms = zone.utc_ms(days * 86400);
        return true;
    }
    int hour = 0, minute = 0, second = 0;
    if ((s[10] != 'T' && s[10] != ' ') || !digits(s, 11, 2, hour) || s.size() < 16 ||
        s[13] != ':' || !digits(s, 14, 2, minute) || hour > 23 || minute > 59)
        return false;
    std::size_t at = 16;
    if (at < s.size() && s[at] == ':')
    {
        if (!digits(s, at + 1, 2, second) || second > 60)
            return false;
        at += 3;
    }
    int millis = 0;
    if (at < s.size() && s[at] == '.')
    {
        int scale = 100;
        for (++at; at < s.size() && s[at] >= '0' && s[at] <= '9'; ++at, scale /= 10)
            millis += (s[at] - '0') * scale;
    }
    // No zone means Home Assistant's local time, taken as the console's.
    const std::int64_t local_s = days * 86400 + hour * 3600 + minute * 60 + second;
    bool named = false;
    std::int64_t offset = 0;
    if (at < s.size())
    {
        if (s[at] == 'Z' && at + 1 == s.size())
            named = true;
        else if ((s[at] == '+' || s[at] == '-') && s.size() >= at + 3)
        {
            int zh = 0, zm = 0;
            if (!digits(s, at + 1, 2, zh))
                return false;
            std::size_t m = at + 3;
            if (m < s.size() && s[m] == ':')
                ++m;
            if ((m < s.size() && !digits(s, m, 2, zm)) || zh > 14 || zm > 59)
                return false;
            offset = (zh * 3600 + zm * 60) * (s[at] == '-' ? -1 : 1);
            named = true;
        }
        else
            return false;
    }
    date_only = false;
    utc_ms = (named ? (local_s - offset) * 1000 : zone.utc_ms(local_s)) + millis;
    return true;
}
std::int64_t parse_http_date(const std::string &value)
{
    static const char *const months[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun",
                                         "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
    // "Mon, 02 Nov 2026 13:15:51 GMT": the fields after the weekday are at fixed places.
    const auto comma = value.find(", ");
    if (comma == std::string::npos || value.size() < comma + 26)
        return kNoTime;
    const std::string s = value.substr(comma + 2);
    int day = 0, year = 0, hour = 0, minute = 0, second = 0, month = 0;
    for (int m = 0; m < 12; ++m)
        if (s.compare(3, 3, months[m]) == 0)
            month = m + 1;
    if (!month || !digits(s, 0, 2, day) || !digits(s, 7, 4, year) || !digits(s, 12, 2, hour) ||
        !digits(s, 15, 2, minute) || !digits(s, 18, 2, second) || s.compare(21, 3, "GMT") != 0)
        return kNoTime;
    return (days_from_civil(year, month, day) * 86400 + hour * 3600 + minute * 60 + second) * 1000;
}
std::int64_t parse_iso_ms(const std::string &value, const Zone &zone)
{
    std::int64_t ms = 0;
    bool date_only = false;
    return parse_iso(value, zone, ms, date_only) ? ms : kNoTime;
}
std::string two(int value)
{
    std::string out = std::to_string(value < 0 ? 0 : value);
    return out.size() < 2 ? "0" + out : out;
}
std::string iso_local(std::int64_t utc_ms, const Zone &zone)
{
    const auto offset_s = zone.offset_s(utc_ms);
    const auto at = local(utc_ms, zone);
    const auto magnitude = offset_s < 0 ? -offset_s : offset_s;
    return std::to_string(at.year) + "-" + two(at.month) + "-" + two(at.day) + "T" + two(at.hour) +
           ":" + two(at.minute) + ":" + two(at.second) + (offset_s < 0 ? "-" : "+") +
           two(static_cast<int>(magnitude / 3600)) + ":" +
           two(static_cast<int>(magnitude % 3600 / 60));
}
std::string hhmm(std::int64_t utc_ms, const Zone &zone)
{
    const auto at = local(utc_ms, zone);
    return two(at.hour) + ":" + two(at.minute);
}
const char *weekday_name(int weekday)
{
    static const char *const names[] = {"Monday", "Tuesday",  "Wednesday", "Thursday",
                                        "Friday", "Saturday", "Sunday"};
    return names[((weekday % 7) + 7) % 7];
}
const char *weekday_short(int weekday)
{
    static const char *const names[] = {"Mon", "Tue", "Wed", "Thu", "Fri", "Sat", "Sun"};
    return names[((weekday % 7) + 7) % 7];
}
const char *month_name(int month)
{
    static const char *const names[] = {"January",   "February", "March",    "April",
                                        "May",       "June",     "July",     "August",
                                        "September", "October",  "November", "December"};
    return names[(month < 1 || month > 12) ? 0 : month - 1];
}
const char *month_short(int month)
{
    static const char *const names[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun",
                                        "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
    return names[(month < 1 || month > 12) ? 0 : month - 1];
}
std::string long_date(std::int64_t utc_ms, const Zone &zone)
{
    const auto at = local(utc_ms, zone);
    return std::string(weekday_name(at.weekday)) + " " + std::to_string(at.day) + " " +
           month_name(at.month);
}
std::string short_date(std::int64_t utc_ms, const Zone &zone)
{
    const auto at = local(utc_ms, zone);
    return std::string(weekday_short(at.weekday)) + " " + std::to_string(at.day) + " " +
           month_short(at.month);
}
} // namespace lamplight

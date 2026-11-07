// Lamplight for PS5 - A PS4 title's name from its param.sfo, for the payload's game sensor.
// Copyright (C) 2026 Hayden Griffin
// SPDX-License-Identifier: GPL-3.0-or-later
#include "console.hpp"
#include <algorithm>
#include <cstring>

namespace lamplight::console
{
namespace
{
std::uint32_t le32(const std::uint8_t *p)
{
    return static_cast<std::uint32_t>(p[0]) | static_cast<std::uint32_t>(p[1]) << 8 |
           static_cast<std::uint32_t>(p[2]) << 16 | static_cast<std::uint32_t>(p[3]) << 24;
}
} // namespace

std::string sfo_title(const std::uint8_t *data, std::size_t size)
{
    // "\0PSF", version, key table, data table, entry count; then 16-byte entries.
    if (size < 20 || std::memcmp(data, "\0PSF", 4) != 0)
        return "";
    const auto keys = le32(data + 8), values = le32(data + 12), count = le32(data + 16);
    for (std::uint32_t i = 0; i < count && 20 + (i + 1) * 16 <= size; ++i)
    {
        const auto *entry = data + 20 + i * 16;
        // In 64 bits, so a hostile offset or length can't wrap past the checks.
        const std::uint64_t key =
            std::uint64_t{keys} + (static_cast<std::uint32_t>(entry[0]) | entry[1] << 8);
        const std::uint64_t length = le32(entry + 4),
                            offset = std::uint64_t{values} + le32(entry + 12);
        if (key >= size || std::strncmp(reinterpret_cast<const char *>(data + key), "TITLE",
                                        std::min<std::size_t>(6, size - key)) != 0)
            continue;
        if (offset >= size || length == 0 || offset + length > size)
            return "";
        std::string title(reinterpret_cast<const char *>(data + offset),
                          static_cast<std::size_t>(length));
        title.resize(std::strlen(title.c_str()));
        return title;
    }
    return "";
}
} // namespace lamplight::console

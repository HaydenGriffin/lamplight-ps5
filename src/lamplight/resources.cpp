// Lamplight for PS5 - Settings saved off the rendering thread, and pictures fetched and decoded.
// Copyright (C) 2026 Hayden Griffin
// SPDX-License-Identifier: GPL-3.0-or-later
#include "lamplight/resources.hpp"
#include "afterglow/artwork.hpp"
#include "core/save_file.hpp"
#include "platform/ps5/system.hpp"
#include <algorithm>
#include <cstddef>

namespace lamplight
{
Config load_config(const std::string &directory, const std::string &bootstrap, std::string &error,
                   bool &bootstrapped)
{
    // The saved settings, then lamplight.conf beside eboot.bin over them: its keys win, so the
    // settings Setup doesn't offer (a room page, Help's categories, names and icons) can be added
    // at any time with a file holding just those.
    bootstrapped = false;
    Json saved = Json::object();
    std::string data;
    if (hui::save::read_file(directory + "/settings.json", &data, 262144))
    {
        auto j = Json::parse(data, nullptr, false);
        if (j.is_object())
            saved = std::move(j);
        else
            error = "Saved settings could not be read. Enter them again in Setup.";
    }
    data.clear();
    if (hui::save::read_file(bootstrap, &data, 262144))
    {
        auto j = Json::parse(data, nullptr, false);
        if (j.is_object())
        {
            bootstrapped = true;
            return merged(Config::parse(saved), j, true);
        }
        error = "lamplight.conf could not be read; it was left out.";
    }
    return Config::parse(saved);
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
    if (requested_.contains(url))
        return;
    const auto failed = failed_.find(url);
    const bool too_soon = failed != failed_.end() && hui::sys::monotonic_us() < failed->second;
    if (!url.starts_with("http://") || url.size() > 2048 || requested_.size() >= 64 || too_soon)
    {
        // Answered, not ignored: a picture waiting for an answer holds up the next.
        Picture refused;
        refused.url = url;
        pictures_.push_back(std::move(refused));
        return;
    }
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
        {
            // Every photo and camera frame has its own address, so these can't be kept for ever.
            const auto now = hui::sys::monotonic_us();
            std::erase_if(failed_, [now](const auto &f) { return f.second <= now; });
            failed_[url] = now + 60000000;
        }
        pictures_.push_back(std::move(picture));
    }
}
bool Resources::load(const std::string &url, Picture &picture)
{
    const auto fragment = url.find('#');
    const bool wants_soft = fragment != std::string::npos && url.substr(fragment) == kSoftPicture;
    std::vector<std::uint8_t> bytes, rgba;
    int w = 0, h = 0;
    if (!hui::afterglow::fetch_image(
            url.substr(0, fragment), [&] { return stopping_.load(); }, bytes) ||
        !hui::afterglow::decode_image(bytes, rgba, w, h, 4096) || w <= 0 || h <= 0)
        return false;
    apply_orientation(rgba, w, h, jpeg_orientation(bytes));
    // At most 1920 wide and 1080 high: a camera or a photo fills the screen, no more.
    const float scale =
        std::min({1.0f, 1920.0f / static_cast<float>(w), 1080.0f / static_cast<float>(h)});
    picture.width = std::max(1, static_cast<int>(static_cast<float>(w) * scale));
    picture.height = std::max(1, static_cast<int>(static_cast<float>(h) * scale));
    picture.rgba = shrunk(rgba, w, h, picture.width, picture.height);
    if (wants_soft)
    {
        soften(picture.rgba, picture.width, picture.height, picture.soft, picture.soft_width,
               picture.soft_height);
        picture.brightness = mean_brightness(picture.soft);
    }
    return true;
}
float mean_brightness(const std::vector<std::uint8_t> &rgba)
{
    const std::size_t pixels = rgba.size() / 4;
    if (!pixels)
        return 0.5f;
    std::uint64_t total = 0;
    for (std::size_t i = 0; i < pixels; ++i)
        total += 2126u * rgba[i * 4] + 7152u * rgba[i * 4 + 1] + 722u * rgba[i * 4 + 2];
    return static_cast<float>(static_cast<double>(total) /
                              (10000.0 * 255.0 * static_cast<double>(pixels)));
}
int jpeg_orientation(const std::vector<std::uint8_t> &b)
{
    const std::size_t size = b.size();
    if (size < 4 || b[0] != 0xff || b[1] != 0xd8)
        return 1;
    std::size_t at = 2;
    // The segments before the picture itself; the EXIF one is an APP1 starting "Exif".
    while (at + 4 <= size && b[at] == 0xff)
    {
        const auto marker = b[at + 1];
        if (marker == 0xda || marker == 0xd9)
            return 1;
        const std::size_t length = static_cast<std::size_t>(b[at + 2]) << 8 | b[at + 3];
        if (length < 2 || at + 2 + length > size)
            return 1;
        const std::size_t body = at + 4;
        const std::size_t end = at + 2 + length;
        if (marker == 0xe1 && end - body >= 14 && b[body] == 'E' && b[body + 1] == 'x' &&
            b[body + 2] == 'i' && b[body + 3] == 'f' && b[body + 4] == 0 && b[body + 5] == 0)
        {
            const std::size_t tiff = body + 6;
            const bool little = b[tiff] == 'I' && b[tiff + 1] == 'I';
            if (!little && !(b[tiff] == 'M' && b[tiff + 1] == 'M'))
                return 1;
            const auto u16 = [&](std::size_t i) -> std::uint32_t
            {
                return little ? static_cast<std::uint32_t>(b[i]) | b[i + 1] << 8
                              : static_cast<std::uint32_t>(b[i]) << 8 | b[i + 1];
            };
            const auto u32 = [&](std::size_t i) -> std::uint32_t
            { return little ? u16(i) | u16(i + 2) << 16 : u16(i) << 16 | u16(i + 2); };
            const std::size_t first = u32(tiff + 4);
            if (first > end - tiff || tiff + first + 2 > end)
                return 1;
            const std::size_t directory = tiff + first;
            const std::size_t entries = u16(directory);
            for (std::size_t i = 0; i < entries; ++i)
            {
                const std::size_t entry = directory + 2 + i * 12;
                if (entry + 12 > end)
                    return 1;
                if (u16(entry) == 0x0112)
                {
                    const auto value = static_cast<int>(u16(entry + 8));
                    return value >= 1 && value <= 8 ? value : 1;
                }
            }
            return 1;
        }
        at = end;
    }
    return 1;
}
void apply_orientation(std::vector<std::uint8_t> &rgba, int &width, int &height, int orientation)
{
    if (orientation <= 1 || orientation > 8)
        return;
    const int w = width, h = height;
    const bool turns = orientation >= 5;
    const int out_w = turns ? h : w, out_h = turns ? w : h;
    std::vector<std::uint8_t> out(rgba.size());
    for (int y = 0; y < out_h; ++y)
        for (int x = 0; x < out_w; ++x)
        {
            int sx = x, sy = y;
            switch (orientation)
            {
            case 2:
                sx = w - 1 - x;
                break;
            case 3:
                sx = w - 1 - x;
                sy = h - 1 - y;
                break;
            case 4:
                sy = h - 1 - y;
                break;
            case 5:
                sx = y;
                sy = x;
                break;
            case 6: // turned a quarter clockwise to read upright
                sx = y;
                sy = h - 1 - x;
                break;
            case 7:
                sx = w - 1 - y;
                sy = h - 1 - x;
                break;
            case 8: // a quarter anticlockwise
                sx = w - 1 - y;
                sy = x;
                break;
            default:
                break;
            }
            const auto from = (static_cast<std::size_t>(sy) * static_cast<std::size_t>(w) +
                               static_cast<std::size_t>(sx)) *
                              4;
            const auto to = (static_cast<std::size_t>(y) * static_cast<std::size_t>(out_w) +
                             static_cast<std::size_t>(x)) *
                            4;
            std::copy_n(rgba.begin() + static_cast<std::ptrdiff_t>(from), 4,
                        out.begin() + static_cast<std::ptrdiff_t>(to));
        }
    rgba = std::move(out);
    width = out_w;
    height = out_h;
}
std::vector<std::uint8_t> shrunk(const std::vector<std::uint8_t> &rgba, int w, int h, int to_w,
                                 int to_h)
{
    if (to_w == w && to_h == h)
        return rgba;
    std::vector<std::uint8_t> out(static_cast<std::size_t>(to_w) * static_cast<std::size_t>(to_h) *
                                  4);
    for (int y = 0; y < to_h; ++y)
    {
        const int y0 = static_cast<int>(static_cast<std::int64_t>(y) * h / to_h);
        const int y1 =
            std::max(y0 + 1, static_cast<int>(static_cast<std::int64_t>(y + 1) * h / to_h));
        for (int x = 0; x < to_w; ++x)
        {
            const int x0 = static_cast<int>(static_cast<std::int64_t>(x) * w / to_w);
            const int x1 =
                std::max(x0 + 1, static_cast<int>(static_cast<std::int64_t>(x + 1) * w / to_w));
            std::uint32_t total[4] = {0, 0, 0, 0};
            for (int sy = y0; sy < y1; ++sy)
            {
                const auto row = static_cast<std::size_t>(sy) * static_cast<std::size_t>(w);
                for (int sx = x0; sx < x1; ++sx)
                    for (std::size_t c = 0; c < 4; ++c)
                        total[c] += rgba[(row + static_cast<std::size_t>(sx)) * 4 + c];
            }
            const auto count = static_cast<std::uint32_t>((y1 - y0) * (x1 - x0));
            const auto to = (static_cast<std::size_t>(y) * static_cast<std::size_t>(to_w) +
                             static_cast<std::size_t>(x)) *
                            4;
            for (std::size_t c = 0; c < 4; ++c)
                out[to + c] = static_cast<std::uint8_t>(total[c] / count);
        }
    }
    return out;
}
void soften(const std::vector<std::uint8_t> &rgba, int w, int h, std::vector<std::uint8_t> &soft,
            int &soft_w, int &soft_h)
{
    constexpr int kLongSide = 192, kRadius = 4, kPasses = 3;
    const float scale =
        std::min(1.0f, static_cast<float>(kLongSide) / static_cast<float>(std::max(w, h)));
    soft_w = std::max(1, static_cast<int>(static_cast<float>(w) * scale));
    soft_h = std::max(1, static_cast<int>(static_cast<float>(h) * scale));
    soft = shrunk(rgba, w, h, soft_w, soft_h);
    std::vector<std::uint8_t> line(soft.size());
    // Box blurs across then down; several in a row look like a smooth one.
    const auto blur = [&](bool across)
    {
        const int length = across ? soft_w : soft_h, lines = across ? soft_h : soft_w;
        for (int l = 0; l < lines; ++l)
            for (int i = 0; i < length; ++i)
                for (std::size_t c = 0; c < 4; ++c)
                {
                    std::uint32_t total = 0;
                    for (int d = -kRadius; d <= kRadius; ++d)
                    {
                        const int at = std::clamp(i + d, 0, length - 1);
                        const int x = across ? at : l, y = across ? l : at;
                        total +=
                            soft[(static_cast<std::size_t>(y) * static_cast<std::size_t>(soft_w) +
                                  static_cast<std::size_t>(x)) *
                                     4 +
                                 c];
                    }
                    const int x = across ? i : l, y = across ? l : i;
                    line[(static_cast<std::size_t>(y) * static_cast<std::size_t>(soft_w) +
                          static_cast<std::size_t>(x)) *
                             4 +
                         c] = static_cast<std::uint8_t>(total / (2 * kRadius + 1));
                }
        soft.swap(line);
    };
    for (int pass = 0; pass < kPasses; ++pass)
    {
        blur(true);
        blur(false);
    }
}
} // namespace lamplight

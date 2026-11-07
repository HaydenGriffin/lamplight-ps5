// Lamplight for PS5 - Now Playing: the cover large, the track, where it is, and the controls.
// Copyright (C) 2026 Hayden Griffin
// SPDX-License-Identifier: GPL-3.0-or-later
#include "lamplight/application.hpp"
#include "lamplight/paint.hpp"
#include "lamplight/room.hpp"
#include <algorithm>
#include <cmath>

namespace lamplight
{
using hui::gfx::Align;
namespace
{
// "3:07", or "1:02:07" past an hour.
std::string minutes_seconds(double seconds)
{
    const auto s = static_cast<long>(std::max(0.0, seconds));
    const auto h = s / 3600, m = s % 3600 / 60, r = s % 60;
    return (h ? std::to_string(h) + ":" + two(static_cast<int>(m)) : std::to_string(m)) + ":" +
           two(static_cast<int>(r));
}
} // namespace

void Application::draw_music(Paint &p) const
{
    const auto id = music_player();
    const auto player = music_state();
    const bool playing = player.state == "playing";
    const bool loaded = playing || player.state == "paused";
    // Together, it says so; alone, which speaker this is.
    const bool together = id == config_.speakers.group && !id.empty();
    draw_header(p, page_title(),
                (together ? speakers_words(config_, snapshot_.states) : player.name(id)) +
                    " \xC2\xB7 " + state_words(id, player),
                playing ? tone::warm : tone::muted);
    // The cover, as large as the page allows; the room's speaker when there is none.
    const Rect art{size::margin, size::content_top + 20, 660, 660};
    if (const auto *cover = shown_picture(Pic::art))
    {
        const float scale = std::min(art.w / static_cast<float>(cover->width),
                                     art.h / static_cast<float>(cover->height));
        const float w = static_cast<float>(cover->width) * scale,
                    h = static_cast<float>(cover->height) * scale;
        p.list.image(cover->texture, {art.cx() - w / 2, art.cy() - h / 2, w, h}, hui::gfx::kFullUv,
                     tone::white, size::tile_radius);
    }
    else
    {
        p.panel(art, size::tile_radius, Color::rgb(0x1c1f25, 0.82f));
        if (playing)
            p.wash(art, size::tile_radius);
        p.disc(art.cx(), art.cy(), 200, loaded ? "music" : "music-note-off",
               playing ? DiscKind::on : DiscKind::rest);
    }
    const float x = art.x + art.w + 72, w = 1920 - size::margin - x;
    const auto title = text(player.attributes, "media_title");
    const auto artist = text(player.attributes, "media_artist");
    const auto album = text(player.attributes, "media_album_name");
    float y = p.paragraph(loaded && !title.empty() ? title : "Nothing playing", x,
                          size::content_top + 100, 72, w, 84, tone::text, 2, true);
    if (!artist.empty() && loaded)
        y = p.paragraph(artist, x, y + 4, size::title, w, 54, tone::sky_muted, 1) + 6;
    if (!album.empty() && loaded)
        p.text(album, x, y, size::body, tone::muted, false, Align::left, w);
    // Where the track is: a bar and the two times, moving on while it plays.
    const double duration = number(field(player.attributes, "media_duration"));
    if (loaded && !std::isnan(duration) && duration > 0)
    {
        const double at = music_position(player);
        const Rect bar{x, 610, w, 12};
        p.list.rounded_rect(bar, 6, tone::raised2);
        p.list.rounded_rect({bar.x, bar.y, static_cast<float>(bar.w * at / duration), bar.h}, 6,
                            playing ? tone::warm : tone::muted);
        p.number(minutes_seconds(at), bar.x, 666, size::caption, tone::sky_muted);
        p.number(minutes_seconds(duration), bar.x + bar.w, 666, size::caption, tone::sky_muted,
                 Align::right);
    }
    // The controls: Previous, Play or Pause, Next.
    const bool busy = room_busy_ == id;
    const float cy = 714, pw = 248, gap = 24;
    const char *icons[] = {"skip-previous", playing ? "pause" : "play", "skip-next"};
    const char *labels[] = {"Previous", playing ? "Pause" : "Play", "Next"};
    for (int i = 0; i < 3; ++i)
    {
        const Rect r{x + static_cast<float>(i) * (pw + gap), cy, pw, 96};
        const auto kind = i != 1 ? PillKind::rest : playing ? PillKind::lead : PillKind::primary;
        p.pill(r, icons[i], labels[i], kind, i == music_focus_ && !alert_ && !sheet_,
               busy || !player.available() || (i != 1 && !loaded));
    }
    // The volume: shown as it is being set, then as Home Assistant has it.
    const double level =
        music_volume_ >= 0 ? music_volume_ : number(field(player.attributes, "volume_level"));
    if (!std::isnan(level))
    {
        const bool muted = field(player.attributes, "is_volume_muted") == true;
        const float vy = 880;
        p.icon(muted || level <= 0 ? "volume-off"
               : level < 0.4       ? "volume-low"
                                   : "volume-high",
               x + 24, vy - 12, 44, tone::sky_muted);
        const Rect bar{x + 72, vy - 18, 420, 12};
        p.list.rounded_rect(bar, 6, tone::raised2);
        p.list.rounded_rect({bar.x, bar.y, static_cast<float>(bar.w * level), bar.h}, 6,
                            music_volume_ >= 0 ? tone::accent : tone::text);
        p.number(std::to_string(static_cast<int>(std::lround(level * 100))) + "%",
                 bar.x + bar.w + 24, vy, size::label, tone::sky_muted);
    }
}
} // namespace lamplight

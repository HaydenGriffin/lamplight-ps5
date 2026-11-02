<p align="center">
  <img src="docs/images/hero.png" alt="Lamplight for PS5: the clock on a TV, the Weather page, the Living room page with lights glowing warm, and a Home Assistant notification shown by the PS5 over its home screen" width="100%">
</p>

# Lamplight for PS5

[Lamplight](https://github.com/HaydenGriffin/lamplight)'s Home Assistant pages on your TV, from the PS5: a clock,
your living room, the weather, the household's agenda, timers, the guest Wi-Fi code, "What can I say?" and the
assistant's last answer. It is a native PS5 title for jailbroken consoles, drawn with the same colours, type and
rules as the wall tablets, and driven with the DualSense. A small background payload also brings Home Assistant's
notifications to the PS5, over any game, and tells Home Assistant how the console is.

> **Status:** run on one PS5 (system software 7.20) against a real Home Assistant on 2 November 2026: every page,
> Setup, the Living room page switching a real light, a View Assist timer with Undo and its finished alert, an
> assistant question, the resting clock, and notifications from Home Assistant all worked there. Summer time,
> waking from rest mode, typing a question, scanning the Wi-Fi code and long sessions haven't been checked yet. See
> [console validation](docs/console-validation.md). The pictures below are drawn by Lamplight's own UI code from a
> made-up house; the notification in the one above was captured from the console.

| The clock | Living room |
|---|---|
| <img src="docs/images/home.png" alt="The clock: 14:05 large, 12 degrees and partly cloudy, Monday 2 November, and a chip for each page: Living room (Harvest Moon), Weather, Agenda (now: Dentist), Timers (12 min 21 s left), Wi-Fi, Help and Assistant"> | <img src="docs/images/room.png" alt="Living room: Harvest Moon by Neil Young playing on the speaker with Pause and Next, actions Gaming mode, Movie time, Lights off and Good night, and tiles for the lights (two on, glowing warm), the radiator, the blinds and an air purifier"> |

| Weather | A timer has finished |
|---|---|
| <img src="docs/images/weather.png" alt="Weather: 12 degrees and partly cloudy, wind, humidity and UV, the next 12 hours as a temperature line with rain bars, and the next five days"> | <img src="docs/images/alert.png" alt="Over the clock, a dimmed screen and one panel: a glowing timer disc, Pasta, Time's up, and a Stop button"> |

| Agenda | Timers, just after cancelling one |
|---|---|
| <img src="docs/images/agenda.png" alt="Agenda: today and tomorrow side by side; Dentist is on now and glows warm, School run and Stand-up are past, Swimming lesson is cancelled"> | <img src="docs/images/timers-undo.png" alt="Timers: a pasta timer with its ring, an alarm for 07:00 tomorrow, a paused timer, the washing machine, quick-start buttons, and a 'Pasta cancelled' toast with Undo on Triangle"> |

| What can I say? | Wi-Fi |
|---|---|
| <img src="docs/images/help-reply.png" alt="Help: categories on the left, examples starting with Okay Nabu on the right, and the assistant's reply in a toast"> | <img src="docs/images/wifi.png" alt="Wi-Fi: a large QR code on white, the network name and three steps to join"> |

## What it does

- **The clock** is home: the time large, the outdoor temperature and the date, and a chip for each page with what
  it would tell you now (what's playing, what's on, the next timer). The sky behind it follows the weather and the
  sun. Left alone, every page goes back to the clock after five minutes, and two minutes later the clock rests: the
  chips go, everything dims and drifts a few pixels over minutes, and late at night it is grey on near-black. An
  OLED keeps no picture of it. The first press wakes it.
- **Living room** (or any room): what the room's speaker is playing, with Play and Next; the room's scripts and
  scenes as actions (Good night and the like ask for a second press); its lights, switches, blinds and radiator as
  tiles that glow when on. Set in `lamplight.conf` (below).
- **Weather:** now, the next 12 hours as a temperature line with rain, and the next five days. The forecast is
  fetched every 20 minutes; if it can't be, the page says so, offers Retry and tries again a minute later, and a
  forecast it already has stays on screen.
- **Agenda:** today and tomorrow from your calendars, side by side. What's on now glows; earlier events are dimmed;
  cancelled ones are struck through. Refreshed every 10 minutes, at midnight and when a calendar changes.
- **Timers:** a View Assist device's timers, paused timers, alarms and reminders, and appliance timers (a timer
  helper with `finishes_at`), with quick-start buttons. Cancelling a running timer offers Undo for 8 seconds;
  alarms, reminders and paused timers ask for a second press instead. When a timer or an appliance finishes, an
  alert takes over the screen and the controller's light bar pulses; Stop also stops View Assist ringing.
- **Wi-Fi:** a QR code for the guest network, made on the console. The password is only ever shown inside the
  code.
- **What can I say?** Voice examples by category, each starting with your wake phrase. Choosing one runs it for
  real through your assistant and shows the reply; a follow-up within 90 seconds continues the conversation.
  Triangle types a question with the PS5 keyboard. A visitors switch stops anything running.
- **Assistant:** the assistant's last answer, large, from the View Assist device or asked here. Square keeps it on
  the View Assist screen.

- **Home Assistant's time.** A console's clock can be minutes out (this one was three fast); Lamplight measures it
  against Home Assistant's and shows the house's time, so countdowns are right.
- **Waking from rest mode,** it reconnects at once and fetches everything again.

## Controls

| Control | Action |
|---|---|
| D-pad / left stick | Move; on the clock, choose a page |
| Cross | Open, select, run an example |
| Circle | Back to the clock (on the clock: close Lamplight) |
| L1 / R1 | Previous and next page |
| Triangle | Undo (Timers); type a question (What can I say?) |
| Square | Keep the answer on screen (Assistant) |
| Cross / Circle | Stop / dismiss a finished timer's alert |
| Options | Setup |

## Requirements

- **A jailbroken PS5 that runs folder homebrew** from `/data/homebrew/<TITLE_ID>` (an enabler such as ShadowMount
  Plus) and an FTP server. This guide does not explain jailbreaking.
- **Home Assistant on your local network at a numeric IPv4 address over plain HTTP** (for example
  `192.168.1.10:8123`). The PS5 does not resolve names such as `homeassistant.local`, and HTTPS is not supported:
  that includes an answer's picture at an `https://` address, which is left out.
- For Timers and the assistant's answers, [View Assist](https://github.com/dinki/View-Assist) is optional:
  without it, Timers shows appliance timers only.

## Install

Build it (below), or take `PPSA99484.zip` from a release and check it against `SHA256SUMS`. Copy the `PPSA99484`
folder to `/data/homebrew/` over FTP, so you have `/data/homebrew/PPSA99484/eboot.bin`. From a checkout,
`make deploy PS5_HOST=<console address>` uploads it, publishing `eboot.bin` and `param.json` last. Check that no
other homebrew on your console uses `PPSA99484` (Afterglow is `PPSA99481`, Prism `PPSA99483`).

Notifications are a separate, optional part: see [Notifications on the PS5](#notifications-on-the-ps5).

## Set up

Open Lamplight; with no settings it starts in Setup (Options opens it any time).

1. **Home Assistant address**, **Port** and **Access token** (a long-lived token from your Home Assistant
   profile; a non-admin user is enough for the app, though the notifications payload needs an administrator's),
   then **Connect / retry**.
2. **Weather**, **Calendars**, **View Assist device**, **Appliance timers**, **Assistant** and **Visitors switch**
   are lists of what your Home Assistant has.
3. **Wake phrase** and **Room** (it fills `{room}` in the examples), and the **Guest Wi-Fi network** and
   **password**.

**From a computer:** put a `lamplight.conf` beside `eboot.bin` (start from `lamplight.conf.example`). Lamplight reads
it at every start, and its keys win over what is saved, so it also adds what Setup doesn't: a **room page**
(`room_page`: the room's speaker, its lights and other things as tiles, its scripts and scenes as actions), each
calendar's and appliance's `name` and `icon` (`mdi:` names from the icon list in `tools/make-fonts.py`),
`quick_minutes`, and Help's `categories` in the same shape as the Help card's YAML (as JSON). A file with only the keys
you want to change is enough. Once Setup shows **Saved on this PS5**, delete it: anyone on your network can read the
install folder over FTP.

**Where secrets live.** The token and the Wi-Fi password are kept in `/download0/lamplight/settings.json` (mode
`0600`) in plain text, and Home Assistant's local HTTP sends the token unencrypted. Use a trusted network.
**Disconnect and forget token** removes it from the PS5; revoke it in Home Assistant to cancel it.

## Notifications on the PS5

`lamplight-notify.elf` is a small payload that runs in the background from boot. It keeps a connection to Home
Assistant, shows the event `lamplight_ps5_notify` (`{title, message}`) as a PS5 system notification over whatever is
on screen, games included, and once a minute fires `lamplight_ps5_status` with the console's SoC and CPU
temperatures, free space and uptime. It opens no port to the network (the one it binds, on 127.0.0.1, only stops a
second copy starting) and uses about 4 MB.

1. Copy `lamplight-notify.elf` (in the release, or `dist/` after `make payload`) to the console, for example to
   `/data/lamplight/`.
2. Put `/data/lamplight/notify.json` beside it: `{"host": "192.168.1.10", "port": 8123, "token": "…"}`. The token
   must be an **administrator's**: Home Assistant lets only administrators subscribe to an event of their own and
   fire one, and the payload stops (and says so in the kernel log) when it is refused.
3. Start it at boot: add a line for it to your autoloader's list (with the ELF beside the list), or send it to an
   ELF loader on port 9021 once. Lamplight itself must be installed for the notification's icon.
4. In Home Assistant, add [`homeassistant/lamplight_ps5.yaml`](homeassistant/lamplight_ps5.yaml) as a package. It
   gives `binary_sensor.ps5_lamplight` (on while the PS5 reports), the temperature, free-space and up-since sensors,
   and `script.ps5_notify`:

   ```yaml
   action: script.ps5_notify
   data: {title: Washing machine finished, message: Laundry}
   ```

   For a notification only when the PS5 is on the TV, check your TV's input in the automation that calls it.

## Build and test

With Docker on Linux or a Mac:

```sh
docker build -t lamplight-ps5-builder .
docker run --rm -v "$PWD:/work" lamplight-ps5-builder make lint test
docker run --rm -v "$PWD:/work" lamplight-ps5-builder make preview
docker run --rm -v "$PWD:/work" lamplight-ps5-builder make app
```

- `make app` builds `dist/PPSA99484/` and `dist/PPSA99484.zip`. Credentials are never included.
- `make payload` builds `dist/lamplight-notify.elf` with the PS5 payload SDK.
- `make test` runs the host tests under AddressSanitizer and UndefinedBehaviorSanitizer (the models, the app driven
  by `tests/fixture.json`, days when the clocks change) and the native client against a fake Home Assistant.
- `make preview` renders every page from the fixture into `build/preview/`, with the console's UI code and shaders.
- `make lint` checks formatting, source notices, the package identity, the assets and the shell scripts.
- `make fonts` rebakes `assets/fonts/`. `tools/make-fonts.py <lamplight checkout>` (fontTools, brotli and
  skia-pathops) first makes the source fonts: Lexend 400 and the icon font.

**Game or media.** Remote Play streams a game's picture but shows a media app as black, so during development
the title is a game. `python3 tools/category.py media` makes it a media app for a release (it then sits with the
media apps), and `tools/check.py --release` refuses a release that isn't. Re-register the title on the console
after changing it.

## Source layout

`src/lamplight/`: `client.*` is the Home Assistant session; `config`, `states`, `clock`, `weather`, `agenda`,
`timers`, `room`, `assistant` and `wifi_code` are the models; `application.cpp`, `data.cpp`, `actions.cpp`,
`alerts.cpp` and `setup.cpp` are behaviour; `paint.*` is Lamplight's design system at TV scale; `view*.cpp` draw the
pages. `payload/` is the notification payload; `homeassistant/` the Home Assistant package. `src/main.cpp`
connects the console's display, controller and keyboard. `host/` holds the PC preview and the pretend Home
Assistant the tests share.

## Licence

GPL-3.0-or-later, because it reuses Afterglow's and BlackBearReloaded's PS5 infrastructure; see [LICENSE](LICENSE),
[UPSTREAM.md](UPSTREAM.md) and the [third-party notices](THIRD_PARTY_NOTICES.md). Lamplight itself is MIT. No Sony
SDK, firmware or keys are included. Lamplight is independent of Sony and Home Assistant.

<p align="center">
  <img src="docs/images/hero.png" alt="Lamplight for PS5: the clock on a TV, a light's sheet with its scenes, brightness and warmth sliders and colours, the Living room page with lights glowing warm, and a Home Assistant notification shown by the PS5 over its home screen" width="100%">
</p>

# Lamplight for PS5

[Lamplight](https://github.com/HaydenGriffin/lamplight)'s Home Assistant pages on your TV, from the PS5: a clock,
your rooms, with sliders for every light and a sheet of controls for everything else, what's playing, the weather,
the household's agenda, timers, the guest Wi-Fi code, "What can I say?" and the assistant's last answer. Hold R2 and speak to ask Home Assistant's Assist through the controller's microphone;
the house's notices have their own page and pop up as they come; when the doorbell rings, its camera fills the
screen; while nobody is using it, the clock rests under a live sky (stars and the moon, clouds, rain) or your photos. It
is a native PS5 title for jailbroken consoles, drawn with the same colours, type and rules as the wall tablets, and
driven with the DualSense. A small background payload makes the PS5 a Home Assistant device too: notifications over
any game, the game being played, the console's temperatures, and commands to open a game or rest.

> **Status:** run on one PS5 (system software 7.20) against a real Home Assistant. 1.0 on 2 November 2026: every
> page, Setup, the Living room page switching a real light, a View Assist timer with Undo and its finished alert, an
> assistant question, the resting clock, and notifications from Home Assistant (over a game too). 1.1 on 6 November:
> sheets and sliders, scenes, charts, warm-ups, House controls, Speakers, the slideshow and photo backgrounds, with
> memory steady over a 16-minute slideshow. Not yet run for real: Everything off, Good night, Away, Night mode, Play
> everywhere and Move here, and the rest command since its fix. [Console validation](docs/console-validation.md) has
> the detail. 1.1.0 is the latest release. The tour and pictures below are drawn by Lamplight's own UI code from a
> made-up house; the notification in the one above was captured from the console.

<p align="center">
  <img src="docs/images/tour.gif" alt="A tour of Lamplight, a button press at a time: the clock, the Living room, a light's sheet with its sliders and scenes, a radiator's chart, Now Playing and Speakers, the notices, Weather, Agenda and Timers, House controls, the doorbell's camera, the slideshow, a question to the assistant, and the resting clock under a night sky" width="100%">
</p>

| The clock | Living room |
|---|---|
| <img src="docs/images/home.png" alt="The clock: 14:05 large, 12 degrees and partly cloudy, Monday 2 November, and a chip for each page: Living room (3 on), Music (playing), House (2 notices, in orange), Weather, Agenda (now), Timers (13 min), Wi-Fi, Help and Assistant"> | <img src="docs/images/room.png" alt="Living room: Harvest Moon by Neil Young playing on the speaker with Pause and Next, actions Gaming mode, Movie time, Lights off, Good night and Everything off, and tiles for the lights (two on, glowing warm), the radiator at 20.5 degrees, the blinds and an air purifier"> |

| A light's controls | The slideshow |
|---|---|
| <img src="docs/images/sheet-light.png" alt="Over the dimmed Living room, the Main lights sheet: Power on, the scenes Bright, Relax, Cinema and Off, Brightness at 55 percent, Warmth at 2700 K and nine colour swatches"> | <img src="docs/images/slideshow.png" alt="A dusk photo of mountains and a lake fills the screen, with the time, date and temperature over a dark gradient and the slideshow's controls below"> |

| Music | Hold R2 and speak |
|---|---|
| <img src="docs/images/music.png" alt="Now playing: the cover large, Harvest Moon by Neil Young, a progress bar at 1:58 of 5:02, Previous, Pause and Next, and the volume at 32 percent"> | <img src="docs/images/talk-thinking.png" alt="The clock with a panel between the time and the chips: a microphone, 'turn on the fireplace' in quotes, and Thinking"> |

| A thermostat: its day, and a quick warm-up | House controls, from any page |
|---|---|
| <img src="docs/images/sheet-climate.png" alt="Over the dimmed Living room, the Radiator sheet at 20.5 degrees: a chart of the last 24 hours, and Warm up buttons for 21 degrees for 2 hours and Stop"> | <img src="docs/images/sheet-house.png" alt="The House controls sheet: Everything off and Good night buttons, then Guests, Cinema, Away and Night mode as switches, each saying what it does"> |

| Speakers | The resting clock in the rain |
|---|---|
| <img src="docs/images/sheet-speakers.png" alt="The Speakers sheet: Play everywhere, the kitchen's music with Move here, a sleep timer, and each room's volume"> | <img src="docs/images/home-mood-rain.png" alt="The resting clock, dimmed, with faint rain falling behind it"> |

| House: the notices up now | The resting clock at night |
|---|---|
| <img src="docs/images/house.png" alt="House: two notices, Front door open (Hallway, open for 2 minutes, 15 min ago, orange) and The dishwasher is clean (Kitchen, 45 min ago), with Dismiss on Cross"> | <img src="docs/images/home-mood-night.png" alt="The resting clock at 21:40, dimmed, under stars and a half moon"> |

| The doorbell | The resting clock over a photo |
|---|---|
| <img src="docs/images/doorbell.png" alt="A doorbell camera's picture fills the screen, with a banner: Someone's at the door, Front door, rang at 14:05"> | <img src="docs/images/home-photo.png" alt="The resting clock, dimmed, over a dusk photo of mountains and a lake"> |

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
- **Rooms:** what each room's speaker is playing, with Play and Next; the room's scripts and scenes as actions
  (Good night and the like ask for a second press); its lights, switches, blinds, radiator and sensors as tiles
  that glow when on. Up to six rooms; Square steps through them. Set in `lamplight.conf` or from Home Assistant
  (below).
- **Every thing's controls:** a tap of Cross presses a tile; hold it for half a second, or press Triangle, and its
  sheet opens with what the thing says it supports. A light has brightness, warmth, colour and effects, and a group
  a level for each of its lamps; a thermostat its target, mode, preset and fan; blinds their position and tilt; a
  fan its speed; a speaker its volume, mute, source and shuffle; locks (Unlock takes two presses), vacuums,
  humidifiers, numbers and lists theirs. Sliders step on the D-pad and glide on the left stick, and each change
  reaches Home Assistant as you move. Triangle on an action opens every lamp in the room, with one slider for them
  all, and the right stick sets the focused tile's level from the page.
- **Scenes:** a room's scenes sit at the top of its main light's sheet and its All lamps sheet: Bright, Relax, Off,
  or a script of your own such as Cinema, with the one in force lit.
- **Charts and warm-ups:** a thermostat's sheet and a number sensor's show the last 24 hours, with the temperature
  it is set to as a line across. A tile's `boosts` put buttons such as 21° for 2 hours in its sheet, and the house's
  own script can answer in its own words. A thermostat with `boosts` is taken to be run by your heating plan: its
  tile and sheet read the room `sensor`, and nothing sets its target by hand, so the sheet is the chart and those
  buttons.
- **House controls:** L2 from any page: Everything off and Good night (two presses, then Undo puts the lamps back as
  they were), and each of the house's modes as a switch: Guests, Cinema, Away, Night. With visitors mode on, only
  the way out of it runs from here.
- **Cameras:** a `camera.` tile fills the screen, a new picture each second, for up to five minutes.
- **Music:** the room's player large: the cover, the track, where it is, Previous, Play or Pause and Next, and the
  volume on Up and Down. A speaker playing with its group is the group here and on the room's page, as on the
  tablets, so Pause and the volume reach every room.
- **Speakers:** Square on Now Playing: Play everywhere or Only this room, Move here for what's playing in another
  room, a sleep timer, and every room's volume, where Cross joins it to the group or takes it out. Moving music
  needs Music Assistant.
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

- **Push-to-talk:** hold R2 on any page and speak; let go, and Home Assistant's Assist hears it through the
  controller's microphone (or a headset's), does it, and answers on screen. It runs your Assist pipeline (its
  speech-to-text and agent), continues the conversation for five minutes, and does nothing in visitors mode.
- **The doorbell:** when it rings (a binary sensor turning on, or an event entity), its camera fills the screen
  for a minute, a new picture each second, over any page. Circle closes it.
- **House:** the notices Home Assistant has up (any entity whose `items` attribute lists them: title, message,
  icon, level, rooms, time), newest first, with a chip on the clock. A new one pops up as it arrives, and wakes the
  resting clock; an alert-level one takes the screen like a finished timer. Cross dismisses one, through an event
  you name (it needs an administrator's token).
- **The resting clock's sky:** stars and the moon in its phase at night, slow clouds by day, rain and snow as the
  weather has them, all faint and slow; Setup's Resting clock row turns it off.
- **Photos:** Triangle on the clock opens a slideshow of a Home Assistant media folder and the folders inside it
  (local media, or Immich and the like through their integrations), shuffled, each picture fading into the next
  with a slow drift. Portrait photos show whole over their own blur, the right way up. Left and Right step (a press
  while the next is still loading waits its turn), Cross
  pauses (for up to a quarter of an hour, for an OLED's sake), Square hides the clock, and L1 and R1 set how long each stays. Choose the
  folder in Setup. Late at night the slideshow dims too, and its clock goes quieter when it's left alone. While the
  clock rests the same pictures sit behind it, dimmed, and much dimmer late at night.
- **Backgrounds:** Setup's Background row puts your photos, or the room picture from View Assist, behind the clock
  and, blurred, behind the pages. `background` can also name one picture, and each room's `photo` one of its own.
  Now Playing has the cover blurred behind it.
- **12 or 24 hours:** Automatic follows Home Assistant's country (12 hours in the US, Canada, Australia and
  others); Setup's Clock row sets it either way. The forecast's units are Home Assistant's.
- **Home Assistant's time.** A console's clock can be minutes out (this one was three fast); Lamplight measures it
  against Home Assistant's and shows the house's time, so countdowns are right.
- **Waking from rest mode,** it reconnects at once and fetches everything again.

## Controls

| Control | Action |
|---|---|
| D-pad / left stick | Move; on the clock, choose a page |
| Cross | Open, select, run an example; on a room's tile, press it |
| Cross (hold) | A room's tile: its controls |
| Right stick | A level: the focused tile's, a sheet's slider, the volume on Music |
| Circle | Back to the clock (on the clock: close Lamplight) |
| L1 / R1 | Previous and next page |
| L2 | House controls: Everything off and the modes, from any page |
| R2 (hold) | Talk to the assistant; let go when you've finished |
| Triangle | Controls (rooms, Music); the slideshow (the clock); Undo (Timers); type a question (What can I say?) |
| Square | The next room (rooms); Speakers (Music); keep the answer on screen (Assistant) |
| Cross (House) | Dismiss the notice |
| Up / Down | Volume (Music) |
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
- For push-to-talk, an Assist pipeline with speech-to-text (Home Assistant Cloud, or Whisper and the like). Cameras
  and photos must be JPEG or PNG at most 4096 pixels a side.

## Install

Build it (below), or take `PPSA99484.zip` from a release and check it against `SHA256SUMS`. Copy the `PPSA99484`
folder to `/data/homebrew/` over FTP, so you have `/data/homebrew/PPSA99484/eboot.bin`. From a checkout,
`make deploy PS5_HOST=<console address>` uploads it, publishing `eboot.bin` and `param.json` last. Check that no
other homebrew on your console uses `PPSA99484` (Afterglow is `PPSA99481`, Prism `PPSA99483`).

The background payload is a separate, optional part: see [The PS5 in Home Assistant](#the-ps5-in-home-assistant).

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
it at every start, and its keys win over what is saved, so it also adds what Setup doesn't: **rooms** (`rooms`: each
room's speaker, its lights and other things as tiles, its scripts and scenes as actions; 1.0's single `room_page`
is still read; a room's `photo` is a picture behind its page), the **doorbell** (`doorbell`: the `trigger` that
rings and the `camera`), **photos** (`photos`: a `media-source://` folder; `photo_seconds`: how long each stays) and
the **`background`** (`sky`, `photos`, `room`, a Home Assistant path such as `/local/wall.jpg`, or an entity with a
`background` attribute), the house's **notices** (`notices`: the `entity` that lists them and the
`dismiss_event`), `mood` (`false` for a plain resting clock), the Assist `pipeline` for push-to-talk, each calendar's and appliance's
`name` and `icon` (`mdi:` names from the icon list in `tools/make-fonts.py`), `quick_minutes`, and Help's
`categories` in the same shape as the Help card's YAML (as JSON). A room's `light` and `scenes`, a tile's `sensor`
(what its chart reads) and `boosts`, the house's `house` (`actions`, `modes` and `undo_lights`) and the `speakers`
(`group`, `rooms` and `sleep_script`, which takes `{player, minutes}`) are in `lamplight.conf.example`. A service in
these settings is called exactly as written, and nothing else is let through: changing a boost's temperature means
changing the settings. A file with only the keys you want to change is enough. Once Setup shows **Saved on this PS5**, delete it: anyone on your network can read the install folder over
FTP.

**From Home Assistant:** the same settings can live in Home Assistant instead, so you never copy a file. Lamplight
watches `sensor.lamplight_ps5` (`settings_entity` names another, `""` turns it off) and applies its `lamplight`
attribute over its own settings whenever it changes; the address and token always stay the PS5's. The package in
[`homeassistant/lamplight_ps5.yaml`](homeassistant/lamplight_ps5.yaml) makes the entity, with the settings as a
template attribute: edit them there and reload templates.

**Where secrets live.** The token and the Wi-Fi password are kept in `/download0/lamplight/settings.json` (mode
`0600`) in plain text, and Home Assistant's local HTTP sends the token unencrypted. Use a trusted network.
**Disconnect and forget token** removes it from the PS5; revoke it in Home Assistant to cancel it.

## The PS5 in Home Assistant

`lamplight-notify.elf` is a small payload that runs in the background from boot. It keeps a connection to Home
Assistant and:

- shows the event `lamplight_ps5_notify` (`{title, message}`) as a PS5 system notification over whatever is on
  screen, games included;
- fires `lamplight_ps5_status` once a minute, and at once when the game in front changes, with the game (its title
  ID and name), the console's SoC and CPU temperatures, free space and uptime;
- takes `lamplight_ps5_command`: `{command: launch, title_id: PPSA07923}` opens a game or app,
  `{command: lamplight}` opens Lamplight, and `{command: rest}` puts the PS5 in rest mode.

It opens no port to the network (the one it binds, on 127.0.0.1, only stops a second copy starting) and uses about
4 MB.

1. Copy `lamplight-notify.elf` (in the release, or `dist/` after `make payload`) to the console, for example to
   `/data/lamplight/`.
2. Put `/data/lamplight/notify.json` beside it: `{"host": "192.168.1.10", "port": 8123, "token": "…"}`. The token
   must be an **administrator's**: Home Assistant lets only administrators subscribe to an event of their own and
   fire one, and the payload stops (and says so in the kernel log) when it is refused.
3. Start it at boot: add a line for it to your autoloader's list (with the ELF beside the list), or send it to an
   ELF loader on port 9021 once. Lamplight itself must be installed for the notification's icon.
4. In Home Assistant, add [`homeassistant/lamplight_ps5.yaml`](homeassistant/lamplight_ps5.yaml) as a package. It
   gives `binary_sensor.ps5_lamplight` (on while the PS5 reports), `sensor.ps5_game` and `binary_sensor.ps5_playing`,
   the temperature, free-space and up-since sensors, Lamplight's settings entity, and the scripts
   `script.ps5_notify`, `script.ps5_launch`, `script.ps5_open_lamplight` and `script.ps5_rest`:

   ```yaml
   action: script.ps5_notify
   data: {title: Washing machine finished, message: Laundry}
   ```

   For a notification only when the PS5 is on the TV, check your TV's input in the automation that calls it. With
   `binary_sensor.ps5_playing`, lights can dim when a game starts; `script.ps5_rest` can end the evening.

   Rest mode on a jailbroken console is at your own risk: some homebrew setups don't survive it.

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
- `make tour` (outside the builder, with Docker and ffmpeg) plays `host/tour.cpp`, a script of button presses against
  the fixture, into `build/tour/` and `docs/images/tour.gif`. `tools/hero.py` and `tools/site-images.py` draw the
  promotional pictures from the preview's.
- `make lint` checks formatting, source notices, the package identity, the assets and the shell scripts.
- `make fonts` rebakes `assets/fonts/`. `tools/make-fonts.py <lamplight checkout>` (fontTools, brotli and
  skia-pathops) first makes the source fonts: Lexend 400 and the icon font.

**Game or media.** Remote Play streams a game's picture but shows a media app as black, so during development
the title is a game. `python3 tools/category.py media` makes it a media app for a release (it then sits with the
media apps), and `tools/check.py --release` refuses a release that isn't. Re-register the title on the console
after changing it.

## Source layout

`src/lamplight/`: `client.*` is the Home Assistant session; `config`, `states`, `clock`, `weather`, `agenda`,
`timers`, `room`, `controls`, `assistant` and `wifi_code` are the models; `application.cpp`, `data.cpp`,
`actions.cpp`, `sheet.cpp`, `photos.cpp`, `alerts.cpp` and `setup.cpp` are behaviour; `paint.*` is Lamplight's design system at TV scale; `view*.cpp` draw the
pages. `payload/` is the notification payload; `homeassistant/` the Home Assistant package. `src/main.cpp`
connects the console's display, controller and keyboard. `host/` holds the PC preview, the tour and the pretend Home
Assistant they share with the tests.

## Licence

GPL-3.0-or-later, because it reuses Afterglow's and BlackBearReloaded's PS5 infrastructure; see [LICENSE](LICENSE),
[UPSTREAM.md](UPSTREAM.md) and the [third-party notices](THIRD_PARTY_NOTICES.md). Lamplight itself is MIT. No Sony
SDK, firmware or keys are included. Lamplight is independent of Sony and Home Assistant.

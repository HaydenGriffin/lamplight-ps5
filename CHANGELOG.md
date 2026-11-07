# Changelog

## 1.1.0 (unreleased)

- **Push-to-talk:** hold R2 on any page and speak; Home Assistant's Assist hears it through the controller's
  microphone, does it and answers on screen, continuing the conversation for five minutes.
- **Rooms:** up to six, Square steps through them; sensors as tiles, with their units.
- **Music:** Now Playing for the room's player, with the cover, progress, Previous, Play or Pause, Next and the
  volume.
- **The doorbell:** its camera fills the screen for a minute when it rings, a new picture each second.
- **House:** the house's notices on their own page and chip, popping up as they come (an alert one takes the
  screen), dismissed with Cross through an event of your choosing.
- **The resting clock's sky:** stars and the moon in its phase, clouds, rain and snow, drawn live and faint.
- **Controls for everything:** hold Cross on a tile, or press Triangle, for its sheet: brightness, warmth, colour and
  effects on a light, and each lamp in a group; target, mode, preset and fan on a thermostat; position and tilt on
  blinds; speed on a fan; volume, mute, source and shuffle on a speaker; locks, vacuums, humidifiers, numbers and
  lists. Sliders step on the D-pad and glide on the left stick. Triangle on an action opens every lamp in the room,
  with one slider for them all, and the right stick sets the focused tile's level from the page.
- **Scenes:** a room's scenes in its main light's sheet and its All lamps sheet, with the one in force lit.
- **Charts:** a thermostat's and a number sensor's last 24 hours in their sheet, with the target as a line across.
- **Warm-ups:** a tile's `boosts` are quick buttons in its sheet, each exactly the service call the settings name. A
  thermostat with them is taken to be run by the house's plan: its tile and sheet read the room `sensor`, and nothing
  sets its target by hand.
- **House controls:** L2 from any page: Everything off and Good night, two presses then Undo for the lamps, and the
  house's modes as switches. Visitors mode leaves only its own way out.
- **Speakers:** Square on Now Playing: Play everywhere, Only this room, Move here, a sleep timer, and every room's
  volume and its place in the group. A speaker playing with its group is the group everywhere in Lamplight.
- **Slideshow:** Triangle on the clock: a Home Assistant media folder and the folders inside it, shuffled, fading
  with a slow drift; portrait photos whole over their own blur, the right way up. Pause, step, hide the clock, and
  L1 and R1 for how long each stays. Setup chooses the folder and the time. Late at night it dims, and its clock goes
  quieter when it is left alone.
- **Backgrounds:** photos, a room's picture, or one picture of your own behind the clock and, blurred, behind the
  pages; the cover behind Now Playing.
- **Camera tiles:** a `camera.` tile fills the screen, a new picture each second.
- **The photo frame:** the resting clock over those photos, dimmed.
- **12 or 24 hours,** automatic by Home Assistant's country or set in Setup; the forecast's rain in its own unit.
- **Settings from Home Assistant:** an entity's `lamplight` attribute (a template sensor in the package) applies
  over the PS5's settings, so nothing has to be copied to the console.
- **The PS5 as a Home Assistant device:** the payload reports the game in front as soon as it changes, and takes
  commands to open a game, open Lamplight or rest; the package adds `sensor.ps5_game`, `binary_sensor.ps5_playing`
  and the scripts for them.
- Pictures up to 4096 pixels a side are accepted (shown at most 1920 by 1080).
- From the first console run of 1.1.0: a room with five or six actions keeps its third row above the hints (the
  speaker card gives up a row); temperatures and percentages read to a tenth ("16.5 °C", not 16.53); a slider at rest
  reads what the thing reports (73%, not the step's 75%); a toast over a sheet sits at the bottom, clear of its
  title, and a script's own sentence stays 4.5 s; the Speakers sheet never opens on Move here, and its sleep timer
  names its room and keeps the choice lit while it runs; a chart's hours are whole ones; All lamps doesn't count a
  group whose lamps are tiles too; 12 more icons from the house's tablets, the kitchen's knife and fork among them.
- From a review of the whole of 1.1.0: a second press doesn't carry over to a Help example that settings from Home
  Assistant have replaced, and Help can't read past a list they've shortened; Undo for the lamps works on Help too, and
  is offered once the house's action has finished, so a slow script can't outlast it or undo it; Off and scenes beat
  levels still waiting on the lamps under them; a room's speaker controls reach its group while it plays with one; Only
  this room and Move here say they worked once the music has moved, not because something was already playing; a photo
  folder that has been emptied stops showing; a cover that couldn't be fetched is asked for again after a minute.
- The scene row's Off beats a mode that is on; a photo step pressed during a load waits its turn; the payload's rest
  command looks for the standby call in the library that has it (not yet run on a console).

## 1.0.0 (2 November 2026)

The first release: Lamplight's pages as a native PS5 title, run on one console (system software 7.20) against a real
Home Assistant. See [console validation](docs/console-validation.md) for what was checked there.

- The clock, Living room, Weather, Agenda, Timers, Wi-Fi, What can I say? and Assistant pages, with Lamplight's
  colours, type, discs, load states, toasts and Undo, at TV scale with controller focus.
- A room page from `lamplight.conf`: the speaker's now playing, the room's actions (two presses for the big ones)
  and its things as tiles that glow when on.
- The clock rests after a few idle minutes (chips gone, dimmed, drifting, near-black at night), and every page goes
  back to it after five; kind to an OLED.
- A finished timer or appliance takes over the screen until answered, and pulses the light bar.
- Setup on the console: the connection, and lists of your Home Assistant's weather, calendars, View Assist
  devices, appliance timers, assistants and switches. `lamplight.conf` adds anything else at any start.
- The guest Wi-Fi QR code is made on the console, so the password is never shown as text.
- Type a question for the assistant with the PS5 keyboard.
- Home Assistant's time when the console's clock is out, and the real midnights on days the clocks change.
- Wakes from rest mode connected.
- `lamplight-notify.elf`: Home Assistant's notifications on the PS5 over any game, and the console's temperatures,
  free space and uptime in Home Assistant, with a package for it (`homeassistant/lamplight_ps5.yaml`).

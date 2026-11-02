# Changelog

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

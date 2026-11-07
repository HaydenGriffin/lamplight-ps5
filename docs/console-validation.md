# Console validation

The PC preview and the tests can't show controller feel, legibility from the sofa, frame rate, the system
keyboard, suspension, or the console's clock. These are checked on a console, with the owner present. Don't deploy
or launch without the owner's permission.

## Before the first run

1. List `/data/homebrew/` over FTP (read only) and confirm nothing uses `PPSA99484`.
2. The title is a game for now (`tools/check.py` says so), so Remote Play can stream it.
3. `make deploy PS5_HOST=<console address>`, wait about ten seconds, and register the title if the enabler
   needs it.

## First run

- Lamplight opens in Setup. Type the address, port and token; Connect; wait for **Connected** and
  **Saved on this PS5**. Restart Lamplight and confirm it connects by itself.
- Choose the weather, calendars, View Assist device, appliance timers, assistant and visitors switch from the
  lists. Check each list shows your entities and that the choices stick after a restart.
- **The clock:** check the time and date against the PS5's own clock. Note the console's time zone setting. In
  summer time (or by changing the zone briefly), check the hour; Afterglow's note that the kernel's summer-time
  handling is unconfirmed applies here too.
- **Weather:** the forecast loads; sunrise and sunset are right.
- **Agenda:** today's and tomorrow's events match your calendars, including all-day ones; Up and Down scroll.
- **Timers:** set a 5-minute timer from the page and see it appear on the View Assist device; cancel it, press
  Triangle within 8 seconds and see it come back; cancel an alarm (two presses).
- **Wi-Fi:** scan the code with an iPhone and an Android phone from the sofa.
- **Controls:** on a light's tile, tap Cross (it toggles, once) and hold Cross (the ring fills and the sheet opens,
  no toggle). Slide brightness with the D-pad and the left stick and watch the lamp follow; set warmth, a colour and
  an effect; move a group's single lamp; use the right stick from the page; the All lamps slider from an action.
  Open a thermostat, blinds and a speaker's sheets and use each row. Close a sheet mid-slide and check the last value
  lands. Unlock takes two presses.
- **Scenes:** on a room's main light, each scene does what it says and the chosen one follows the lamp, including
  when something else changes it. Cinema runs its script.
- **Charts:** a thermostat's and a sensor's chart load in a moment, and their last point matches the reading now.
  Leave one open for six minutes: it refreshes without a stutter.
- **Warm-ups:** 21° for 2 hours starts the warm-up, in the house's own words; Stop ends it. A press with the radiator
  offline says why.
- **House controls:** L2 from every page. Everything off: first press only arms it; second runs it; Undo puts the
  lamps back within 8 seconds. Turn each mode on and off, and see it on the tablets. Guests off asks twice. With the
  visitors switch on, only that switch runs.
- **Speakers:** Square on Now Playing. Play everywhere, then Only this room; Move here from another room; Pause and
  the volume while together reach every room; a room joining and leaving; a sleep timer counting down and Off
  cancelling it. Stop Music Assistant and check a move says it couldn't.
- **Slideshow:** Triangle on the clock; check portrait and EXIF-rotated photos, step, pause, hide the clock, change
  the time with L1 and R1, and choose a folder in Setup. Leave it an hour and watch the memory: nothing should grow.
- **Backgrounds:** set each of Setup's Background choices and read every page from the sofa over them; a room's
  `photo`; the cover behind Now Playing; a camera tile.
- **What can I say?:** run an example; run one marked `confirm` (two presses); type a question with Triangle;
  turn the visitors switch on and check nothing runs.
- **Assistant:** ask the View Assist device something and see the answer (and its picture, if it has one at an
  `http://` address); Square toggles Keep.
- **Legibility and feel:** read every page from the sofa; move around with the D-pad and the stick.
- **Connection loss:** stop Home Assistant briefly; pages keep what they had and say they're offline; after it
  returns, nothing pressed while it was away is sent.
- Leave Lamplight on the clock for 30 minutes and check it still updates. Close it with Setup → Close Lamplight.

Record the firmware, enabler and FTP server versions, the package SHA-256 and what you saw. A build that compiles
or a preview that renders is not hardware evidence.

## Before a release

`python3 tools/category.py media`, rebuild, deploy, re-register, and check it opens from the media apps. The picture
can't be streamed over Remote Play then, so check it on the TV.

## Results

### 2 November 2026, first console run

PS5 on system software 7.20, ShadowMount Plus 1.7-beta11, ftpsrv on 2121, PS5Upload 10.7.2 (its Remote Play
automation API drove the controller and took the pictures). A Home Assistant with about 3,170
entities; a dedicated token. Title registered as a game.

- **Worked:** install, registration and launch; connecting from a bootstrap `lamplight.conf`, saving its own
  settings, and connecting again from them after a restart with the file deleted; the clock, Weather (live
  forecast, sunrise and sunset), Agenda (real calendars, all-day items with their icons), Timers (an appliance
  timer counting down; a 5-minute View Assist timer started, cancelled, brought back with Undo and cancelled
  again, each checked in Home Assistant), Wi-Fi's empty state, Help (an example asked `conversation.house_voice`
  and its answer showed on Assistant), Assistant's "Didn't catch that" for a View Assist misfire, the PS5
  keyboard opening with its title, the Setup lists from the catalogue, L1 and R1, and Close.
- **Found and fixed:**
  - Every local time was 00:00 on 1 January 1970. The kernel's UTC-to-local call was declared with buffers
    smaller than it writes (as in Afterglow). It now gets larger ones, and an offset no time zone has is ignored.
  - The console's clock was 3 minutes fast, so a new 5-minute timer showed 1 min 47 s. Lamplight now measures
    the console against Home Assistant's clock (the Date of the websocket upgrade) and corrects it.
  - Undo sent "741 seconds"; View Assist sets nothing for seconds over 59. It now sends "12 minutes 21 seconds".
  - The Timers subtitle said nothing was running while an appliance was; Keep was offered for an answer that
    wasn't View Assist's.
- **Not checked:** summer time (the UK is on GMT, so the kernel's offset and the UTC fallback look the same);
  typing a question (over Remote Play the console hands typing to the streaming device, which the API can't
  type into); scanning a Wi-Fi code; the visitors switch on the console; legibility from the sofa and the feel
  of the controller; frame rate; a long session. While streaming, the console's own "Remote Play connected"
  banner sits over the top of Lamplight's toasts; on the TV alone it doesn't.

### 2 November 2026, second run (the room page, alerts, the resting clock, notifications)

Same console and Home Assistant.

- **Worked:**
  - The Living room page from a `lamplight.conf` holding only `room_page` (merged over the saved settings, then
    deleted): the speaker's last track, the lights at 100 % glowing, the radiator heating at 17°. Pressing the side
    light's tile turned it off and on again (read back in Home Assistant each time).
  - A 30-second View Assist timer finished and its alert took over the room page.
  - The clock at rest on the console: chips gone, dimmed; and a page left open went back to the clock by itself.
  - `lamplight-notify.elf` loaded through an ELF loader: Home Assistant's event shown as a PS5 notification with
    Lamplight's icon (from `/user/appmeta/PPSA99484/icon0.png`; the install folder's copy shows a blank icon), and
    the status event reaching Home Assistant every minute (SoC 38 °C, CPU 39 °C, 111 GB free). Added to the
    autoloader's list for boot.
- **Found and fixed:**
  - Stop on an alert sent `cancel_timer` for a timer View Assist had already removed: an error toast. Stop now
    cancels only a timer View Assist still lists (it rings until then), and says OK otherwise.
  - A timer View Assist drops from its list as it finishes didn't alert if Lamplight hadn't yet seen it past its
    end. One that leaves the list at its end now alerts; one that leaves early (cancelled), or a paused one, doesn't.
  - `lamplight.conf` was read only before the first save, so nothing Setup doesn't offer could be added later. It is
    now read at every start, over the saved settings.
- **Not checked:** waking from rest mode (it would need the console put to rest and woken remotely), the payload
  starting from the autoloader at the next boot, and a notification over a running game (the PS5 shows system
  notifications over games by design; seen here over the home screen).


### 2 November 2026, third run (the 1.0.0 release)

Same console and Home Assistant.

- **Worked:**
  - The 1.0.0 build, in the media category, installed over the earlier one, registered and launched with its
    saved settings (no `lamplight.conf`). As expected for a media title, Remote Play shows it black.
  - The 1.0.0 `lamplight-notify.elf` replaced the running payload and the autoloader's copy. Its first status
    event reached Home Assistant within seconds (SoC 44 °C, CPU 48 °C).
  - A notification over a running game: `script.ps5_notify` showed over Tetris Effect: Connected's opening screen,
    with Lamplight's icon.
- **Not checked:** waking from rest mode and the payload starting at the next boot. Either one risks the
  console's homebrew setup, which has to be restored by hand, so they wait for someone at the console.

### 6 November 2026, the 1.1.0 run (controls, scenes, charts, House controls, Speakers, photos)

Same console (7.20, jailbreak still in place after rest mode), Home Assistant and PS5Upload 10.7.2. Title as a game.
The new settings came from a test `lamplight.conf` (Home Assistant's settings entity switched off for the run, the
tablets' entities, and one extra action, Fireplace off, to try Undo safely); afterwards the saved settings were put
back as they were and the file deleted. People were home and the office speaker was playing, so Everything off, Good
night, Away, Night mode, Play everywhere and Move here were not run for real. Final package SHA-256
`f0c1c0fd97f262eca59ba055da904a6e641f0a50975aee100836d3501eb2f034`.

- **Worked:**
  - L2 opens House controls on every page tried (the clock, each room, Now Playing, House, Weather, Agenda, Timers,
    Wi-Fi, Help, Assistant) and closes it again; not on the close screen.
  - Cinema on and off from House controls and from the scene row (the house's script dimmed the lit lamps to 10% and
    put them back). Guests on; with it on, Cinema refused with "Visitors mode is on"; Guests off took two presses.
  - Everything off: the first press only armed it, Circle within 5 s cancelled just that, and nothing ran. Fireplace
    off ran on the second press and Undo brought the lamp back in 2 s ("Lights back as they were").
  - Relax, Bright and Off in Main lights' sheet, each read back in Home Assistant (45% at 2700 K, 100% at 4000 K, off),
    the chosen one lit. Brightness by D-pad (5% a step) and by left stick; the right stick from the page; hold Cross
    0.9 s opened the sheet without toggling; a tap toggled once.
  - The radiator's chart (from the room sensor) and the kitchen temperature's loaded in about a second, last points
    matching the readings. 21° for 2 hours and Stop ran the house's script and showed its own words.
  - Speakers on Square and from the speaker sheet: the office's music listed under Playing elsewhere; a 15-minute
    sleep timer on the kitchen started and Off cancelled it; volume; the bedroom leaving and rejoining Everywhere.
  - The folder chooser over Home Assistant's media; the slideshow (portrait photos over their blur, step, pause,
    hide the clock, L1 and R1 down to 10 s); Background: Photos behind the clock and the pages.
  - Memory over a 16-minute slideshow at 10 s a photo (two laps of 46 phone photos): 577 to 585 MiB, no growth.
- **Found and fixed** (fixed build deployed and each checked again on the console):
  - A room with five actions put the fifth under the button hints (since 1.0.0).
  - Toasts over a sheet covered its title; a script's reply was gone in 2.5 s.
  - Sliders snapped what a thing reports to their step (75% beside the page's 73%).
  - Sensors read "16.53 °C" and "75.02%"; the kitchen's icon wasn't in the font (12 house icons added).
  - The Speakers sheet opened on Move here; its sleep timer didn't say whose it was, or show the running choice.
  - Chart hours read 17:38, 23:38...; All lamps counted Main lights and its own two lamps as three.
- **Seen, not changed:**
  - The house's warm-up sets the radiator head to 25° (its plan's way to reach 21°), so the sheet then says Set 25°.
  - The radiator's header reads the head (18°) while its chart reads the room sensor (19.6°).
  - With the lamps off but Cinema mode on, the scene row keeps Cinema lit (the tablets' order: a mode first).
  - A Next pressed while the next photo was still loading was dropped.
  - The Remote Play stream dropped once ("nothing for 15 s") just after Background: Photos; Lamplight carried on.
  - `script.ps5_rest` didn't put the console to rest within 90 s; the house's rest button did.
- **Changed since, and not yet run on the console:** a thermostat with `boosts` reads its room `sensor` and has no
  target of its own; the scene row's Off beats a mode; a photo step during a load waits its turn; `script.ps5_rest`
  looks for the standby call in `libSceSystemService`, where the payload SDK has it; and the fixes from a review of the
  whole of 1.1.0, listed in the [changelog](../CHANGELOG.md).
- **Not checked:** Everything off, Good night, Away and Night mode for real; Play everywhere and Move here; a move
  failing; a chart refreshing after five minutes; connection loss; a session over an hour; typing; the sofa;
  `script.ps5_rest` with the new lookup.

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

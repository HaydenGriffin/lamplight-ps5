# The DualSense microphone: spike (2 November 2026)

**Question:** can a homebrew title read the controller's microphone, so "What can I say?" could listen, through Home
Assistant's Assist pipeline, instead of being typed?

**How:** the payload SDK has no import stub for `libSceAudioIn`, so one was made: a shared object whose soname is
`libSceAudioIn.sprx` and which defines `sceAudioInOpen`, `sceAudioInInput`, `sceAudioInClose` (empty bodies; only the
names matter to the converter, as in the SDK's own stubs):

```sh
clang-18 -target x86_64-unknown-linux -shared -nostdlib -fPIC -fuse-ld=lld \
    -Wl,-soname,libSceAudioIn.sprx stub.c -o libSceAudioIn.so
```

and added to `APP_IMPORT_STUBS`. A probe build opened the microphone for the controller's user
(`sceAudioInOpen(user, type, 0, 256, 16000, 0)`: 256 samples, 16 kHz, signed 16-bit mono) and read 30 blocks with
`sceAudioInInput`.

**Result on the console (system software 7.20):** types 0 and 1 opened and delivered every block; type 2 was refused
(`0x80260104`). Every sample was 0. Nobody was speaking, the controller's microphone may have been muted, and the
session was driven over Remote Play, so silence doesn't say whether sound gets through.

**So:** the API is reachable from a homebrew title, and the data path works. Before building listening into Help, check
with someone speaking into an unmuted DualSense (the peak level should rise well above 0). If it does, the rest is
known ground: stream the 16 kHz samples to Home Assistant's `assist_pipeline/run` over the same websocket (binary
frames prefixed with the pipeline's handler id), from `stt` to `intent`, and show the reply as Help does now.

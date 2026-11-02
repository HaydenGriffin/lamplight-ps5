# Lamplight for PS5 notices

Lamplight for PS5 is GPL-3.0-or-later. Lamplight, the Home Assistant cards it comes from, remains MIT.

- Native UI, core, graphics, runtime and build tools: BlackBearReloaded's ps5-homebrew-ui / ps5-native-app-boilerplate, GPL-3.0-or-later. See LICENSE and the retained source headers.
- WebSocket, image fetch and system keyboard: Afterglow contributors, GPL-3.0-or-later; the Home Assistant session and settings storage follow Prism for PS5. See UPSTREAM.md.
- nlohmann JSON 3.12.0: Niels Lohmann, MIT. src/third_party/JSON-LICENSE.txt, also in the app's licences folder.
- stb_image, stb_image_write and stb_truetype: Sean Barrett and contributors, MIT or public domain. src/third_party/stb/LICENSE.
- QR Code generator library 1.8.0 (C): Project Nayuki, MIT. third_party/qrcodegen/LICENSE.
- Material Design Icons 7.4.47 (from @mdi/js): Pictogrammers, Pictogrammers Free License (icons under Apache-2.0). third_party/mdi/LICENSE. tools/make-fonts.py turns the paths of the icons Lamplight uses into third_party/fonts/lamplight_icons.ttf.
- Atkinson Hyperlegible Next, Braille Institute of America, and Lexend, The Lexend Project Authors: SIL Open Font License 1.1; third_party/fonts/LICENSE.txt. Lexend 400 is instanced from Lamplight's variable Lexend; the outlines are merged before baking. `make fonts` bakes the SDF atlases.
- PS5 Payload SDK 0.42 and musl objects: MIT, third_party/musl/COPYRIGHT. The public SDK is fetched and hash-checked by tools/setup-native-dependencies.sh.
- ps5-opengl / Mesa: the pinned release's LICENSE, THIRD_PARTY_NOTICES.md and LICENSES are copied into every package by tools/build.sh. ps5-opengl 1.0.2 is by BlackBearReloaded (GPL-3.0-or-later); its source is kept at https://github.com/J7mbo/ps5-opengl (commit a519561 upstream).

No proprietary Sony SDK, firmware or keys are shipped. No connection credentials are packaged.

# Where this comes from

- The PS5 infrastructure (src/gfx, src/ui, src/core, src/platform, src/runtime, src/afterglow, tooling/ and the
  build helpers in tools/) is copied from [Prism for PS5](https://github.com/HaydenGriffin/prism-ps5) at
  be7b0cd, which took it from [Afterglow for PS5](https://github.com/HaydenGriffin/afterglow-ps5) at
  4e0862028d40c695f86e44a5266b67740e895c60 and BlackBearReloaded's ps5-homebrew-ui. Original copyright headers
  are kept. Changes here: the JSON decimal patch is in the `lamplight` namespace, and the font baker gained
  `range:` and `digits=`.
- The pages, words and rules follow [Lamplight](https://github.com/HaydenGriffin/lamplight) 1.2.0 (its cards in
  `src/cards/` and its `DESIGN.md`).

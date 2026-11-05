# Third-party notices

Kyuhachi X is distributed under GPL-2.0 (see `LICENSE`). It bundles or links the following.

| Component | License | Where |
|---|---|---|
| DOSBox-X (parts, modified) | GPL-2.0 | `core_patches/`, `core_added/`, pinned in `third_party/DOSBOX_X.lock` |
| SDL2 | zlib | linked |
| zlib | zlib | linked |
| libpng | libpng | linked |
| Neodgm (NeoDunggeunmo), © 2017-2021 Eunbin Jeong (Dalgona.) | SIL OFL 1.1 | source: `legacy/work_t52i/OFL_neodgm.txt` |
| built-in bitmap font (derived from Neodgm by the Kyuhachi team, embedded in the program as `src/gensei_embed.h`) | SIL OFL 1.1 | built in |
| Shinonome 16 dot Gothic (`shnmk16.bdf`, JIS X 0208), converted to `res/fonts/shinonome16_jp.fnt` | Public Domain | notice: `legacy/work_t52i/shinonome16_LICENSE.txt` |
| SDL_GameControllerDB (`gamecontrollerdb.txt`) | zlib | notice: `legacy/work_t48e4/LICENSE_gamecontrollerdb.txt` |
| Sharp-Shimmerless shader | CC0 | converted, built in |

The full license texts of the bundled fonts and the GPL are in the `LICENSE` file shipped with each package.

Not distributed (and never committed): game images, NEC BIOS / font ROMs, FREECG98, original game fonts, YM2608 rhythm samples.

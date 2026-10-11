# Classic c0.0.13a_03 implementation status

Branch: `classic-c0.0.13a_03`. The released `main` branch and its Pre-Classic
tags remain unchanged.

## Implemented in the current branch

- Classic-style improved Perlin, octave and distorted noise primitives.
- Six-pass world generation: raising, eroding, soiling, carving, watering and
  melting.
- Tile IDs 1–6 from the earlier engine, boundary tile 7, flowing/still water
  8–9 and flowing/still lava 10–11.
- Basic flow, stationary-to-flowing transitions, water/lava reactions and
  neighbor-change notifications.
- Classic random-tile update interval and 32-bit LCG coordinate selection.
- Classic lighting-height convention and boundary collision boxes.
- Dry-land, water and lava movement constants with liquid detection.
- Classic save metadata header (magic/version/name/creator/time/dimensions),
  compressed block payload, raw-GZIP legacy load fallback and atomic save
  replacement with a backup.
- Separate mesh buffers and an alpha-blended pass for liquid geometry.
- Zero zombies spawned on world entry, matching the Classic client.

## Still incomplete

- **Exactness validation:** generator noise and flood fill have been compared
  against the supplied classes, but the complete native result has not yet
  been compared block-for-block against the original Java output.
- **Fluid rendering:** the mesh path exists, but correct sorting of transparent
  water by distance and reference-matched fluid face visibility still need
  work. Water/lava immersion fog isn't wired into the player render path.
- **Horizon:** the 16×16 `rock.png` and `water.png` textures, horizon ground,
  walls and water plane have not yet been connected.
- **Interface:** Classic pause/menu flow, world naming/selection and terrain
  generation progress are not implemented. Existing inputs remain a native
  3DS adaptation.
- **Physics:** liquid detection and major movement constants are present, but
  the original liquid wall-step collision response is still missing.
- **Build/test:** this environment has no devkitPro toolchain, so I cannot
  honestly claim a compiled 3DS build. The branch still needs a devkitPro build
  and Citra/original-hardware play test.

Use the compiled `.class` files as the behavior reference; do not carry over
invalid decompiler constructs.

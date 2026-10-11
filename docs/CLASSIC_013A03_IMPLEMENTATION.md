# Classic c0.0.13a_03 implementation status

Active development target: Minecraft Classic `c0.0.13a_03` on `main`. Published
Pre-Classic release tags remain available as historical snapshots.

## Implemented in the current branch

- Classic improved Perlin noise, octaves and distorted-noise fields.
- The six terrain generation passes: raising, eroding, soiling, carving,
  watering and melting.
- Tile IDs 1–6, special boundary tile 7, flowing/still water 8–9, and
  flowing/still lava 10–11.
- Fluid random-tick flow, stationary-to-flowing transitions, neighbor changes,
  lava/water reactions, and the original tickable-tile filter and random update
  scheduling. The six original
  selectable block types (IDs 1–6) are available through the 3DS selector.
- Classic lighting-height convention, world-boundary collision and the major
  dry/water/lava movement constants.
- Original Classic save metadata header (16-bit dimensions in X/Z/Y field order),
  compressed block payload, backward loading of early 32-bit-dimension native
  drafts and raw GZIP legacy saves, and safer temporary-file/backup replacement.
- Separate opaque-lava and transparent-water mesh paths, approximate
  far-to-near water chunk sorting, water/lava immersion fog, and the outer rock
  floor/skirt, edge walls, and surrounding water plane using `rock.png` and
  `water.png`. Horizon UV orientation and side-wall texture scaling now follow
  the original renderer.
- Source-style BREAK/PLACE mode toggle and a whole-block wireframe selection
  outline (the original has no filled selection flash or placement preview),
  with player/zombie collision checks before placement.
- Top-left version and live FPS/chunk-update text using a built-in bitmap font.
- Zero zombies spawned when a Classic world starts.
- Generation-stage progress on the bottom screen.
- A temporary pause menu with resume, save, load, regenerate and exit options.

## Still incomplete / unverified

- **Exactness validation:** the native terrain has not yet been compared
  block-for-block against output produced by the original Java class files.
- **Fluid rendering fidelity:** lava has an opaque mesh pass and water uses
  alpha blending with approximate far-to-near chunk sorting. Fog color/density
  follows the extracted source (blue water, red-orange lava). The original
  ambient-light tint during immersion and exact transparent display-list order
  still need visual validation.
- **Physics:** the original player wall-step response while swimming or in lava
  is present, but its collision edge cases still need in-game testing.
- **Interface:** generation progress and a basic 3DS pause/save/load/regenerate
  menu are implemented. The original Classic world-selection and named-world
  screens are not yet reproduced; current save/load uses level.dat.
- **Build/test:** the editing environment does not have devkitPro installed,
  so a 3DS build could not be run here. The branch needs a local devkitPro
  build and Azahar/original-hardware test before release.

Required external textures for this branch are `terrain.png`, `char.png`,
`rock.png`, and `water.png` in `3ds/assets/textures/`.

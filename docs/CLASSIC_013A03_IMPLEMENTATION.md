# Classic c0.0.13a_03 implementation status

Branch: `classic-c0.0.13a_03`. The released `main` branch and all Pre-Classic
release tags remain unchanged.

## Implemented in the current branch

- Classic improved Perlin noise, octaves and distorted-noise fields.
- The six terrain generation passes: raising, eroding, soiling, carving,
  watering and melting.
- Tile IDs 1–6, special boundary tile 7, flowing/still water 8–9, and
  flowing/still lava 10–11.
- Fluid random-tick flow, stationary-to-flowing transitions, neighbor changes,
  lava/water reactions and Classic random update scheduling.
- Classic lighting-height convention, world-boundary collision and the major
  dry/water/lava movement constants.
- Classic save metadata header, compressed block payload, raw GZIP legacy-load
  fallback, and safer temporary-file/backup replacement.
- A separate liquid mesh path, alpha-blended water rendering, and the outer
  rock floor/skirt, edge walls, and surrounding water plane using `rock.png`
  and `water.png`.
- Zero zombies spawned when a Classic world starts.

## Still incomplete / unverified

- **Exactness validation:** the native terrain has not yet been compared
  block-for-block against output produced by the original Java class files.
- **Fluid rendering fidelity:** water chunks are not yet sorted exactly as
  the reference's transparent display lists; fluid immersion fog and some
  face-visibility/tint details still need matching.
- **Physics:** the original player wall-step response while swimming or in lava
  is still missing.
- **Interface:** generation-stage progress and the Classic pause, save/load
  and world-selection menus are not implemented. Existing 3DS-native controls
  are retained as the temporary interface.
- **Build/test:** the editing environment does not have devkitPro installed,
  so a 3DS build could not be run here. The branch needs a local devkitPro
  build and Citra/original-hardware test before release.

Required external textures for this branch are `terrain.png`, `char.png`,
`rock.png`, and `water.png` in `3ds/assets/textures/`.

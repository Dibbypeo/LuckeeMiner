# LuckeeMiner: Classic c0.0.13a_03

**Experimental native C++ / Citro3D port of Minecraft Classic `c0.0.13a_03` for the original Nintendo 3DS.**

This branch is a separate milestone based on the existing LuckeeMiner engine.
The published Pre-Classic releases and the `main` branch remain unchanged.
Classic-specific behavior is being recreated from the supplied compiled client
classes; the Java implementation is used as a behavioral reference and is not
copied into the project.

## Current status

**Implementation in progress; not release-ready yet.**

Implemented in this branch:
- Classic-style improved Perlin / octave / distorted noise.
- Six terrain generation passes: raising, eroding, soiling, carving, watering, melting.
- Tile IDs 1–11, including the anonymous stone, wood and bush entries (4–6), the boundary tile (7), water (8–9) and lava (10–11).
- Basic fluid ticks, neighbor notifications, and water/lava reactions.
- Classic random tile-update schedule and coordinate RNG.
- Core land/water/lava movement constants.
- Classic metadata save header plus legacy raw-GZIP block payload loading.
- Initial liquid mesh pass and the rock / water horizon.

Known remaining work includes the pause and world-selection menus, generation
progress reporting, player liquid wall-step behavior, exact fluid-rendering
details and sort order, and full comparison of generated terrain and save files
against the original Java client. This branch has **not yet had a confirmed
devkitPro build or a 3DS emulator/hardware test**.

## Controls (temporary 3DS adaptation)

| Input | Action |
|---|---|
| Circle Pad | Move |
| Touchscreen drag | Look / camera movement |
| A | Jump |
| L | Perform the current action |
| R | Toggle between BREAK and PLACE modes |
| D-Pad Up | Previous selectable block (1–6) |
| D-Pad Down | Next selectable block (1–6) |
| Y | Spawn a zombie (testing convenience) |
| SELECT | Save world |
| X | Reset player position |
| START | Open pause menu (resume/save/load/regenerate/exit) |

Classic uses L for the current action and R to toggle BREAK/PLACE mode. Block
placement checks player and zombie collisions. The top-left HUD shows the version
and live FPS/chunk-update counters; bottom-screen diagnostics remain available.
Original named-world selection screens are still not implemented.

## Installation and required textures

Copy the built `LuckeeMiner.3dsx` to:

    SD:/3ds/LuckeeMiner.3dsx

Required external files:

    SD:/3ds/assets/textures/terrain.png
    SD:/3ds/assets/textures/char.png
    SD:/3ds/assets/textures/rock.png
    SD:/3ds/assets/textures/water.png

- `terrain.png`: 256 × 256
- `char.png`: 64 × 32
- `rock.png`: 16 × 16
- `water.png`: 16 × 16 RGBA

The source archive supplied for this version contains these files; this project
does not bundle them. See [assets/README.md](assets/README.md).

World saves are written to `level.dat` in the application working directory.
The writer uses the original Classic metadata layout, including the
16-bit dimensions in the original field order, and a GZIP-compressed block
array. The loader also accepts earlier 32-bit-dimension native draft saves and
raw GZIP block-array saves. End-to-end exchange with the original Java client
remains to be tested.

## Build

Requirements:
- devkitPro with the 3DS toolchain
- libctru
- Citro3D
- libpng
- zlib

From the project directory:

    make clean
    make

The current editing environment does not have devkitPro available, so there is
no verified compile result to report for this branch yet.

## Source layout

- `source/classic_noise.cpp`: Classic-compatible improved Perlin noise primitives.
- `source/classic_worldgen.cpp`: six-pass Classic terrain generation.
- `source/level.cpp`: block storage, lighting, fluid notifications and save/load.
- `source/tile.cpp`: Classic tile registry and fluid simulation.
- `source/player.cpp`: player movement and liquid handling.
- `source/renderer.cpp`: chunk meshes, fluid meshes, and horizon.
- `docs/CLASSIC_013A03_IMPLEMENTATION.md`: detailed implementation status.

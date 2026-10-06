# LuckeeMiner

**A native C++ recreation of the early RubyDung / Minecraft prototype (rd-132211) for the Nintendo 3DS.**
<img width="400" height="240" alt="2026-10-05_20-54-09 630_top" src="https://github.com/user-attachments/assets/6284eb91-1963-49c4-98c7-df7642794173" />
<img width="320" height="240" alt="2026-10-05_20-54-09 630_bot" src="https://github.com/user-attachments/assets/dc65092b-7e16-4fdb-a434-4a59de98666b" />

LuckeeMiner recreates the gameplay, rendering, movement, collision, block interaction, lighting, fog, targeting, and world-save behavior of the supplied rd-132211 prototype using an original C++ implementation built for the 3DS.

The current recreation is **complete and playable for the rd-132211 scope**. It runs on original 3DS hardware, including the original 2DS-class hardware, and is designed around the limitations of the original system rather than assuming New 3DS features.

## Current status

**rd-132211 recreation: COMPLETE**

**Future versions in development.**

The current build has been tested on an original 2DS/3DS and is working as expected. The game supports the complete prototype gameplay loop currently targeted by this project:

- Native 3DS application and rendering
- RubyDung-style camera and mouse-look behavior
- 60 ticks/sec fixed simulation
- Player movement, gravity, jumping, and AABB collision
- Visible-face voxel rendering
- Terrain atlas textures with nearest-neighbor filtering
- Prototype-style lighting and fog
- CPU block targeting using the reference's centered 5×5 selection behavior
- Pulsing selected-face highlight
- Block placement and destruction
- Chunk geometry caching and invalidation
- Original-3DS memory-conscious rendering
- Compatible rd-132211 level.dat loading and saving
- PC ↔ 3DS ↔ PC save-file transfer for the rd-132211 save format

The current program has been tested in normal gameplay without crashes or known gameplay/rendering bugs.

Future work is focused on expanding LuckeeMiner beyond the rd-132211 recreation into later Minecraft versions. Those are separate development targets and are not required for the current prototype to be considered complete.

## Controls

| Input | Action |
|---|---|
| Circle Pad | Move |
| Touch screen drag | Look / camera movement |
| A | Jump |
| L | Place a block against the targeted face |
| R | Break the targeted block |
| START | Exit the application |

## Installation

The .3dsx executable and its assets are expected to live on the SD card under the 3ds directory.

Example:

    SD:/3ds/LuckeeMiner.3dsx
    SD:/3ds/assets/textures/terrain.png

The terrain texture must be named exactly:

    terrain.png

and must be a 256×256 PNG compatible with the project's terrain atlas layout.

A saved world is stored as:

    SD:/3ds/level.dat

when the application is run from that directory.

## Building from source

Requires devkitPro with the 3DS toolchain, libctru, Citro3D, libpng, and zlib.

    cd LuckeeMiner
    make

This produces the 3DS application files according to the installed devkitPro 3DS rules.

For a normal Homebrew Launcher setup, copy the generated .3dsx into:

    SD:/3ds/

and copy the texture to:

    SD:/3ds/assets/textures/terrain.png

## Save-file compatibility

The current save format intentionally matches the rd-132211 prototype.

The save consists of:

1. A GZIP-compressed block array.
2. Raw block bytes in the original (y × height + z) × width + x layout.
3. No additional header or metadata.

For the default world size, the uncompressed block payload is **4,194,304 bytes**.

This allows the block data to move between the original PC prototype and LuckeeMiner on 3DS without converting the world format.

## Project structure

- source/main.cpp — application lifecycle, input polling, simulation loop, and block interaction.
- source/player.cpp — movement, gravity, jumping, and collision.
- source/level.cpp — world storage, lighting, block edits, and save/load.
- source/renderer.cpp — chunk meshing, rendering, frustum culling, picking, and hit highlighting.
- source/texture_loader.cpp — terrain atlas loading and 3DS texture conversion.
- include/luckee/ — engine interfaces and data structures.
- docs/ — recreation notes, controls, and development roadmap.
- Makefile — devkitPro 3DS build configuration.
- LICENSE — MIT License.

## Project principles

1. LuckeeMiner uses an original C++ implementation. The historical prototype is used as a behavioral reference.
2. Original 3DS hardware is a first-class target. New 3DS-only CPU assumptions are avoided.
3. Systems are kept small and measurable so they can be tested and optimized on real hardware.
4. Rendering state is kept separate from world data.
5. Version-specific Minecraft behavior is implemented deliberately rather than mixing mechanics from different releases.

## License

LuckeeMiner is released under the **MIT License**.

See LICENSE for the complete license text.

## Future development

The rd-132211 recreation is complete, but LuckeeMiner is intended to continue beyond it.

Planned future work includes versioned world generation, later Alpha/Beta behavior, and eventually later Minecraft versions while preserving the same native 3DS-focused architecture.

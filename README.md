# LuckeeMiner

**A native C++ recreation of early RubyDung / Minecraft prototypes for the Nintendo 3DS, currently targeting rd-132328.**

<img width="400" height="240" alt="2026-10-05_20-54-09 630_top" src="https://github.com/user-attachments/assets/6284eb91-1963-49c4-98c7-df7642794173" />
<img width="320" height="240" alt="2026-10-05_20-54-09 630_bot" src="https://github.com/user-attachments/assets/dc65092b-7e16-4fdb-a434-4a59de98666b" />

LuckeeMiner recreates early RubyDung / Minecraft prototype behavior using an original C++ implementation built for the 3DS. Version-specific behavior is implemented from decompiled source used as documentation.

The current recreation is **complete and playable for the rd-132211 scope**. It runs on original 3DS hardware, including the original 2DS-class hardware, and is designed around the limitations of the original system rather than assuming New 3DS features.

## Current status

**rd-132211 recreation: COMPLETE**

**rd-132328 upgrade: IMPLEMENTED IN SOURCE**

The rd-132328 work adds the shared Entity system, randomized spawning, 100 wandering zombies, character animation/rendering, the 64×32 character texture path, and nearby-only zombie simulation/rendering to keep the original 3DS from spending resources on off-screen entities. The version-specific build is now working on hardware.

The rd-132211 baseline has been tested on an original 2DS/3DS and is working as expected. The rd-132328 source upgrade is implemented and is awaiting its own hardware verification. The current source supports the following prototype features:

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
- Randomized player spawning across the baseplate, matching the reference behavior
- 100 wandering zombies with the rd-132328 movement behavior
- Nearby-only zombie simulation and distance/frustum render culling for 3DS performance
- Animated zombie character models using the external char.png atlas

The previously tested rd-132211 build has no known gameplay, rendering, or stability issues. The new rd-132328 behavior must still be exercised on hardware before it is marked stable.

Future work will continue version by version, using each release's decompiled source as documentation and avoiding mechanics that do not exist in the selected reference.

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
    SD:/3ds/assets/textures/char.png

The terrain texture must be named exactly:

    terrain.png

and must be a 256×256 PNG compatible with the project's terrain atlas layout.

The rd-132328 character texture must be named exactly:

    char.png

and must be the 64×32 character atlas used by the prototype.

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

and copy the textures to:

    SD:/3ds/assets/textures/terrain.png
    SD:/3ds/assets/textures/char.png

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
- source/entity.cpp — shared entity movement, collision, rotation, and random spawning.
- source/player.cpp — player-specific movement, gravity, and jumping.
- source/zombie.cpp — rd-132328 wandering zombie behavior.
- source/level.cpp — world storage, lighting, block edits, and save/load.
- source/renderer.cpp — chunk meshing, terrain/character rendering, frustum culling, picking, and hit highlighting.
- source/texture_loader.cpp — terrain and character PNG loading plus 3DS texture conversion.
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

The rd-132328 implementation is the current development target. The next version step will continue the same process: compare against that version's decompiled reference, implement only the behavior actually present there, then verify it on original 3DS hardware.

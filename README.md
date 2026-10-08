# LuckeeMiner

**A native C++ Nintendo 3DS recreation of early RubyDung / Minecraft prototype clients, currently targeting rd-20090515.**

LuckeeMiner recreates historical RubyDung / Minecraft prototype behavior with an original C++ implementation for the Nintendo 3DS. The extracted Java source from each selected client is used as behavioral documentation. The Java implementation is not copied into the project.

## Current status

**rd-132328 recreation: RELEASED**

**rd-20090515 recreation: IN DEVELOPMENT**

The rd-20090515 upgrade has begun. The current branch already contains the version's 20 TPS simulation clock, updated player and zombie physics, ten initial zombies, dynamic zombie spawning, the expanded tile registry, per-face block textures, Perlin terrain generation, random tile ticking, grass behavior, bush behavior, block-destruction particles, particle simulation/rendering, and lit/shadow entity passes.

The extracted rd-20090515 Java source audit is complete at the source level. Hardware verification for the new version has not yet been completed.

## rd-20090515 features

- 20 ticks/sec simulation.
- 256 × 64 × 256 generated voxel world.
- Perlin-noise terrain generation when no level.dat exists.
- Rock, grass, dirt, stone brick, wood, and bush tile IDs.
- Per-face terrain atlas textures.
- Grass growth and decay through random tile ticks.
- Non-solid, non-light-blocking bush tiles rendered as crossed planes.
- Ten initial zombies.
- Dynamic zombie spawning.
- Updated zombie movement, jumping, gravity, friction, and void removal.
- Terrain-textured block destruction particles.
- Particle gravity, friction, random size, lifetime, and billboard rendering.
- Lit and shadow terrain/entity rendering passes.
- Original 3DS chunk caching and frustum culling.
- Fixed-step simulation with interpolated rendering.
- Existing PC ↔ 3DS ↔ PC level.dat compatibility.
- Bottom-screen runtime debug information.

The rd-20090515 front-end now includes the centered crosshair and selected-block preview on the top screen. The bottom screen remains the runtime debug display during development.

## Controls

| Input | Action |
|---|---|
| Circle Pad | Move |
| Touch screen drag | Look / camera movement |
| A | Jump |
| L | Place the selected block |
| R | Break the targeted block |
| D-Pad Up | Select previous placeable block |
| D-Pad Down | Select next placeable block |
| Y | Spawn a zombie at the player's position |
| SELECT | Save the world immediately |
| X | Reset the player's position |
| START | Save and exit |

The full control description is in docs/CONTROLS.md, and the same information is shown on the bottom-screen debug display.

## Installation

LuckeeMiner expects the executable and external assets under the SD card's 3ds directory.

    SD:/3ds/LuckeeMiner.3dsx
    SD:/3ds/assets/textures/terrain.png
    SD:/3ds/assets/textures/char.png

Required assets:

- terrain.png: 256 × 256 terrain atlas from the target client.
- char.png: 64 × 32 character atlas from the target client.

The application checks for both files before starting and reports missing assets on the bottom screen.

A saved world is stored as:

    SD:/3ds/level.dat

when the application is launched from that directory.

LuckeeMiner does not automatically bundle historical game assets. See assets/README.md for the external asset layout and distribution note.

## Building from source

Requirements:

- devkitPro with the 3DS toolchain.
- libctru.
- Citro3D.
- libpng.
- zlib.

Build:

    cd LuckeeMiner
    make

## Save-file compatibility

The block save format remains the raw GZIP-compressed block array used by the earlier prototype family. For the default 256 × 64 × 256 world, the uncompressed block payload is 4,194,304 bytes.

The rd-20090515 client still uses the same basic raw block layout, so existing block-format compatibility is retained while the meanings of the nonzero tile IDs expand.

SELECT performs an immediate save. START performs a normal shutdown save.

## Version-specific development

LuckeeMiner is developed one historical client version at a time. For each target, the extracted source is compared against the current C++ implementation before adding or changing mechanics.

The rd-20090515 source extracted from the supplied client JAR is the primary source for this milestone. Public mirrors may be consulted for corroboration, but version-specific behavior is not taken from a different build when the supplied client provides the exact classes.

The C++ renderer uses Citro3D vertex buffers, cached chunks, frustum culling, and bounded allocations instead of desktop LWJGL/OpenGL display lists. These changes are implementation adaptations for the original 3DS, not new gameplay systems.

## Project structure

- source/main.cpp — application lifecycle, 20 TPS loop, input, tile selection, block interaction, saving, and zombie spawning.
- source/entity.cpp — shared entity physics, collision, sizes, reset/removal state, and lighting.
- source/player.cpp — rd-20090515 player movement, gravity, jumping, and reset handling.
- source/zombie.cpp — rd-20090515 zombie movement and removal behavior.
- source/tile.cpp — tile registry, block properties, grass logic, bush logic, and destruction particles.
- source/perlin_noise.cpp — native recreation of the extracted PerlinNoiseFilter.
- source/particle.cpp — particle simulation.
- source/particle_engine.cpp — particle ownership and ticking.
- source/level.cpp — terrain generation, world storage, lighting, random tile ticks, and level.dat I/O.
- source/renderer.cpp — chunk meshing, tile/character/particle rendering, frustum culling, picking, and hit highlighting.
- source/texture_loader.cpp — terrain and character PNG loading plus 3DS texture conversion.
- include/luckee/ — native engine interfaces and data structures.
- docs/ — version notes, controls, prototype notes, and roadmap.
- assets/ — external asset layout documentation.
- Makefile — devkitPro 3DS build configuration.
- LICENSE — MIT License.

## Documentation

- docs/CONTROLS.md — current 3DS control mapping.
- docs/PROTOTYPE_NOTES.md — recreation rules and version-specific behavior.
- docs/VERSION_RD-132328.md — previous released target.
- docs/VERSION_RD-20090515.md — current version-specific notes and implementation status.
- docs/ROADMAP.md — milestone tracking.
- assets/README.md — external assets.

## License

LuckeeMiner is released under the MIT License.

See LICENSE for the complete license text.

## Current release workflow

The rd-132328 version has already been released. The rd-20090515 version will be released only after its extracted-source audit, hardware testing, and final packaging are complete.

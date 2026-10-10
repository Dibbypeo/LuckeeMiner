# LuckeeMiner

**A native C++ Nintendo 3DS recreation of early RubyDung / Minecraft prototype clients, currently targeting rd-20090515.**

LuckeeMiner recreates historical RubyDung / Minecraft prototype behavior with an original C++ implementation for the Nintendo 3DS. The extracted Java source from each selected client is used as behavioral documentation. The Java implementation is not copied into the project.

## Current status

**rd-132328 recreation: RELEASED**

**rd-20090515 recreation: READY FOR MANUAL RELEASE (not published)**

The rd-20090515 implementation includes the version's 20 TPS simulation clock, updated player and zombie physics, ten initial zombies, bounded dynamic zombie spawning, the expanded tile registry, per-face block textures, Perlin terrain generation, random tile ticking, grass behavior, bush behavior, bounded block-destruction particles, particle simulation/rendering, and lit/shadow entity passes.

The extracted rd-20090515 Java source and native implementation have received a full source-level audit. The maintainer has confirmed the current gameplay works. Build, package, tag, and publish the version manually using the existing project workflow.

## rd-20090515 features

- 20 ticks/sec simulation.
- 256 × 64 × 256 generated voxel world.
- Perlin-noise terrain generation when no level.dat exists.
- Rock, grass, dirt, stone brick, wood, and bush tile IDs.
- Per-face terrain atlas textures.
- Grass growth and decay through random tile ticks.
- Non-solid, non-light-blocking bush tiles rendered as crossed planes.
- Ten initial zombies.
- Dynamic zombie spawning, bounded to 32 active zombies on original 3DS hardware.
- Updated zombie movement, jumping, gravity, friction, and void removal.
- Terrain-textured block destruction particles, bounded to 512 active particles.
- Particle gravity, friction, random size, lifetime, and billboard rendering.
- Lit and shadow terrain/entity rendering passes.
- Original 3DS chunk caching and frustum culling.
- Fixed-step simulation with interpolated rendering.
- PC ↔ 3DS ↔ PC level.dat payload/layout compatibility.
- Bottom-screen runtime debug information.

The rd-20090515 front-end includes the centered crosshair and selected-block preview on the top screen. The bottom screen remains a runtime debug display.

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

The full control description is in docs/CONTROLS.md. Original-3DS safety budgets limit the game to 32 active zombies and 512 active particles; when the particle pool cannot fit a full 64-particle burst, that burst is skipped.

## Installation

Copy the built executable to:

    SD:/3ds/LuckeeMiner.3dsx

Required external assets:

    SD:/3ds/assets/textures/terrain.png
    SD:/3ds/assets/textures/char.png

- terrain.png: 256 × 256 terrain atlas from the target client.
- char.png: 64 × 32 character atlas from the target client.

The application checks for both files before starting and reports missing assets on the bottom screen. Historical game assets are not bundled; see assets/README.md.

A saved world is stored as:

    SD:/3ds/level.dat

when the application is launched from the documented directory.

## Building from source

Requirements:

- devkitPro with the 3DS toolchain.
- libctru.
- Citro3D.
- libpng.
- zlib.

Build:

    make clean
    make

## Save-file compatibility

The block save format is the historical GZIP-compressed block array used by the rd-20090515 client. For the default 256 × 64 × 256 world, the uncompressed block payload is exactly 4,194,304 bytes.

LuckeeMiner stores that payload byte-for-byte in the same `(y * height + z) * width + x` layout. Its writer uses raw DEFLATE with the historical Java GZIP header/trailer format, while its reader accepts the same GZIP data produced by the Java client.

SELECT performs a save and shows its status on the bottom screen. START performs a normal shutdown save. The writer first completes `level.dat.tmp`, then replaces `level.dat`; the loader can recover `level.dat.bak` if the primary save is missing or fails validation. Compression favors speed on the 3DS. The compressed DEFLATE bytes are not promised to match Java's output byte-for-byte, but the GZIP stream and uncompressed block payload/layout remain compatible.

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
- docs/ — version notes, controls, audit, release notes, and roadmap.
- assets/ — external asset layout documentation.
- Makefile — devkitPro 3DS build configuration.
- LICENSE — MIT License.

## Documentation

- docs/CONTROLS.md — current 3DS control mapping.
- docs/PROTOTYPE_NOTES.md — recreation rules and version-specific behavior.
- docs/VERSION_RD-132328.md — previous released target.
- docs/VERSION_RD-20090515.md — current version-specific notes and implementation status.
- docs/AUDIT_RD-20090515.md — source-audit findings.
- docs/RELEASE_NOTES_RD-20090515.md — prepared release notes.
- docs/ROADMAP.md — milestone tracking.
- assets/README.md — external assets.

## License

LuckeeMiner is released under the MIT License. See LICENSE for the complete text.

## Release process

Build and test locally using the existing workflow, prepare the release files, then create the version tag and GitHub Release manually. The repository does not automate building, packaging, tagging, or publishing.

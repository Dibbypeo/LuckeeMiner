# LuckeeMiner

**A native C++ Nintendo 3DS recreation of the early RubyDung / Minecraft prototype rd-132328.**

<img width="400" height="240" alt="LuckeeMiner top screen" src="https://github.com/user-attachments/assets/6284eb91-1963-49c4-98c7-df7642794173" />
<img width="320" height="240" alt="LuckeeMiner bottom screen" src="https://github.com/user-attachments/assets/dc65092b-7e16-4fdb-a434-4a59de98666b" />

LuckeeMiner recreates the rd-132328 front-end and gameplay behavior with an original C++ implementation for the Nintendo 3DS. The historical Java source is used as a behavioral reference; the Java implementation itself is not copied into the project.

The current release candidate is designed for original 3DS-family hardware, including original 2DS-class hardware, and avoids assumptions that require a New 3DS.

## Current status

**rd-132211 foundation: COMPLETE**

**rd-132328 recreation: RELEASE CANDIDATE**

The rd-132328 source implementation is complete. It includes the shared Entity system, randomized entity spawning, 100 wandering zombies, the six-part animated character model, the required 64×32 character texture, native 3DS rendering, nearby zombie simulation/render culling, manual saving, and player position reset.

The final public release step is maintainer verification of the intended .3dsx artifact and assets on the target original 3DS/2DS hardware, followed by the release tag.

## Features

- Native Nintendo 3DS application using devkitPro, libctru, and Citro3D.
- Fixed 60 ticks/sec simulation with interpolated rendering.
- Player movement relative to horizontal facing.
- Touch-screen camera look with reference sensitivity and pitch limits.
- Player gravity, jumping, and AABB collision.
- Randomized player spawning across the X/Z baseplate.
- 256 × 64 × 256 voxel world.
- Grass and rock terrain using the reference terrain atlas.
- Visible-face chunk meshing with cached geometry.
- Reference-style lighting, fog, and two terrain rendering passes.
- Centered 5×5 block targeting.
- Pulsing selected-face highlight.
- L to place blocks and R to break blocks.
- 100 wandering zombies with reference movement, jumping, gravity, friction, and void reset behavior.
- Animated zombie head, body, arms, and legs using char.png.
- Nearby zombie simulation and distance/frustum render culling for original 3DS performance.
- PC ↔ 3DS ↔ PC compatibility for the rd-132211 level.dat block format.
- SELECT manual world saving.
- X-button player position reset.
- START exit with an automatic normal-shutdown save.
- Runtime control display on the bottom screen.

## Controls

The full control reference is documented in docs/CONTROLS.md. The same mapping is shown on the bottom screen during play.

| Input | Action |
|---|---|
| Circle Pad | Move |
| Touch screen drag | Look / camera movement |
| A | Jump |
| L | Place a block against the targeted face |
| R | Break the targeted block |
| SELECT | Save the world immediately |
| X | Reset the player's position |
| START | Save and exit |

SELECT writes the current world to level.dat immediately. START also saves during normal shutdown.

X resets the player using the reference-style reset routine, choosing a new randomized X/Z position and starting three blocks above the level depth before the simulation tick continues.

## Installation

LuckeeMiner expects the executable and external assets to be placed under the SD card's 3ds directory.

Example layout:

    SD:/3ds/LuckeeMiner.3dsx
    SD:/3ds/assets/textures/terrain.png
    SD:/3ds/assets/textures/char.png

Required assets:

- terrain.png: 256 × 256 terrain atlas.
- char.png: 64 × 32 rd-132328 character atlas.

The application checks for both files before starting and reports missing files on the bottom screen.

Saved worlds are stored as:

    SD:/3ds/level.dat

when the application is launched from that directory.

LuckeeMiner intentionally does not bundle the historical Minecraft prototype assets. See assets/README.md for the asset layout.

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

The Makefile produces the 3DS application files using the installed devkitPro rules.

For a normal Homebrew Launcher setup, copy the generated .3dsx to:

    SD:/3ds/

and place the required textures in:

    SD:/3ds/assets/textures/terrain.png
    SD:/3ds/assets/textures/char.png

## Save-file compatibility

LuckeeMiner keeps the current world format compatible with the rd-132211 prototype.

The save contains:

1. A GZIP-compressed block array.
2. Raw block bytes in the original index layout: (y × height + z) × width + x.
3. No additional world header or metadata.

For the default 256 × 64 × 256 world, the uncompressed block payload is 4,194,304 bytes.

This allows block data to be transferred between the PC prototype and LuckeeMiner without a conversion step.

Manual saves use SELECT. A normal START exit also saves the current world.

## Version-specific behavior

LuckeeMiner is developed version by version. The selected prototype's source is compared against the current C++ implementation before new mechanics are added.

For rd-132328, the implementation covers the Entity system, randomized entity spawning, 100 zombies, the character model, zombie animation and movement, and the corresponding front-end presentation.

The original Java renderer uses desktop OpenGL/LWJGL APIs. LuckeeMiner recreates the resulting geometry, transforms, texture mapping, animation, ordering, and visible behavior with native Citro3D code suitable for the original 3DS.

Some internal implementation details necessarily differ because this is a native console recreation rather than a literal Java/OpenGL port. The goal is matching the player-visible and gameplay behavior of the selected prototype while keeping the code viable on original 3DS hardware.

## Project structure

- source/main.cpp — application lifecycle, input polling, simulation loop, saving, reset handling, and block interaction.
- source/entity.cpp — shared entity movement, collision, rotation, interpolation, and random spawning.
- source/player.cpp — player-specific movement, reset handling, gravity, and jumping.
- source/zombie.cpp — rd-132328 wandering zombie behavior and character data.
- source/level.cpp — world storage, lighting, block edits, and save/load.
- source/renderer.cpp — chunk meshing, terrain/character rendering, frustum culling, picking, and hit highlighting.
- source/texture_loader.cpp — terrain and character PNG loading plus 3DS texture conversion.
- include/luckee/ — engine interfaces and data structures.
- docs/ — controls, prototype notes, roadmap, and rd-132328 version notes.
- assets/ — external asset layout documentation.
- Makefile — devkitPro 3DS build configuration.
- LICENSE — MIT License.

## Project principles

1. LuckeeMiner uses an original C++ implementation. Historical source is behavioral documentation, not code to copy.
2. Original 3DS hardware is a first-class target.
3. Version-specific mechanics are implemented deliberately and are not mixed across releases.
4. Renderer state remains separate from world data.
5. Performance and memory use are treated as core constraints on the original hardware.
6. Front-end behavior takes priority over preserving desktop-specific implementation details.

## Documentation

- docs/CONTROLS.md — complete 3DS controls and behavior notes.
- docs/PROTOTYPE_NOTES.md — reference, porting rules, version behavior, and stability notes.
- docs/VERSION_RD-132328.md — rd-132328-specific implementation and release notes.
- docs/ROADMAP.md — development and release status.
- assets/README.md — required external asset layout.

## License

LuckeeMiner is released under the **MIT License**.

See LICENSE for the complete license text.

## Release checklist

Before the public rd-132328 release:

- Build the intended final .3dsx with the release configuration.
- Verify terrain.png and char.png are present at the documented paths.
- Test normal startup on the target original 3DS/2DS hardware.
- Confirm Circle Pad movement, touch look, jumping, block placement/destruction, SELECT saving, X reset, and START exit.
- Confirm level.dat saving and loading after a restart.
- Confirm the release artifact is the version documented as rd-132328.
- Create the public repository release/tag.

The source tree and documentation are prepared for that final verification step.

# LuckeeMiner rd-20090515

**Status:** Release candidate  
**Target:** Nintendo 3DS / 2DS homebrew  
**Client recreation:** RubyDung rd-20090515 (Pre-Classic)

## Highlights

- Recreates the rd-20090515 world and simulation in native C++.
- 20 ticks per second and Perlin-noise terrain generation.
- Historical tile types, per-face textures, random tile ticks, grass behavior, and crossed-plane bushes.
- Player and zombie physics based on the extracted reference client.
- Terrain-textured block destruction particles.
- Citro3D chunk caching and frustum culling for original 3DS hardware.
- Bounded runtime budgets of 32 active zombies and 512 active particles.
- GZIP-compressed `level.dat` save/load using the same uncompressed block payload and indexing layout as the Java client.

## Installation

1. Copy `LuckeeMiner.3dsx` to `SD:/3ds/LuckeeMiner.3dsx`.
2. Place the required, separately obtained assets at:
   - `SD:/3ds/assets/textures/terrain.png` (256 × 256)
   - `SD:/3ds/assets/textures/char.png` (64 × 32)
3. Launch LuckeeMiner from the Homebrew Launcher.

The archive does not contain historical game assets. Only obtain and use assets you are permitted to use. See the included `assets/README.md`.

The world save is written to `SD:/3ds/level.dat` when launched from the documented directory.

## Controls

See `docs/CONTROLS.md` for the full control mapping.

- Circle Pad: move
- Touch drag: look
- A: jump
- L: place block
- R: break block
- D-Pad Up/Down: change selected block
- Y: spawn a zombie (up to 32 active)
- SELECT: save
- X: reset player position
- START: save and exit

## Save compatibility

The reader accepts the Java client's GZIP save format. The native writer preserves the uncompressed block payload and indexing layout; compressed DEFLATE bytes are not guaranteed to be identical to Java's output.

## Credits and license

LuckeeMiner source is distributed under the MIT License. See `LICENSE`. Historical asset licensing is separate.

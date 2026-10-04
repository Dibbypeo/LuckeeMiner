# LuckeeMiner

**An original C++ recreation of the early RubyDung / Minecraft prototype for the Nintendo 3DS.**

LuckeeMiner studies the behavior and structure of the supplied `rd-132211` prototype archive, but its implementation is written from scratch for the 3DS. It does not copy Minecraft source code. The long-term goal is a deterministic voxel world with a path toward Java Edition-compatible seed generation, while respecting original 3DS hardware limits.

## Current status

This repository is at the **engine foundation** stage. It currently contains a native 3DS application loop and input mapping, as well as rendering. A complete mining, and placement system is not implemented yet.

LuckeeMiner can load actual **world.dat** files, but it may be limited or have issues.

## Controls

| Input | Action |
|---|---|
| Circle Pad | Move |
| Touch screen drag | Look / camera movement |
| A | Jump |
| L | Break targeted block |
| R | Place block |

## Build

Requires devkitPro with the 3DS toolchain and libctru.

```sh
cd LuckeeMiner
make
```

This produces `LuckeeMiner.3dsx` (and a `.3ds` build if supported by the installed rules). Run it on an original 3DS or in a compatible emulator.

## Project structure

- `source/main.cpp` — application lifecycle, input polling, and initial screen output.
- `include/luckee/input.hpp` — platform-independent input state and control mapping.
- `docs/` — prototype notes, controls, and development roadmap.
- `Makefile` — devkitPro 3DS build configuration.

## Executable Dependencies
**Requires `terrain.png` from the *original jar* OR a *custom terrain.png texture file* to run.**

## Principles

1. Original C++ implementation; use the prototype as a behavioral reference, not code to copy.
2. Target the **original 3DS** first. Do not assume New 3DS CPU features.
3. Prefer simple, measurable systems and profile on real hardware.
4. Keep world generation deterministic and isolate it from rendering.
5. Build in small milestones that can be tested independently.

See [docs/ROADMAP.md](docs/ROADMAP.md) for planned milestones.

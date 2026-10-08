# Development roadmap

## Current target

**rd-132328: RELEASED**

**rd-20090515: IN DEVELOPMENT**

## M0 — Repository and platform foundation

- [x] Create C++ 3DS project structure.
- [x] Establish devkitPro Makefile and application loop.
- [x] Define 3DS control mapping.
- [x] Verify the rd-132328 foundation on original 2DS/3DS hardware.

## M1 — Shared engine foundations

- [x] Native Entity movement, collision, interpolation, and AABB support.
- [x] Native Level storage, lighting, chunk invalidation, and compressed level.dat I/O.
- [x] Native Citro3D terrain and character rendering.
- [x] Original-3DS memory-conscious chunk cache and frustum culling.

## M2 — rd-20090515 world

- [x] Move simulation timing to 20 ticks/sec.
- [x] Replace the flat fallback world with the extracted client's Perlin terrain generation.
- [x] Add the rd-20090515 tile registry: rock, grass, dirt, stone brick, wood, bush.
- [x] Add per-face tile texture selection.
- [x] Add random tile ticking with the reference update budget.
- [x] Add grass spreading and decay behavior.
- [x] Add non-solid bush behavior and crossed-plane rendering.
- [ ] Audit generated terrain visually against the extracted client on hardware.

## M3 — rd-20090515 entities

- [x] Update player physics to the extracted rd-20090515 values.
- [x] Update zombie physics, jump chance, and friction.
- [x] Reduce initial zombie count to 10.
- [x] Add dynamic zombie spawning.
- [x] Remove zombies that fall below y < -100 instead of resetting them.
- [x] Preserve lit/shadow entity rendering passes.
- [ ] Audit zombie front-end behavior against the extracted client on hardware.

## M4 — rd-20090515 particles

- [x] Add Particle entity state and movement.
- [x] Add ParticleEngine ownership and ticking.
- [x] Spawn the reference 4 × 4 × 4 particle grid when a block is destroyed.
- [x] Render camera-facing particle quads using terrain atlas sub-regions.
- [x] Render particles in lit and shadow passes.
- [ ] Verify particle appearance and lifetime on hardware.

## M5 — rd-20090515 front end

- [x] Keep the bottom screen as a runtime debug display during development.
- [x] Preserve block interaction and reference-oriented targeting behavior.
- [x] Add the extracted client's centered crosshair to the top screen.
- [x] Add the selected-block preview to the top-right of the top screen.
- [ ] Audit character and terrain visual parity with the extracted JAR assets.

## M6 — Verification and release

- [ ] Perform a complete extracted-source audit across every Java class in rd-20090515.
- [ ] Build the final release artifact.
- [ ] Test on original 3DS/2DS hardware.
- [ ] Test level.dat save/load and round-trip compatibility.
- [ ] Test block selection, zombie spawning, particle creation, grass ticking, and bush behavior.
- [ ] Document final release controls, assets, and version details.
- [ ] Tag and publish the rd-20090515 release.

## Later work

After rd-20090515 is verified and released, development can continue to the next historical version without mixing later-version mechanics into this target.

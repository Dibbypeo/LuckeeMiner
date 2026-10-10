# Development roadmap

## Current target

**rd-132328: RELEASED**

**rd-20090515: READY FOR MANUAL RELEASE (not published)**

The maintainer has confirmed the current gameplay works. Build, package, tag, and publish this version manually using the existing project workflow.

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
- [x] Check generated terrain in the working game.

## M3 — rd-20090515 entities

- [x] Update player physics to the extracted rd-20090515 values.
- [x] Update zombie physics, jump chance, and friction.
- [x] Reduce initial zombie count to 10.
- [x] Add dynamic zombie spawning.
- [x] Remove zombies that fall below y < -100 instead of resetting them.
- [x] Preserve lit/shadow entity rendering passes.
- [x] Check zombie behavior in the working game.

## M4 — rd-20090515 particles

- [x] Add Particle entity state and movement.
- [x] Add ParticleEngine ownership and ticking.
- [x] Spawn the reference 4 × 4 × 4 particle grid when the 512-particle budget can fit a complete burst.
- [x] Render camera-facing particle quads using terrain atlas sub-regions.
- [x] Render particles in lit and shadow passes.
- [x] Check particle appearance and lifetime in the working game.

## M5 — rd-20090515 front end

- [x] Keep the bottom screen as a runtime debug display.
- [x] Preserve block interaction and reference-oriented targeting behavior.
- [x] Add the extracted client's centered crosshair to the top screen.
- [x] Add the selected-block preview to the top-right of the top screen.
- [x] Check character and terrain visuals with the required assets.

## M6 — Verification and release

- [x] Perform a complete extracted-source audit across every Java class in rd-20090515.
- [x] Bound active zombie and particle allocations for original 3DS stability.
- [x] Remove the world-save compression scratch buffer from the main stack.
- [x] Correct save-compatibility documentation to distinguish payload compatibility from compressed-byte identity.
- [x] Verify normal gameplay and controls.
- [x] Verify level.dat save/load and PC ↔ 3DS ↔ PC payload/layout compatibility.
- [x] Document controls, external assets, save compatibility, and release installation.
- [x] Add release notes for rd-20090515.
- [ ] Build and package the final version locally.
- [ ] Final maintainer review of release files.
- [ ] Tag the rd-20090515 version.
- [ ] Publish the rd-20090515 GitHub Release (manual maintainer action only).

## Later work

After rd-20090515 is released, development can continue to the next historical version without mixing later-version mechanics into this target.

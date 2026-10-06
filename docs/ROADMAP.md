# Development roadmap

## Current milestone

**rd-132211 recreation: COMPLETE ✅**

The original targeted prototype gameplay loop has been recreated in native C++ for original 3DS hardware and has been tested on an original 2DS/3DS without known gameplay, rendering, or stability issues.

## M0 — Repository and platform foundation

- [x] Create C++ 3DS project structure.
- [x] Establish devkitPro Makefile and application loop.
- [x] Define requested control mapping.
- [x] Build with the devkitPro 3DS toolchain and test on original 2DS/3DS hardware.

## M1 — Scene and camera

- [x] Initialize a 3D render target and camera matrices.
- [x] Draw a basic voxel scene using Citro3D.
- [x] Implement touch-drag camera yaw/pitch state with reference sensitivity and pitch limits.
- [x] Match the reference camera eye offset and interpolated player position.
- [x] Add CPU frustum culling without desktop OpenGL dependencies.

## M2 — Player simulation

- [x] Player position/velocity and fixed 60 Hz simulation.
- [x] Circle Pad movement relative to player yaw.
- [x] Gravity, grounded state, and held A-button jump.
- [x] AABB collision.
- [x] Interpolated rendering independent of simulation tick rate.
- [x] Recreate the movement and collision behavior required by the rd-132211 target without adding later-version mechanics.

## M3 — World data

- [x] Block storage and block properties used by the current prototype.
- [x] Chunk storage with cached geometry.
- [x] Deterministic separation between world data and rendering state.
- [x] Level listener/invalidation path for block and lighting changes.
- [x] Robust compressed level.dat loading.
- [x] rd-132211-compatible level.dat saving and PC ↔ 3DS round-trip support.

## M4 — Rendering

- [x] Visible-face chunk meshing.
- [x] External terrain atlas loading and per-face UVs.
- [x] Frustum culling.
- [x] One-chunk-per-rendered-frame rebuild scheduling.
- [x] Bounded chunk mesh cache/render distance for original-3DS memory safety.
- [x] Two-pass terrain rendering with reference-style lighting/fog structure.
- [x] Selected-face highlight and depth-safe rendering.
- [x] Verified stable rendering on original 2DS/3DS hardware.

## M5 — Interaction

- [x] Block targeting / picking using the reference's centered 5×5 selection behavior.
- [x] Selection face highlight with reference-style pulsing brightness.
- [x] L places a block against the targeted face.
- [x] R breaks the targeted block.
- [x] Placement inside the player remains allowed to preserve reference behavior.

## M6 — Prototype recreation

- [x] Recreate the rd-132211 core loop and visual style with original C++ code.
- [x] Match movement, collision, targeting, world presentation, block interaction, and save behavior against the supplied reference.
- [x] Test the finished build on original 2DS/3DS hardware.

## M7 — Later-version research

These milestones are intentionally separate from the completed rd-132211 recreation.

- [ ] Document version-specific world-generation differences.
- [ ] Build deterministic terrain generation behind a versioned interface.
- [ ] Recreate later Alpha/Beta behavior version by version.
- [ ] Investigate the long-term Java Edition seed compatibility goal.
- [ ] Extend the native 3DS engine toward later Minecraft versions without mixing behavior between releases.

## M8 — Future optimization and release work

The current rd-132211 build is already playable and stable. Further optimization is reserved for expanding the project to larger/later versions.

- [x] Package a stable .3dsx build.
- [x] Document installation, controls, assets, and save-file behavior.
- [ ] Profile and tune performance further as later versions are added.
- [ ] Re-evaluate chunk size, draw distance, mesh memory, and update budgets for later versions.

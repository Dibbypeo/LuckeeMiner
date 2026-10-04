# Development roadmap

## M0 — Repository and platform foundation
- [x] Create C++ 3DS project structure.
- [x] Establish devkitPro Makefile and application loop.
- [x] Define requested control mapping.
- [ ] Build with the installed devkitPro toolchain and test on hardware/emulator.

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
- [ ] Add only behaviors that actually exist in rd-132211; no step-up behavior is currently present in the supplied reference.

## M3 — World data
- [x] Block storage and block properties used by the current prototype.
- [x] Chunk storage with cached geometry.
- [x] Deterministic separation between world data and rendering state.
- [x] Level listener/invalidation path for block and lighting changes.
- [x] Robust compressed level.dat loading.

## M4 — Rendering
- [x] Visible-face chunk meshing.
- [x] External terrain atlas loading and per-face UVs.
- [x] Frustum culling.
- [x] One-chunk-per-rendered-frame rebuild scheduling.
- [x] Two-pass terrain rendering with reference-style lighting/fog structure.
- [ ] Profile and tune the renderer on an original 3DS.

## M5 — Interaction
- [ ] Block targeting / picking using the reference's centered 5x5 pick region behavior.
- [x] Selection face highlight with reference-style pulsing brightness.
- [x] L places a block against the targeted face.
- [x] R breaks the targeted block.
- [ ] Prevent invalid placement inside the player (the reference permits this, so parity is intentionally not enforced).

## M6 — Prototype recreation
- [ ] Recreate the reference prototype's complete core loop and visual style with original code.
- [ ] Compare movement, collision, targeting, world presentation, and interaction against the supplied reference.

## M7 — World generation and parity research
- [ ] Document version-specific world-generation differences.
- [ ] Build deterministic terrain generation behind a versioned interface.
- [ ] Investigate the long-term Java Edition seed compatibility goal.

## M8 — Optimization and release
- [ ] Profile on original 3DS hardware.
- [ ] Tune chunk size, draw distance, mesh memory, and update budgets.
- [ ] Package a stable .3dsx build and document controls.
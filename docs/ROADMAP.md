# Development roadmap

## Current milestone

**rd-132211 recreation: COMPLETE**

**rd-132328 recreation: RELEASE CANDIDATE**

The rd-132328 behavior is implemented in native C++ and the documentation now describes the release candidate's controls, assets, save behavior, and version-specific systems. Final release verification remains a maintainer hardware/build step.

## M0 — Repository and platform foundation

- [x] Create C++ 3DS project structure.
- [x] Establish devkitPro Makefile and application loop.
- [x] Define the 3DS control mapping.
- [x] Build with the devkitPro 3DS toolchain and establish the original 2DS/3DS runtime target.

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
- [x] Recreate the movement and collision behavior required by the rd-132328 target without adding later-version mechanics.
- [x] Add the X-button player position reset mapping while preserving the reference reset behavior.

## M3 — World data

- [x] Block storage and block properties used by the current prototype.
- [x] Chunk storage with cached geometry.
- [x] Separation between world data and rendering state.
- [x] Level listener/invalidation path for block and lighting changes.
- [x] Robust compressed level.dat loading.
- [x] rd-132211-compatible level.dat saving and PC ↔ 3DS round-trip support.
- [x] Add SELECT manual saving.

## M4 — Rendering

- [x] Visible-face chunk meshing.
- [x] External terrain atlas loading and per-face UVs.
- [x] Frustum culling.
- [x] One-chunk-per-rendered-frame rebuild scheduling.
- [x] Bounded chunk mesh cache/render distance for original-3DS memory safety.
- [x] Two-pass terrain rendering with reference-style lighting/fog structure.
- [x] Selected-face highlight and depth-safe rendering.
- [x] Verified stable rendering on original 2DS/3DS hardware for the rd-132211 foundation.

## M5 — Interaction

- [x] Block targeting / picking using the reference's centered 5×5 selection behavior.
- [x] Selection face highlight with reference-style pulsing brightness.
- [x] L places a block against the targeted face.
- [x] R breaks the targeted block.
- [x] Placement inside the player remains allowed to preserve reference behavior.
- [x] Document all 3DS controls on the bottom-screen runtime display.

## M6 — Prototype recreation

- [x] Recreate the rd-132211 core loop and visual style with original C++ code.
- [x] Match movement, collision, targeting, world presentation, block interaction, and save behavior against the supplied reference.
- [x] Test the finished rd-132211 build on original 2DS/3DS hardware.

## M7 — rd-132328 upgrade

These milestones document the version-specific changes found in rd-132328.

- [x] Compare rd-132211 and rd-132328 source and isolate version-specific changes.
- [x] Add the shared Entity movement/collision base used by rd-132328.
- [x] Restore randomized player spawning across the baseplate.
- [x] Add 100 wandering zombies with reference movement behavior.
- [x] Add the 64×32 character texture path and animated character rendering.
- [x] Add original-3DS zombie simulation and render culling.
- [x] Correct zombie void reset and randomized spawn behavior.
- [ ] Perform final rd-132328 release build and hardware verification.
- [ ] Continue version-by-version recreation without mixing mechanics between releases.

## M8 — Release and later development

The current source is prepared as an rd-132328 release candidate. Release verification should confirm the final build, required assets, controls, save/load behavior, and target original 3DS/2DS hardware.

- [ ] Build and package the final rd-132328 .3dsx release artifact.
- [x] Document installation, controls, assets, and save-file behavior.
- [x] Document manual save and player reset controls.
- [ ] Perform final hardware verification of the release artifact.
- [ ] Tag the rd-132328 release in the repository.
- [ ] Profile and tune performance further as later versions are added.
- [ ] Re-evaluate chunk size, draw distance, mesh memory, and update budgets for later versions.

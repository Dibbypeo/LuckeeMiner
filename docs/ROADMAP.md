# Development roadmap

## M0 — Repository and platform foundation
- [x] Create C++ 3DS project structure.
- [x] Establish devkitPro Makefile and application loop.
- [x] Define requested control mapping.
- [ ] Build with the installed devkitPro toolchain and test on hardware/emulator.

## M1 — Scene and camera
- [ ] Initialize a 3D render target and camera matrices.
- [ ] Draw a simple test scene using Citro3D.
- [x] Implement touch-drag camera yaw/pitch state with reference sensitivity (0.15 degrees per pixel) and pitch limits (-90 to +90 degrees). Rendering integration remains pending.

## M2 — Player simulation
- [ ] Player position/velocity and fixed-step simulation.
- [ ] Circle Pad movement relative to camera yaw.
- [ ] Gravity, grounded state, and A-button jump.
- [ ] AABB collision and step-up behavior.

## M3 — World data
- [ ] Block IDs and block properties.
- [ ] Chunk storage with explicit memory limits.
- [ ] Deterministic seed handling and save format.

## M4 — Rendering
- [ ] Visible-face chunk meshing.
- [ ] Texture atlas loading and per-face UVs.
- [ ] Frustum culling and chunk rebuild queue.

## M5 — Interaction
- [ ] Block raycast / selection outline.
- [ ] L breaks targeted blocks.
- [ ] R places a block on the targeted face.
- [ ] Prevent placement inside the player and validate block rules.

## M6 — Prototype recreation
- [ ] Recreate the reference prototype's core loop and visual style with original code.
- [ ] Compare movement, collision, world presentation, and interaction against the supplied reference.

## M7 — World generation and parity research
- [ ] Document Java Edition version-specific world-generation differences.
- [ ] Build deterministic terrain generation behind a versioned interface.
- [ ] Investigate the long-term Java Edition seed compatibility goal. Exact parity is a separate, substantial effort and is not implied by a matching seed alone.

## M8 — Optimization and release
- [ ] Profile on original 3DS hardware.
- [ ] Tune chunk size, draw distance, memory use, and update budgets.
- [ ] Package a stable `.3dsx` build and document controls.

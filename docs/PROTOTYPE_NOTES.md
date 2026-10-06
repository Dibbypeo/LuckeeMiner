# Prototype recreation notes

## Reference archive

The supplied `rd-132211` archive is an early RubyDung-era Java prototype. Its code and assets are a historical reference for the game's basic presentation and behavior, not a drop-in engine for the 3DS.

The archive includes Java classes for the game/player, AABB collision, level and chunk storage, tile definitions, tessellation, frustum/level rendering, and a terrain texture. LuckeeMiner will implement its own equivalents in C++.

## Porting constraints

- The target is the **original 3DS**, not just the faster New 3DS.
- Use native 3DS graphics APIs (Citro3D/libctru), not desktop OpenGL assumptions.
- Keep chunk memory and geometry budgets explicit.
- Preserve a deterministic separation between seed/world data and graphics state.
- Do not import copied prototype source. Asset use must be checked separately for licensing and attribution before bundling.

## First recreation targets

1. Basic scene and camera.
2. Player movement, gravity, jumping, and AABB collision.
3. Small deterministic block world and chunk storage.
4. Block targeting and L/R block editing.
5. Visible-face meshing and texture atlas support.
6. Terrain generation and performance passes.

## Original 3DS stability notes

- Cached chunk VBOs must remain bounded because the complete exposed surface of the 256 x 64 x 256 prototype world is too large to keep resident indefinitely on original 3DS.
- Chunk invalidation keeps edited geometry dirty so changed blocks are rebuilt when their chunks become visible.
- Renderer rebuild buffers and player collision query storage are reused to reduce repeated heap allocation and fragmentation.

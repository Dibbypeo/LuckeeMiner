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

## rd-132328 additions

The rd-132328 reference adds a shared Entity base, 100 wandering zombies, and the character model/texture system. LuckeeMiner implements these with native C++ and Citro3D while preserving the reference behavior.

Player spawning now uses a per-process random seed so the player can begin at different X/Z positions across the baseplate instead of repeating the same default std::rand() sequence.

The external character atlas is expected at `assets/textures/char.png` and is 64×32. The renderer builds the six-part zombie model into a reusable linear-memory vertex buffer so the 3DS does not allocate a new GPU buffer for every zombie every frame.

## Original 3DS stability notes

- Cached chunk VBOs must remain bounded because the complete exposed surface of the 256 x 64 x 256 prototype world is too large to keep resident indefinitely on original 3DS.
- Chunk invalidation keeps edited geometry dirty so changed blocks are rebuilt when their chunks become visible.
- Renderer rebuild buffers and player collision query storage are reused to reduce repeated heap allocation and fragmentation.

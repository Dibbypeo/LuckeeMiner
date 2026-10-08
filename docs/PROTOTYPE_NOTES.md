# Prototype recreation notes

## Reference

LuckeeMiner is developed against historical RubyDung / Minecraft prototype clients one version at a time. The current target is **rd-20090515**. The supplied Java source is treated as behavioral documentation for the front-end and simulation rules. LuckeeMiner uses an original C++ implementation rather than copying the Java implementation.

Reference repository:

https://github.com/thecodeofnotch/rd-132328

## Porting constraints

- The target is the **original 3DS**, not only the faster New 3DS.
- Use native 3DS graphics APIs (Citro3D/libctru), not desktop OpenGL assumptions.
- Keep chunk memory and geometry budgets explicit.
- Keep world data separate from renderer state.
- Reproduce version-specific behavior from the selected reference instead of mixing later Minecraft mechanics into rd-132328.
- Do not copy the Java source into the project. The C++ code recreates the same visible and behavioral results using native 3DS internals.

## rd-132211 foundation

The rd-132211 foundation provides the basic voxel world and presentation layer used by the current release candidate:

- 256 × 64 × 256 world dimensions.
- Grass/rock terrain using the reference terrain atlas.
- Visible-face voxel meshing.
- Reference-style lighting and two terrain render layers.
- Fixed 60 ticks/sec simulation with interpolated rendering.
- Player movement, gravity, jumping, and AABB collision.
- Centered 5×5 block targeting.
- Block placement and destruction.
- GZIP level.dat loading/saving using the original raw block layout.

The save data remains compatible with the rd-132211 block format so a world can be transferred between the PC prototype and LuckeeMiner without a conversion step.

## rd-132328 additions

The rd-132328 reference adds a shared Entity base, randomized entity spawning, 100 wandering zombies, and the six-part human character model.

LuckeeMiner recreates:

- Shared Entity movement, collision, rotation, interpolation, and random spawning.
- Player inheritance from Entity.
- Randomized player spawning across the X/Z baseplate.
- 100 zombie entities.
- Zombie wandering, jumping, gravity, friction, and void reset.
- The six character cubes: head, body, two arms, and two legs.
- The reference character dimensions and char.png atlas offsets.
- The reference model scale, Y inversion, body rotation, interpolation, and animation formulas.
- Zombie rendering between the bright and dark terrain passes.
- Original-3DS distance and frustum culling for zombie work.
- A required external assets/textures/char.png character atlas at 64 × 32 pixels.

The historical Java zombie constructor receives (0, 0, 0) from RubyDung, although its Entity constructor has already initialized a randomized position. LuckeeMiner keeps a valid position/AABB pair instead of reproducing the temporary stale bounding-box mismatch that would otherwise cause an artificial visual teleport during initialization.

## 3DS control adaptations

The desktop reference uses keyboard/mouse input. LuckeeMiner maps the same gameplay actions to 3DS controls:

- Circle Pad → movement.
- Touch screen drag → camera look.
- A → jump.
- L → place.
- R → break.
- SELECT → manual save, corresponding to the reference's Enter-key save action.
- X → player position reset, corresponding to the reference player's reset-position key.
- START → exit, with a normal shutdown save.

These bindings are control adaptations, not new gameplay systems.

## Original 3DS stability notes

- Cached chunk VBOs are bounded because the complete exposed surface of the 256 × 64 × 256 prototype world is too large to keep resident indefinitely on original 3DS.
- Chunk invalidation keeps edited geometry dirty so changed blocks are rebuilt when their chunks become visible.
- Renderer rebuild buffers and player collision query storage are reused to reduce repeated heap allocation and fragmentation.
- Zombie simulation and rendering are limited to nearby entities so the original 3DS does not spend CPU/GPU time on distant off-screen zombies.
- The renderer uses Citro3D-native buffers and transforms instead of desktop OpenGL immediate mode.

## Release status

The rd-132328 source recreation is complete and documented as a release candidate. Final release verification consists of building the intended release artifact and confirming the current behavior on the maintainer's target original 3DS/2DS hardware.

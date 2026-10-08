# rd-132328 recreation notes

## Changes from rd-132211

The rd-132328 reference keeps the rd-132211 level, chunk, lighting, AABB, and player-physics systems and adds an entity/character layer.

LuckeeMiner now implements:

- Shared Entity movement, collision, rotation, interpolation, and random spawning.
- Player inheritance from Entity.
- Randomized player spawning across the X/Z baseplate.
- 100 zombies created using the reference constructor position of (128, 0, 128).
- Zombie wandering, jumping, gravity, friction, and reset-above-height behavior.
- Native Citro3D character geometry matching the reference cube dimensions and UV layout.
- Zombie head, arm, and leg animation.
- External 64×32 char.png loading.
- Zombie rendering between the two terrain passes, matching the reference render order.
- Distance and frustum culling for zombie rendering on original 3DS hardware.
- Nearby-only zombie simulation using the same 64-block range as terrain rendering.

## Historical behavior preserved

The rd-132328 Entity reset routine chooses random X/Z coordinates and starts an entity three blocks above the level depth.

LuckeeMiner initializes zombies directly from that valid randomized position instead of reproducing the prototype's temporary stale bounding-box mismatch. This prevents zombies from appearing at y=0 and then visibly teleporting to their reset height on the first tick. Void reset also follows the reference's y < -100 condition.

The historical Zombie constructor overwrites x/y/z after calling the Entity constructor without rebuilding the inherited AABB. LuckeeMiner preserves this ordering rather than silently fixing the prototype.

The original character model consists of a head, body, two arms, and two legs. LuckeeMiner reproduces the same box dimensions, atlas offsets, model scale, Y inversion, body rotation, interpolation, and animation formulas with native 3DS vertex buffers.

The original simulates and renders all zombies, but the 3DS port deliberately limits simulation and rendering to nearby zombies because reproducing all 100 off-screen entities wastes CPU and GPU time on the original hardware.

The original uses LWJGL/OpenGL immediate-mode rendering. LuckeeMiner does not copy that renderer. It recreates the resulting geometry, texture mapping, transforms, animation, ordering, and behavior using Citro3D.

## Verification

The rd-132211 build was previously verified on original 2DS/3DS hardware. The rd-132328-specific source changes are implemented, but they still require hardware verification.

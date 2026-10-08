# rd-132328 recreation notes

## Scope

LuckeeMiner's current release candidate targets the rd-132328 prototype. The implementation uses the historical Java source as a behavioral reference and recreates the same front-end behavior with native C++ and Citro3D on the original Nintendo 3DS family.

## Changes from rd-132211

The rd-132328 reference keeps the rd-132211 level, chunk, lighting, AABB, and player-physics foundations and adds an entity/character layer.

LuckeeMiner implements:

- Shared Entity movement, collision, rotation, interpolation, and randomized spawning.
- Player inheritance from Entity.
- Randomized player spawning across the X/Z baseplate.
- 100 zombies created from the same dummy constructor arguments used by RubyDung: (0, 0, 0).
- Zombie wandering, jumping, gravity, friction, and reset after falling below y < -100.
- Native Citro3D character geometry matching the reference cube dimensions and UV layout.
- Zombie head, arm, and leg animation.
- External 64 × 32 char.png loading.
- Zombie rendering between the two terrain passes, matching the reference render order.
- Distance and frustum culling for zombie rendering on original 3DS hardware.
- Nearby-only zombie simulation to avoid spending CPU time on distant entities.
- SELECT manual world saving.
- X-button player position reset.
- Bottom-screen control documentation so the runtime mapping is visible to players.

## Historical behavior preserved

The rd-132328 Entity reset routine chooses random X/Z coordinates and starts an entity three blocks above the level depth.

LuckeeMiner initializes zombies from the randomized position established by its Entity constructor instead of reproducing the reference's stale bounding-box mismatch after the Java constructor overwrites x/y/z. This keeps the displayed position and collision box coherent while preserving the reference's intended randomized spawning behavior.

Void reset follows the reference y < -100 condition.

The original character model consists of a head, body, two arms, and two legs. LuckeeMiner reproduces the same box dimensions, atlas offsets, model scale, Y inversion, body rotation, interpolation, and animation formulas with native 3DS vertex buffers.

The original simulates and renders all zombies. The 3DS recreation deliberately limits simulation and rendering to nearby zombies because the original 3DS has much tighter CPU/GPU budgets.

The original uses LWJGL/OpenGL immediate-mode rendering. LuckeeMiner does not copy that renderer. It recreates the resulting geometry, texture mapping, transforms, animation, ordering, and behavior using Citro3D.

## 3DS control mapping

The desktop reference uses keyboard/mouse actions that are mapped to the 3DS as follows:

| 3DS input | Action | Reference action |
|---|---|---|
| Circle Pad | Move | Keyboard movement |
| Touch drag | Look | Mouse movement |
| A | Jump | Jump key |
| L | Place block | Left mouse button |
| R | Break block | Right mouse button |
| SELECT | Save world | Enter |
| X | Reset player position | Player reset key |
| START | Exit | Escape / window close |

SELECT writes the current block array to level.dat immediately. START also saves during normal shutdown.

X causes the player to run the reference-style position reset before continuing that simulation tick, so the player is placed at a new randomized X/Z position and three blocks above the level depth.

## Assets

The rd-132328 character atlas is required at assets/textures/char.png.

The atlas is expected to be 64 × 32 pixels.

The terrain atlas remains assets/textures/terrain.png with the existing 256 × 256 terrain layout.

## Verification and release status

The source recreation is complete enough to serve as the rd-132328 release candidate. The final maintainer release step is to build the intended .3dsx, install the documented assets, and verify the final artifact on original 3DS/2DS hardware before tagging the public release.

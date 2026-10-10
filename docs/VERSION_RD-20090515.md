# rd-20090515 recreation notes

## Scope

LuckeeMiner is now being advanced from rd-132328 to the rd-20090515 Pre-Classic client.

For this version, the supplied Java files extracted directly from the historical client JAR are the primary behavioral reference. The repository's native C++ implementation is recreated from that source rather than copying or decompiling Java into the project.

## Major version changes

Compared with rd-132328, rd-20090515 changes the game loop and adds several new world systems:

- Simulation rate changes from 60 ticks/sec to 20 ticks/sec.
- Initial zombie population changes from 100 to 10.
- Zombies can be spawned dynamically by the player.
- Zombies falling below y < -100 are removed instead of being reset.
- Player and zombie physics become much faster and use stronger gravity and jumping.
- The flat world is replaced by Perlin-noise terrain generation when no save exists.
- The tile registry expands to rock, grass, dirt, stone brick, wood, and bush.
- Tile textures are selected independently per face.
- Grass becomes a ticking tile that can spread in sunlight and turn into dirt without light.
- Bushes are non-solid, do not block light, render as crossed planes, and disappear when their conditions are no longer valid.
- The world performs random tile ticks every game tick.
- Breaking a block creates a 64-particle terrain-textured effect when the bounded particle pool has room; at most 512 particles can be active.
- Particles use gravity, friction, lifetime, random size, and camera-facing billboard rendering.
- Zombies and particles are rendered separately in lit and shadow passes.
- The desktop client adds number-key block selection, a zombie-spawn key, a crosshair, and a selected-block HUD preview.

## Current 3DS adaptation

LuckeeMiner preserves the established 3DS controls where they remain useful and adapts desktop-only controls:

| 3DS input | Action |
|---|---|
| Circle Pad | Move |
| Touch drag | Look |
| A | Jump |
| L | Place the selected block |
| R | Break the targeted block |
| D-Pad Up | Select previous placeable block |
| D-Pad Down | Select next placeable block |
| Y | Spawn a zombie at the player's position |
| SELECT | Save the world |
| X | Reset player position |
| START | Save and exit |

The bottom screen remains a debug display rather than a recreation of a full desktop HUD. The top-screen crosshair and selected-block preview are implemented with native Citro3D geometry.

## Implemented in the current branch

The current native implementation has already introduced:

- 20 TPS simulation timing.
- rd-20090515 player physics.
- rd-20090515 zombie physics and removal behavior.
- Ten initial zombies with the source's (128, 0, 128) constructor followed by resetPos().
- Dynamic zombie spawning, with a 32-active-zombie safety budget for original 3DS hardware.
- A generalized tile registry.
- Rock, grass, dirt, stone brick, wood, and bush tile definitions.
- Per-face atlas texture selection.
- Perlin-noise terrain generation.
- Random tile ticking.
- Grass spreading and decay.
- Non-solid bush behavior.
- Entity resize/reset/removal/light state.
- Particle and particle-engine simulation.
- Block-destruction particle spawning.
- Native particle billboards.
- Lit/shadow rendering passes for zombies and particles.
- Alpha testing for transparent character/bush textures.

## 3DS-specific implementation choices

The desktop reference uses LWJGL/OpenGL and display-list rendering. LuckeeMiner instead uses Citro3D vertex buffers, chunk caching, frustum culling, and bounded linear-memory allocations.

The native RNG reimplements Java's 48-bit java.util.Random algorithm, including bounded nextInt and nextDouble arithmetic. Default seeding remains runtime-dependent, just as Java's no-argument Random constructor is runtime-dependent, but every subsequent RNG operation uses the Java algorithm.

The renderer keeps a bounded world cache and a conservative render distance for original 3DS hardware. Dynamic zombies and particles also have explicit active-count budgets to prevent unbounded heap/linear-memory growth. These are original-3DS stability adaptations rather than new gameplay systems.

## Reference asset paths

The extracted client provides:

- terrain.png at 256 × 256.
- char.png at 64 × 32.

LuckeeMiner continues to expect these as external assets under:

    SD:/3ds/assets/textures/terrain.png
    SD:/3ds/assets/textures/char.png

The project does not automatically bundle historical game assets.

## Save-file compatibility

The native reader and writer use the same raw block payload and `(y * height + z) * width + x` layout as the Java client. The writer emits a valid GZIP member with raw DEFLATE, CRC-32, and the 32-bit uncompressed size trailer. LuckeeMiner uses `Z_BEST_SPEED` for 3DS performance, unlike Java's default compression level, so the compressed DEFLATE bytes are not expected to be identical. Compatibility is at the GZIP/payload level, not compressed-byte identity.

## Verification status

The source-level audit covers every extracted Java class in the supplied rd-20090515 archive and the corresponding native implementation or intentional Citro3D/libctru replacement. The native review also checked save/load failure paths, stack use in the save routine, tile index validation, renderer resource ownership, and entity/effect allocation bounds. See `docs/AUDIT_RD-20090515.md` for findings.

This is not yet a completed release. A successful final devkitPro build, PC ↔ 3DS ↔ PC save/load round-trip, and original 3DS/2DS gameplay and performance checks remain release blockers.

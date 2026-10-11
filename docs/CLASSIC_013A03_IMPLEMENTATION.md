# Classic c0.0.13a_03 implementation status

This branch starts the C++/Citro3D port from the released `rd-20090515`
engine without changing the published Pre-Classic branch.

## Implemented in the first engine pass

- Classic-style improved Perlin, octave and distorted noise primitives.
- The six world-generation stages used by `c0.0.13a_03`: raising, eroding,
  soiling, carving, watering and melting.
- Tile registry entries for the special boundary block (7), flowing/still
  water (8/9) and flowing/still lava (10/11).
- Fluid random-tick flow, stationary-to-flowing transitions, limited lateral
  spread and water/lava rock reactions.
- Six-neighbor tile-change notifications plus a raw setter for fluid-internal
  transitions.
- The Classic random block-update interval and LCG coordinate selection.
- Classic lighting-height convention, horizontal/bottom boundary collision,
  and dry/water/lava player movement constants.
- Zero zombies at world startup, matching the original Classic client.

## Still incomplete

- **Rendering:** fluids currently pass through the existing chunk mesher. A
  dedicated water transparency pass, fluid surface geometry, immersion fog,
  horizon rock/water and the Classic resource set are still required.
- **Save compatibility:** the engine still writes its raw GZIP block payload.
  The Classic metadata header, metadata parsing, saved dimensions and legacy
  load fallback have not yet been integrated.
- **Interface:** generation-stage progress, pause/menu flow and world
  selection are not yet implemented. The current controls remain a 3DS-native
  adaptation of the earlier engine.
- **Physics:** liquid detection and the major movement constants are present,
  but Classic's liquid wall-step behavior still needs implementation and
  comparison against the bytecode.
- **Validation:** no devkitPro toolchain is available in this editing
  environment. A 3DS build and hardware/emulator tests are required before this
  branch should be considered release-ready.

The original compiled `.class` files supplied for this milestone are the
reference for resolving decompiler omissions. Native code should reproduce the
reference behavior, not invalid constructs emitted by a decompiler.

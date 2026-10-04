# Controls

The current control contract for LuckeeMiner:

- **Circle Pad:** player movement relative to the player's horizontal facing.
- **Touch screen drag:** camera look. Horizontal and vertical drag use the reference yaw/pitch convention.
- **A:** jump while held and grounded, matching the reference simulation behavior.
- **L:** place a block against the currently targeted face.
- **R:** break the currently targeted block.
- **START:** exit the homebrew application.

The selected face uses the reference's pulsing brightening effect. The simulation runs at a fixed 60 ticks/sec even when rendering falls behind. Rendering uses interpolation between simulation ticks.
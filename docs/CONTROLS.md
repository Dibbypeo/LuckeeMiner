# Controls

The current control contract for LuckeeMiner:

- **Circle Pad:** player movement relative to the player's horizontal facing.
- **Touch screen drag:** camera look. Horizontal and vertical drag use the reference yaw/pitch convention.
- **A:** jump while held and grounded, matching the reference simulation behavior.
- **L:** break the currently targeted block. Input is wired, but targeting/editing is not implemented yet.
- **R:** place the selected block against the targeted face. Input is wired, but targeting/editing is not implemented yet.
- **START:** exit the homebrew application.

The simulation runs at a fixed 60 ticks/sec even when rendering falls behind. Rendering uses interpolation between simulation ticks.
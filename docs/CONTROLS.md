# Controls

The initial control contract for LuckeeMiner:

- **Circle Pad:** player movement (forward/back and strafe).
- **Touch screen drag:** camera look. The input layer reports drag deltas; camera sensitivity and pitch clamping belong to the camera system.
- **A:** jump (press edge, not continuous hold).
- **L:** break the currently targeted block.
- **R:** place the selected block against the targeted face.
- **START:** exit the homebrew application.

Block actions are currently input signals only. They do not mine or place blocks until the raycast, world, and block-edit systems exist.
# Controls

LuckeeMiner uses a Nintendo 3DS control mapping for the rd-132328 recreation. The controls are shown on the application's bottom screen while the game is running.

| Input | Action |
|---|---|
| **Circle Pad** | Move relative to the player's horizontal facing |
| **Touch screen drag** | Look / camera movement |
| **A** | Jump while held and grounded |
| **L** | Place a block against the currently targeted face |
| **R** | Break the currently targeted block |
| **SELECT** | Save the current world immediately |
| **X** | Reset the player's position |
| **START** | Save on exit and leave the homebrew application |

## Behavior notes

The Circle Pad movement follows the player's horizontal facing. Touch dragging uses the reference yaw/pitch convention and the same sensitivity and pitch limits.

A is checked while held, so holding it while grounded can trigger the reference-style jump behavior.

L and R are edge-triggered actions. They affect the block selected by the centered 5×5 target region.

SELECT performs the manual world save provided by the reference's Enter-key save action. The save is written to level.dat.

X performs the player's position reset before that simulation tick continues, matching the position-reset behavior of the reference player. The reset chooses a new randomized X/Z location and starts the player three blocks above the level depth.

START leaves the application. LuckeeMiner also saves the world during normal shutdown so the current world state is preserved when exiting normally.

Rendering uses interpolation between fixed simulation ticks, so the control display and simulation timing are independent of the render frame rate.

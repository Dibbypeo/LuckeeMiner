# Controls

LuckeeMiner currently uses the following Nintendo 3DS control mapping for the rd-20090515 recreation. The bottom screen displays the same mapping alongside runtime debug information.

| Input | Action |
|---|---|
| **Circle Pad** | Move relative to the player's horizontal facing |
| **Touch screen drag** | Look / camera movement |
| **A** | Jump while held and grounded |
| **L** | Place the selected block against the targeted face |
| **R** | Break the targeted block |
| **D-Pad Up** | Select the previous placeable block |
| **D-Pad Down** | Select the next placeable block |
| **Y** | Spawn a zombie at the player's position (up to 32 active zombies) |
| **SELECT** | Save the current world immediately |
| **X** | Reset the player's position while held |
| **START** | Save on exit and leave the homebrew application |

## Block selection

The extracted desktop client uses number keys to select rock, dirt, stone brick, wood, and bush. LuckeeMiner adapts that selection to D-Pad cycling:

1. Rock (ID 1)
2. Dirt (ID 3)
3. Stone brick (ID 4)
4. Wood (ID 5)
5. Bush (ID 6)

D-Pad Up moves backward through the list and D-Pad Down moves forward.

The Circle Pad uses an independent 18% deadzone on each axis. Small accidental offsets are ignored, including vertical drift while moving horizontally.

## Zombie spawning

The extracted desktop client uses G to spawn a zombie at the player's current position. LuckeeMiner uses Y for that gameplay action. To protect the original 3DS's limited memory, additional requests are ignored while 32 zombies are active.

## Saving and reset

SELECT performs the manual save action corresponding to the reference client's Enter key.

START exits normally and also saves level.dat.

X performs the established LuckeeMiner player reset adaptation. It uses the rd-20090515 Entity reset routine: a randomized X/Z location and a starting Y of depth + 10, then continues the simulation tick.

## Notes

The desktop reference also contains a small top-screen crosshair and selected-block preview. The 3DS bottom screen remains a debug display during development while those version-specific front-end elements are implemented.

A and X are held rather than edge-triggered. L, R, D-Pad selection, Y, and SELECT are edge-triggered adaptations.

For original-3DS stability, LuckeeMiner keeps at most 32 active zombies and 512 active particles. A block break creates the full 64-particle burst only when the particle pool has room for all 64; otherwise that burst is skipped.

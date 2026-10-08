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
| **Y** | Spawn a zombie at the player's position |
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

## Zombie spawning

The extracted desktop client uses G to spawn a zombie at the player's current position. LuckeeMiner uses Y for that same gameplay action.

## Saving and reset

SELECT performs the manual save action corresponding to the reference client's Enter key.

START exits normally and also saves level.dat.

X performs the established LuckeeMiner player reset adaptation. It uses the rd-20090515 Entity reset routine: a randomized X/Z location and a starting Y of depth + 10, then continues the simulation tick.

## Notes

The desktop reference also contains a small top-screen crosshair and selected-block preview. The 3DS bottom screen remains a debug display during development while those version-specific front-end elements are implemented.

A and X are held rather than edge-triggered. L, R, D-Pad selection, Y, and SELECT are edge-triggered adaptations.

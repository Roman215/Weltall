# The world map

A `world map E` document adds locations, names, arrival points and minimap dots to era E. Eras range
from 0 to 8; Disc 2 uses era 8. It can also temporarily hide the party's ship at specified story values.
The additions load with the world map scene.

`Content/Examples/disc2/mass-driver-world-map.txt` adds the Mass Driver ruins on Taura's land:

```
world map 8

vehicle away at story 253

message MassDriver
{
Mass Driver
}

place on foot x 29640 z 16180 width 520 height 400 to map 732 entrance 0 name MassDriver
arrival 40 x 29700 z 16620

dot 21
```

## Locations

`place on foot x X z Z width W height H to map M entrance E name N` defines a rectangular entry area.
Coordinates use whole world units, and the rectangle includes its edges. Pressing confirm inside it
enters field map M through entrance E.

Replace `on foot` with `in gears` or `in vehicle` for those travel modes. Add `on touch` before `to`
to enter automatically when the party reaches the area; this works only on foot.

Include walkable ground in the rectangle. Town and ruin models aren't walkable, so entry areas need
to cover the surrounding ground, as the original locations do. Original locations take priority if
areas overlap.

The name can reference a message in your document or an existing scene name by number (`name #20`).
It appears while the party is inside the area and must fit on one line, up to 16 letters.

## Arrival points

`arrival N x X z Z` sets the position used when a field script exits with `ExitToWorldMap #1024 #N`.
Choose an unused arrival number and place it just outside your entry area, so pressing confirm after
leaving doesn't immediately send the player back in.

## Minimap dots

`dot N` shows an existing yellow minimap dot while the document is active. Select toggles the
minimap. The game has 24 dots, numbered 0 to 23, at fixed positions. Normally, scripts enable them
with `SetVarBit var[1FC] #N`, or `var[1FE] #N-16` for dots 16 and above.

The original game never enables dots 14, 19, 21 or 22. Dot 21 is at the Mass Driver ruins, so the
Mass Driver document uses it.

Use `dot N` instead of setting the saved flag in a script. A saved flag would remain set after your
sequence ends, changing the state expected by the original story.

You can't move a dot or add one at a new position yet. A location away from the existing dots won't
have a minimap marker.

## Temporarily hiding the ship

`vehicle away at story S` hides the ship at story value S. Use `S-T` for a range. During those values,
the ship isn't drawn, can't be boarded and has no minimap dot. If the party would normally start
aboard it, they start on foot or in Gears instead. The saved ship position and state remain unchanged,
so the ship returns where it was when the story brings it back.

The Mass Driver sequence uses this because Disc 1 ends with the party aboard the flying ship near
Taura's forest. Without it, Elly and Emeralda would start their trip aboard the ship.

## Random encounters

Random battles use the region's original encounter table for the current story progress. Taura's
land is one region, and its encounters at this point are the weakest from Disc 1. New world map
locations can't override that table yet.

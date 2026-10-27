# A map of your own

Field map IDs 730 to 1023 are available for new maps. A document can reuse an existing map's geometry,
scenery, cameras, textures, walkmesh, zones and encounters. The result supports normal entrances,
exits and saves. Choose `from` or `over` depending on how much of the original scripting you need.

## `from`: reuse the map assets and write new scripts

```
map 733 from 384
encounters from 629

message Sealed
{
It won't open.
}

actors 19
unsigned 00A-00E 026-02A 030 036
entrances
entrance x -595 z -571 layer 0 camera keep facing keep
entrance x 441 z -590 layer 0 camera keep facing keep

actor 0 script 0:
    ...
```

A `from` document supplies the entire script section: actor count, unsigned variable declarations,
entrance table and actor scripts. Messages are numbered from 0 in document order. Use
`encounters from K` to take random battle formations and their weights from map K instead of the base map.

The Mass Driver's three rooms are examples:
`Content/Story/disc2/mass-driver-airlock.txt` uses map 382, `mass-driver-corridor.txt` uses map 384 with
map 629's encounters, and `mass-driver-control-room.txt` uses map 369. Compare them with the base map
listings, such as `WeltallMapText 382`, to see how the geometry is reused with new scripts.

### Actors

A map archive defines script actors first, followed by scenery. Your document uses those actor
definitions in the same order and can declare the same number of script actors or fewer. Any remaining
definitions become scenery. Actor definitions also specify models; for example, door actors animate
their models by scrolling the door texture. Include enough actors for the doors and characters you
need, and check their original scripts.

The Mass Driver rooms follow this pattern:

- **Actor 0 initializes the map.** Use `SetHiddenModel`, the base map's `CameraSet` values,
  `DisableTalk`, `MusicLoad` and, if needed, `SetEncounterRate`. The Zeboim facility halls use `#900 #2`.
- **Actors 1 to 11 represent the party.** Actor N + 1 is character N (`SetPartyCharacter #N`,
  `SetMessageFace #N`). Their `script 1` runs `PlayerControlStep` for movement and following. To hold
  the party during a scene, have that script check a scratch variable; see `If Scene == #0 else Held`
  in `mass-driver-control-room.txt`.
- **Doors use an invisible trigger actor.** Set up its position and collision box with `SetHiddenModel`,
  `SetPosition`, `SetStateFlags 0 0x2000`, `SetStateFlags 3 0x4`, `SetCollisionSize` and
  `SetCollisionFlags #1`. Its `script 2` handles confirm, and `script 3` handles walking into the box.
  These scripts play the sound, start the door animation (`ScrollModelUV 2 0`, one step per tick for
  fourteen ticks), move the leader through with `WalkLeaderTo`, and leave via `ChangeMap` or
  `ExitToWorldMap`. Copy trigger positions from the base map. The box also blocks scripted movement,
  so place arriving characters inside the doorway, past the box.
- **Remove unwanted actors** with `RemoveActor self` in their init script.
- **Define scripts 0 to 3 for every actor**, using `End` for unused scripts. An omitted script points
  to the first byte of the bytecode, which is the entrance table marker rather than an instruction.

You can enter a new map through `ChangeMap #733 #0` in another map's script, a
[world map location](the-world-map.md), or a save made there.

### Sprites from another map

A map's sprites are its own: `SetModel #N` takes slot N of the base map's sprite files. A map of its
own can take a slot from another disc map instead:

```
map 740 from 93
sprite 9 from map 164 slot 2
```

Slot 9 then holds map 164's slot 2: its sprite file and its images, drawn at slot 9's place in
VRAM with its own palette. `SetModel #9` on any actor shows it. The Nisan assault uses this for
the Solarian uniforms of Renk's men, which Nisan's maps lack and the Shevat hangar (map 164) has.

- The slot taken must be one the base map has a sprite file for; the other map's slot must take
  its images from its archive (most do; `WeltallSlotText`, if you build it, or the tests tell you
  which).
- Each slot is taken once. `sprite` lines belong to a map of its own only.
- The images are uploaded after the base map's, so they show whatever the base put there.

## `over`: keep the base scripts and patch them

```
map 730 over 613
encounters from 273

message Hurry
{
...
}

detour @0x00FD to Rebuilt SetGearModel #0
Rebuilt:
    SetGearModel #42
    Jump @0x0100

actor 8 script 1:
    ...
```

An `over` document keeps the base map's scripts and messages, then applies a patch as described in
[Changing a map](changing-a-map.md). New messages follow the existing ones. Use `expect` and `detour`
to patch instructions, or replace individual scripts. This is useful when you need the base map's
actor setup, cameras and effects.

`Content/Story/disc2/rematch-lead-in.txt` and `rematch-aftermath.txt` reuse map 613, the sky scene from
the final chase on Disc 1. They keep its cockpits, Gears and cameras while adding new dialogue.

The base scripts still run at the story value you enter with. Read their conditions and disable any
behavior that doesn't belong in your scene, such as an original story event or an unsupported Gear
pose. Replace the relevant actor script or prevent it from starting.

## Using Disc 1 maps on Disc 2

The game starts with Disc 1 and reads its files from that disc. Disc 2's duplicate files are used for
movies. A new map on Disc 2 can therefore use any Disc 1 map as its base.

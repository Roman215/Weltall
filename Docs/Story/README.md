# Writing for the story

Weltall loads story additions from plain text files at startup. You can change scripts, add dialogue
or create playable scenes without recompiling. These guides use the files in `Content/Story` and the
example scenes in `Content/Examples` as examples.

Keep original game scripts and text out of these files. Reference the map, instruction offset or
message you need, and include only your additions. Weltall reads the original content from your disc.

## Packs and documents

Each folder in `Content/Story` is a pack, and each `.txt` file is a document. Packs load in folder-name
order, with documents in filename order. If a document can't be read, the game stops at startup and
shows the filename and line number.

| Pack | Purpose |
|---|---|
| `additions` | Small changes controlled by individual settings, such as the RPS helper and staging fix |

A Disc 2 expansion goes in `Mods/<your expansion>/disc2`. It plays when you select **Extended** at
the start of Disc 2 ([A stretch of story](a-stretch-of-story.md)). The two example scenes in
`Content/Examples` form one expansion. **Options → Modding → Extract example scenes** copies it
into `Mods`. To write your own, start with [Writing an expansion](writing-an-expansion.md).

The first line identifies the document type:

| First line | Purpose | Guide |
|---|---|---|
| `map 7` | Patch an existing map's scripts and messages | [Changing a map](changing-a-map.md) |
| `map 732 from 382` | Reuse a disc map's assets with entirely new scripts | [A map of your own](a-map-of-your-own.md) |
| `map 730 over 613` | Reuse a disc map's assets and scripts, then patch them | [A map of your own](a-map-of-your-own.md) |
| `map 617 whole` | A disc map's scripts and messages given in full, as Export Disc 2 writes them | [Modding](../Modding.md#starting-from-export-disc-2) |
| `battle event 19` | Change where battle narration starts and add an introduction | [A battle's narration](a-battles-narration.md) |
| `battle event 19 whole` | A battle event's messages given in full, as Export Disc 2 writes them | [Modding](../Modding.md#starting-from-export-disc-2) |
| `world map 8` | Add locations, names and arrival points, or temporarily hide the ship | [The world map](the-world-map.md) |
| `way back` | Map expansion saves to a compatible point in the original story | [A stretch of story](a-stretch-of-story.md) |
| `subtitles` | Add titles within the original chapters | [Subtitles](subtitles.md) |

[A stretch of story](a-stretch-of-story.md) explains how these fit together: branch from the original
story, play through new scenes and maps, then return with the game state matching the original.

## Script and message syntax

Scripts use the instruction names and operands printed by `WeltallMapText`.
`Source/WeltallCore/Public/Field/EventAssembly.h` describes labels, `If`, operands, `define` and `data`.
`Source/WeltallCore/Private/Field/EventInstructionRows*.cpp` lists the instructions and their operands.

For message syntax, see `Source/WeltallCore/Public/Text/MesText.h`. Messages use plain characters,
line breaks and markup such as `{page}`, `{wait 0A}` and `{name 00}`.

## Inspecting existing maps

`WeltallMapText` builds alongside the core tests (see the main README). It prints a field map from
your discs in the same format used by story documents:

```sh
WeltallMapText 616        # map 616 from the first disc, as text
WeltallMapText 369 369 1  # map 369, numbered 369, from disc 1
```

The listing includes instruction offsets for `expect` and `detour`, and messages named `msg0`,
`msg1`, etc. It contains original game scripts and text, so use it as a reference without committing it.

In the game, **Options → Modding → Export Disc 2** writes the second disc's maps into a new expansion
folder in `Mods`. Each document contains a complete map (`map 617 whole`). Edit one and restart to play
it in place of the disc's map ([Modding](../Modding.md#starting-from-export-disc-2)). The export contains
the game's scripts and text; use it as a reference and share only your changes.

Use the entrance coordinates and actor positions (`SetPosition`, `WalkTo`) in the listing to place
characters and doors. Placing an actor off the walkmesh stops the game with an error identifying the actor.

## Checking your work

The core tests read each document against your discs. They check that:

- Patches match their target maps, with bug fixes both enabled and disabled.
- New maps can be built and entered.
- Expansion sequences start from the original scenes and return to the same game state.

Run the tests as described in the main README. Story tests use the `[Story]` tag. If a document no
longer matches its target, the test reports its filename and line number.

You can preview a scene with the editor's `RenderScreen` commandlet. It runs headlessly from a chosen
map and story value, saving images as it goes:

```sh
UnrealEditor-Cmd Weltall.uproject -run=RenderScreen -AllowCommandletRendering -RenderOffscreen -unattended -Screen=Field -Map=616 -FieldEntrance=1 -FieldStory=252 -StoryMode=Expanded '-Reader=0.6,0.055' -Seconds=70 -Every=1 -Size=1280x720 -Out=Saved/Renders/shot.png
```

`-Reader` advances dialogue at a reading pace. `-Route=x,z;x,z;!x,z` walks the party through the listed
points and presses confirm at points marked `!`, such as a door. `-SoundOut=sound.wav` records audio,
and `-Every=0.016` captures every frame for a video. `-StoryMode=Expanded` plays the expansion the
Modding page offers from the checkout's `Mods` folder, so extract the examples, or put yours there,
first. Run the commandlet without options to see the rest.

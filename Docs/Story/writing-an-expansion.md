# Writing an expansion

This guide takes you from the example scenes to a Disc 2 expansion of your own, ready to share. The other story
guides explain each kind of document in detail; this one puts them in order.

## Start from a copy of the examples

1. Options → Modding → Disc 2 → **Extract example scenes** copies the examples to `Mods/Disc 2 examples`.
2. Copy that folder inside `Mods` under your expansion's name, such as `Mods/Elly's errand`. Choose the name now: a
   save made in an expansion keeps its folder's name, and a save whose folder is missing loads in the original Disc 2.
   Keep the folder name short: saves have room for 64 bytes, and accented letters use more than one byte.
   Weltall refuses an expansion whose folder name is too long.
3. Change the copy's `mod.txt`:

   ```
   name Elly's errand
   author Your name
   version 0.1
   ```

   The Expansion row and the Disc 2 question show this name, so make it different from the examples'. The row has
   room for 16 characters.
4. Restart Weltall. Expansions are read as the game starts, so restart after every change to your documents. Then
   choose yours on Options → Modding → **Expansion**.

Keep a save from the end of Disc 1, before Disc 2 begins. Loading it asks whether to play Original or Extended,
so you can choose your expansion and try it from the beginning as often as you like. Once you answer, later saves
keep that choice: an Original game stays Original, and an Extended game keeps the expansion it started with.

## How the examples fit together

The examples are one sequence, each document leading to the next:

| Document | Its part |
|---|---|
| `rematch-parting.txt` | Map 273, after Fei's launch: fades out into map 730 instead of starting the rematch. Here the expansion leaves the original story. |
| `rematch-lead-in.txt` | Map 730, built over map 613's sky: a new scene, then the original battle, which ends on map 731. |
| `rematch-narration.txt` | Battle event 19: a new first page for the battle's narration. |
| `rematch-aftermath.txt` | Map 731, also over map 613: the scene after the battle, then into Taura's house, map 616 by entrance 1. |
| `mass-driver-setting-out.txt` | Map 616: entered by entrance 1 at story value 252, a new scene changes the party and sets the story to 253. |
| `mass-driver-way-out.txt` | Map 614: the clearing opens onto the world map at 253. |
| `mass-driver-world-map.txt` | The world map: the Mass Driver at 253, leading to map 732. |
| `mass-driver-airlock.txt`, `mass-driver-corridor.txt`, `mass-driver-control-room.txt` | Maps 732, 733 and 734, built from maps 382, 384 and 369. The control room sets the story back to 252 and starts the original launch scene, where the expansion rejoins the original story. |
| `way-back.txt` | A save made at 253 loads in the original game at the narration on map 617. |
| `subtitles.txt` | Titles shown inside the original chapters. Most are for points past the examples' end; change or delete them. |

Delete what you don't need. Because each document leads to the next, change the exit of the one before any you
remove. Nothing in the folder is reached without the branch in `rematch-parting.txt`, or one of your own.

## Find where to branch

Choose the part of Disc 2 you want to make playable, then read the scripts around it:

- **Export Disc 2** on the Modding page writes every map of the second disc to `Mods/Disc 2 (export)/disc2`: its
  messages, then its scripts, each instruction's offset in a comment at the end of its line (`; 0614`, which a patch
  writes as `@0x0614`). Read the maps there, but don't share them: they hold the game's own scripts and words. Until
  you edit it, the export changes nothing.
- `WeltallMapText`, built with the core tests, prints any map of either disc in the same form
  ([Inspecting existing maps](README.md#inspecting-existing-maps)).

The story's progress is event variable 0, `var[000]`, and a scene's script usually compares it
(`If var[000] == #252`) to decide whether to run. Find the value at the point you branch from, then choose the value
your stretch plays at: one that no map of the original sets (search the export for `SetVar var[000]`), between the
value at your branch and the next one the original sets. Every other map then treats your stretch as that point of the
story, and your own patches can still tell the two apart. The examples' 253 is such a value: the original sets 252
for the Mass Driver's narration, and never 253.

Branch with a patch of the map where the original scene plays: a `detour` at the instruction where your story parts
from the original's, sending the player into your first map ([Changing a map](changing-a-map.md)).
`rematch-parting.txt` does this with one detour: a fade, then `ChangeMap #730 #0`.

## Write the scenes

- Patch the original's maps to add scenes, open exits or change who stands where: [Changing a map](changing-a-map.md).
  Make each change depend on your story value, so the map plays as before at every other point.
- Make maps of your own, numbered 730 to 1023, from any map's geometry: [A map of your own](a-map-of-your-own.md).
  Each expansion plays alone, so two expansions may use the same numbers; inside yours, use each number once.
- Add places to the world map: [The world map](the-world-map.md).
- Go on with a battle's narration from where your scene ends: [A battle's narration](a-battles-narration.md).
- Write your own words as messages. Where a character says one of the game's own lines, name it
  (`message Name from map 617 message 16`) rather than copying it. Your expansion then holds only your own writing,
  and a translated disc shows the line translated.

## Rejoin the original story

At the end of your stretch, leave the game as the original would have it at that point: the story value, the flags
the original's scenes there would have set, and the party. Then make the same map change or start the same scene the
original does. Read the original's scripts where you rejoin and do what they do, in the same order. The examples'
control room sets the flag the narration on map 617 sets, puts the story back to 252 and starts the same launch scene
([A stretch of story](a-stretch-of-story.md)).

## Saves and titles

A save made in your stretch is also a save of the original game, which knows nothing of your story value or your
maps. Give every value your stretch sets a row in `way-back.txt`, naming the original's point that matches what the
player has seen there, usually the narration that tells your stretch in the original:

```
way back
story 253 to map 617 entrance 2 story 252
```

Loaded without your expansion, in Weltall or the original game on a PlayStation, the save goes on from that point.
Loaded with it, the save goes on where it was made. A save made at a value without a row is written as it stands, at
your own map and value, which the original game can't load.

`subtitles.txt` gives your stretches titles within the original chapters ([Subtitles](subtitles.md)).

## Try it

Restart, choose your expansion on the Expansion row, load the save from the end of Disc 1 and answer Extended, whose
help names your expansion. Problems found as the game starts show on the Modding page:

- **Can't play**: a document can't be read. The log names the document, the line and the problem.
- **Other disc**: a document doesn't match your game data, such as a patch whose `expect` check fails.
  Weltall skips that document, and the log names it.
- **Unchanged**: none of the documents changes the disc.

The log is `Weltall.log` in `%LOCALAPPDATA%\Weltall\Saved\Logs` on Windows and `~/.config/Epic/Weltall/Saved/Logs` on
Linux. A scene that places a character off the walkable ground stops the game with an error naming the actor.

If you build Weltall from source, the editor's `RenderScreen` commandlet previews a scene headlessly from any map and
story value ([Checking your work](README.md#checking-your-work)).

## Share it

Your expansion's folder is what you share: players put it in their `Mods` folder and choose it on the Expansion row.
Share only your own work. If you started from Export Disc 2 and edited its maps, use **Make shareable** first
([the modding guide](../Modding.md#making-an-expansion-shareable)): its copy keeps only your changes, the game's own
lines named rather than copied. The copy starts with the same `name` line as your expansion, so change one of them to
tell the two apart on the Expansion row.

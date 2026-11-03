# Subtitles

Xenogears shows chapter titles in its save slots. Weltall also displays them on screen when a chapter
begins. The Disc 2 expansion keeps those chapters and adds titles for sequences within them, called
subtitles.

A subtitle appears once, when the player first has control in that sequence. Letters distinguish it
within the chapter, for example "Chapter 57a: Secret Battleship" and "Chapter 57b: Soylent System".

`Content/Examples/disc2/subtitles.txt` defines the examples:

```
subtitles
story 254 Secret Battleship
story 258 Soylent System
```

- `story S Words` sets the subtitle from story value S until the next subtitle or chapter. List story
  values in increasing order.
- Titles use plain characters from the message font, with a maximum of 24. This leaves room for the
  prefix, such as "Chapter 57a: ", on one line.
- The chapter containing S determines the chapter number. Its first subtitle gets `a`, the next `b`, etc.
- If a chapter starts with a subtitle already active, the subtitle card follows the chapter card.
  Loading a save during a sequence doesn't display its subtitle card.
- Subtitles are only used in the expanded Disc 2, with one document in the pack. Save slots still
  display the original chapter title.

To capture a subtitle card with the editor's `RenderScreen` commandlet, use `-Card=57,254`: the
chapter number followed by the subtitle's starting story value. `-CardAt=S` sets when the card appears.

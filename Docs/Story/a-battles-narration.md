# A battle's narration

Many Disc 2 battles start with narration over the battle scene. If your playable sequence already
covers part of that narration, you can skip those pages and optionally add a transition. Use a
`battle event N` document, where N is the event number referenced by the battle formation.

`Content/Story/disc2/rematch-narration.txt` is an example. The lead-in scene above the forest has
already shown the meeting with Ramsus, using the first narration page in Fei's cockpit. The battle
therefore starts at page 2, after a new transition page:

```
battle event 19
message 0 from page 2 after
{
I could not shake him.{wait 1E}
   Driven from the sky,
      we came down in
         the forest...
}
```

- `message K from page P` starts message K at page P.
- Adding `after` and text in braces displays that text first, followed by the original message from page P.

New pages use the original message window's dimensions. In this example, that means 24 plain
characters per line and four lines per page. If a page doesn't fit, the document is rejected and the
tests fail.

Reference the original narration by message and page number. Only new text belongs in the document.

## Reusing a narration page in a map

A map document can reference a battle message or one of its pages without copying the text:

```
message Met from event 19 message 0 page 1
```

`Content/Story/disc2/rematch-lead-in.txt` uses this to show the meeting in the sky scene. The original
text is read from your disc when the map loads.

A field map's message can be named the same way:

```
message Where from map 617 message 23
```

The Nisan assault uses this for everything Ramsus and Elly say before the cathedral: the chair's
own lines (map 617), read from the disc as the map loads, so the scene speaks whichever script the
game plays with, the original's or Perfect Works Build's, and carries none of the text itself.

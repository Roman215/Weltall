# Disc 2 REDO (design, external pack)

A pack that turns every narrated stretch of Disc 2 into a played sequence, built on the
toolkit described in this folder. The design lives at
https://github.com/MRinkl/Weltall-Disc2-REDO (docs/DESIGN-BIBLE.md); the pack will arrive
here as `Content/Story/disc2-redo/`, sequence by sequence, each gated by the `[Story]`
tests.

Planned sequences, in build order: Ramsus' assault on Nisan played as Elly; the Soylent
System facility with the Sufal Mass; Nortune and the raising of the Yggdrasil IV; Mahanon
as a full dungeon; Golgoda as Elly's solo fight; the Shevat aftermath; the Excalibur run;
a short post-credits epilogue. The existing `disc2` examples (rematch, Mass Driver) are
used unchanged.

Two questions for upstream that shape the first files:

1. Story values: the pack needs roughly a dozen unused `var[000]` values inside chapters
   57–68. Is there a preferred way to reserve them so `disc2` and `disc2-redo` never
   collide?
2. Pack loading: packaged builds read `Content/Story` at startup. Is a second pack folder
   beside `disc2` meant to be selectable from the Disc 2 prompt, or should a pack declare
   itself as an alternative to `disc2`?

# Modding

Open Options from the title screen or the in-game menu, press L1 or R1 to reach the Modding page, then choose
Assets. One export runs at a time. The page shows its progress, and you can cancel it. Everything you export stays
on your computer. Neither the game nor this repository includes any extracted game content.

## On Android

Android keeps Weltall's folders out of reach, so the Modding page adds mods itself, under **This device**. **Add
zip file** adds a texture pack, such as Perfect ART Works, or Perfect Works Build from a zip file. **Add folder**
adds one from a folder you pick. A `.7z` or `.rar` file has to be unpacked first, on a computer or with a file
manager that can, before you copy the folder to the device. A zip of a computer's `Mods` and `Assets` folders adds
everything in them, each file in its place. The mods and packs on the device are listed below with their sizes, and
you can remove any of them there. Adding a texture pack turns on **External assets**, which loads texture packs and
other replacements, and Perfect Works is turned on and used from the next time you start Weltall. Making exports and
editing files is a computer's work, so the Android version has no exports.

Perfect Works Build is read while the game runs, so replacing or removing one the game already has takes effect
the next time you start Weltall.

## Voices

Put both discs of a supported English undub in `Games` to play with English text and Japanese voices. If
you also have the USA discs there, choose English, Japanese or Auto under **Options → Sound → Voices**. Auto
uses the voices of the version you're playing, which is the USA release when both are present. If the voices
you chose aren't available, you'll hear the ones from the version you're playing. A change applies from the
next battle or movie.

Movie subtitles come from the undub's discs or from Perfect Works Build (see below). **Options → Sound → Subtitles**
has three choices: Auto shows them whenever a movie plays the Japanese voices, On also shows them with the English
voices, and Off hides them. The Japanese voices in movies need the undub disc for the disc you're playing in `Games`.

When you have both, **Subtitles from** picks whose translation you read. Auto uses Perfect Works Build's while the
mod is on, and the undub's otherwise. If only one of them has subtitles for the disc you're playing, the game uses
that one. With neither, the Subtitles row reads None found, and its help says what to add.

The game recognizes the undub by the SHA-1 hashes of its boot files, not by file names:

| Disc | Boot file | SHA-1 |
|---|---|---|
| 1 | `SLUS_006.64` | `1e21636bc067bd3873b78be3d72978c8038daca0` |
| 2 | `SLUS_006.69` | `2d9a51f9d6fc6855118fc217002edaef30dadec6` |

Japanese retail discs use a different text encoding, different scripts and different movie numbering. In this
version you can dump their files, but they can't supply Japanese text or voices.

## Fan translations

A fan translation that only changes the game's text, pictures and movies plays like the USA release it was made
from, such as the Spanish translation by Romhack Hispano. The game checks that the patched boot file runs the same
program as the USA one. It doesn't need to know the translation. Put the patched images in `Games`. If the original
USA images are there too, the patched ones play. Weltall's own screens, such as Options, stay in English.

Translations that also change the game's program play only when Weltall knows them. The French translation by
Atelier Traduction (version 1.1) is one: make the images with its patcher, and with its undub step for the
Japanese voices. Weltall plays its font with letters of different widths, its menus and battle screens as it
lays them out, its opening logo, the subtitles in its movies and the credits screen it adds at the very end
of the game. Its version with Japanese command names hasn't been tried.

## Dumping disc files

**Dump disc assets** copies every file in each recognized disc's index into an `Assets` folder next to
`Games`. That includes textures, sprites, models, animations, sound banks, music sequences, scripts and movies,
all in their original formats. Running it again skips files that are already there, so it never overwrites
your edits. Allow several GB if you have more than one version of the game. To edit textures as pictures, use
the texture export below instead.

Each disc gets its own folder: `usa.disc1`, `usa.disc2`, `usa.undub.disc1`, `usa.undub.disc2`, `jpn.disc1` or
`jpn.disc2`. Each folder has a `manifest.tsv` listing every file's number, disc sector, original size and path.
Keep it with that disc's files.

| Path inside a disc folder | Contents |
|---|---|
| `files/DD/NNNN.bin` | File NNNN in directory DD, padded to a whole sector |
| `records/NNNNNN.bin` | A file from the disc's index that isn't in a directory |
| `movies/NN.str` | Movie NN, with its video, audio and sector headers |

BIN/CUE images, and CHD images made from them, keep the movies' audio. An ISO doesn't contain it, so a dump
from an ISO can't recover it. The dump doesn't convert movies to MP4 or models to Unreal assets.

## Loading replacements

Turn on **External assets** to load your files from `Assets`. Anything you haven't replaced still comes from
the disc, and the discs are still needed either way. Put replacements in the folder for the version you're
playing. Voices come from the folder for the voice version you chose.

A changed file or song takes effect the next time the game loads it. Some data stays loaded the whole time, so
restart the game if a change doesn't show up. If the game can't read a replaced file, it shows its usual error
screen. Turn External assets off to go back to the disc's files.

A replacement for a dumped file can't be empty, its size must be a multiple of 2,048 bytes (a whole number of
sectors), and it can be up to 512 MB. The game reads some formats using the sizes in the disc's index, so a
bigger file doesn't always work. A replacement movie must keep the original's number of sectors and their
layout. These files are mainly useful with tools that already understand the game's formats.

## Music

**Export music WAVs** saves the game's 73 songs from Disc 1 as 44.1 kHz, 16-bit stereo WAV files in
`Assets/music/original`, ready to edit. It runs in the background, shows how many songs are done and can be
cancelled. Running it again keeps the files already there. It's separate from the disc dump because
uncompressed audio can take several GB.

Each WAV is named by its song number and a 16-digit key calculated from the song's data, such as
`song-07-3f2a9c41d0e8b6a1.wav`. A song has the same key everywhere it appears: on either disc, on the undub
and on the world map. To replace a song, copy its WAV from `original` to anywhere else under `Assets/music`,
then edit or replace the copy, keeping the key at the end of the name. Only the name matters. The key
identifies the original song, not your file, so the WAV can be any recording of any length. Turn on External
assets to use it. After editing, turn External assets off and on again to reload.

Replacements can be mono or stereo 16-bit PCM at 8,000–192,000 Hz. To loop part of the file, add a `smpl`
chunk with one forward loop. Without one, the whole file loops for as long as the original song plays. Each
export contains the song's first full loop, or its ending plus a short tail. If a song neither loops nor ends
within ten minutes, its export stops with an error.

The original song still decides when the music changes, its timing and its fades. A recording can't mute a
single instrument, so scenes that do that switch to the game's own synthesized music for the moment. Music
replacements don't affect sound effects or movie audio. When a scene changes a song's tempo or pitch, the
recording is resampled, which changes both together.

## Textures

**Export textures** saves every texture it finds on Disc 1 as a PNG in `Assets/textures/original`. It takes
under a minute, runs in the background and can be cancelled. Running it again keeps the files already there.
It covers:

| Folder inside `original` | Contents |
|---|---|
| `field/NNNN` | Field map NNNN's scenery and the background picture behind it, plus each character's sprite sheet in each palette |
| `sprites/04-NNNN` | Sprites the field loads as it needs them (the party, their Gears and extra animations), with all of a sprite's cells on one sheet per palette |
| `files/DD/NNNN` | Pictures stored whole in file NNNN of directory DD, in their own colors: the world map (`36`), the field's system images and full-screen pictures (`04`), the menus (`16`), the battle results screen (`18`), and some battle (`12`) and Battling arena (`48`) pictures |
| `battle/stages/NNN` | Battle stage NNN's textures, including its background picture and the clouds of its sky dome |
| `battle/enemies/NNN` | The enemies of battle scene NNN: textures for 3D enemies, and a sheet of all the cells for each sprite enemy |
| `battle/party/NN` | Character NN's battle sprite and attack animations, with all the cells on one sheet per palette |
| `battle/gears/GG-V` | Gear GG's battle textures, with variant V's parts or colors (0 for none) |
| `battle/special/N` | The textures of special battle model N |
| `battle/hud` | The battle screen's pictures: the command ring, the party's panels with their HP digits and gauges, the damage numbers, the windows, and every character's portrait |
| `battle/result` | The same with the results screen's own pictures |
| `battle/effects/NNN` | Battle effect NNN's sprites, with all the cells on one sheet per palette. `common` holds the effects every battle uses |
| `gears/NN` | The textures of Gear model NN, which field maps and the Gear shop show in 3D |
| `captured` | Textures saved by **Capture textures** while you play |

Each texture is named by a 16-digit key calculated from its pixels and colors, so it has the same name
wherever it appears. A map's folder holds every texture that map draws. A texture used by several maps is in
each of their folders, as a link rather than a copy on drives that support it. `original/index.tsv` lists every
folder each key appears in.

Pictures bigger than a 256 x 256 texture page, such as full-screen pictures and dungeon maps, are saved whole as
one PNG, even though the game draws them in pieces. When Gears are shown in 3D on the field and world map
(Options → Display → Gear sprites), they use their battle models, so their textures are the ones in
`battle/gears`.

For anything the export misses, such as the strips some battle effects cut out of a sprite, turn on **Capture
textures** and play through that part. A texture is saved once its pixels stay the same for a frame, and only if
there's no PNG with its key yet. If the game draws something that isn't a known picture, the whole 256 x 256
texture page is saved. Turn capture off when you're done.

To replace a texture, copy its PNG from `original` to anywhere else under `Assets/textures` (any subfolder
works) and edit the copy. You can add a description before the key, like
`village-door-0d381f34099245c6.png`, as long as the name ends with the key. Only the name matters. The key
identifies the original texture, not your file, so the PNG can be any picture. Don't edit the files in
`original`. They're your reference, and because shared textures are linked, an edit there changes every folder at
once. Turn on External assets to use your replacements. After editing, turn External assets off and on again to
reload them.

Each cell of a sprite is a separate texture. The export collects them on a sheet such as `cells-0.png`, with
`cells-0.tsv` next to it listing each cell's key and position. Copy both files and edit the PNG. The `.tsv` file
has to stay next to it with the same name. If a cell's key also has a PNG of its own, the game uses that PNG
instead of the sheet's cell.

A replacement can be up to 16 times the original's width and height, and it's stretched to cover exactly the
original's area. For example, a 64 x 48 texture can be replaced at 96 x 72, 128 x 96 or 1024 x 768. Keep the
same proportions, or the picture gets squeezed to fit. Whole multiples (2x, 3x, 4x and so on) line up exactly
with the original's pixels. A sheet scales the same way, as a whole. Pixels with an alpha of 128 or more are
drawn, and the rest are transparent. A replacement can draw where the original was transparent, for a smoother
outline, and leave holes where it was solid. If the original blends with what's behind it, the replacement
blends the same way.

Replacements load in the background the first time the game draws them. Small ones appear right away. A large
one may show the original for a moment first, usually while a map fades in. The replacements in one scene can
use up to 512 MB of video memory. Beyond that, the remaining textures keep their original look, and the log says
so. As a guide, replacing every texture of a large map at 4x takes under 100 MB, and at 8x up to about 350 MB.

When the game changes a texture's colors, as in fades, flashes and palette animations, the replacement changes
the same way. A texture drawn with a different palette, such as a character's color variant or the world map's
fog, has its own key. If it has no replacement of its own, it uses the replacement for the original colors and
applies the difference. When a 3D model (a battle stage, an enemy or a Gear) animates its own texture, as with the
fire behind Lahan in the first Gear battle, the replacement stays on but doesn't animate. Textures the game shifts
every frame, such as scrolling water, and pictures copied from the screen keep their original look.

### Texture packs made for Beetle PSX HW

Weltall can use texture packs made for Beetle PSX HW, a PlayStation core for RetroArch, without any changes to them.
Perfect ART Works and the Xenogears HD Portraits Project are two such packs. Put the pack's folder, the one whose name
ends in `-texture-replacements`, anywhere under `Assets/textures` and turn on External assets. You don't need to
rename it.

These packs name each picture after the data the game loads into the PlayStation's video memory and the colors it's
drawn with, not after the texture on screen. Weltall keeps track of that data the way Beetle PSX HW does, so each of
the pack's pictures shows up wherever the game draws from it. Your own replacements come first: where a texture has
a PNG named by its key, the game uses that instead of the pack's picture.

Like Beetle PSX HW, Weltall only uses the pictures directly inside the pack's folder. Packs often keep alternative
versions in folders inside it, such as the `alternate portraits` folder of Perfect ART Works. To use one, copy it
over the file with the same name in the pack's folder, as the pack's instructions say. If two packs have a picture
with the same name, the game uses the first one by folder name. Perfect ART Works and the HD Portraits Project both
replace the portraits, so keep only one of them in `Assets/textures`. If you use Perfect Works Build from `Mods`,
leave its Portraits and Roni's portrait rows on Original, because the packs' portraits are named after the original
ones. Perfect ART Works also covers Perfect Works Build's version of the battle screen.

A pack picture's transparency works as it does in Beetle PSX HW. Fully transparent pixels aren't drawn, fully
opaque ones are solid, and anything in between blends with what's behind it where the game draws semi-transparent
textures. Pictures can be PNG, JPEG, BMP or TGA files. DDS and WebP files aren't read, so convert them to PNG.
Packs that replace whole texture pages, in a folder whose name ends in `-texture-replacements-pages`, aren't
supported.

## Text

**Export text** saves every message in the game as text files in `Assets/text/original`, and the font in
`Assets/font/original` (see the next section). It takes a few seconds. Running it again keeps the files already there.

| File inside `original` | Contents |
|---|---|
| `field/NNN.txt` | Field map NNN's dialogue, signs and prompts |
| `names.txt` | The names of items, weapons, armor, abilities, Deathblows and Gear parts, and the battle screen's labels |
| `menu.txt` | The menu's prompts and labels: saving and loading, the shops, the party |
| `menu-help.txt` | The menu's descriptions of items, abilities and equipment, and the Gear parts' names |
| `battle.txt` | What a battle says as actions happen: statuses, counterattacks, fuel |
| `battle-help.txt` | The help lines in battle for items, Ethers and Gear Specials |
| `enemies.txt` | Each battle's enemy names, followed by what the enemies say |
| `battle-events.txt` | The dialogue of scripted battles |
| `worldmap.txt` | The world map's place names and messages |
| `battling.txt` | The Battling arena's menus, tutorial and taunts |
| `chapters.txt` | The chapter titles, shown when a chapter begins and in the save slots |

Each message is a block like this:

```
message field/19/0  ; 30 x 3
{
Fei
"Mmmm!{wait 1E}
That was good!"
}
```

The first line gives the message's id: the table it's in and its number there. Everything between the braces is the
message's text, line breaks included. Marks in braces control the window and fill in things during play. The numbers in
them are hexadecimal.

| Mark | What it does |
|---|---|
| `{page}` | Waits for the button, then clears the window and goes on |
| `{more}` | Waits for the button, then goes on in the same window |
| `{wait 1E}` | Pauses for that many 60ths of a second (1E is 30, half a second) |
| `{close 3C}` | Closes the window by itself after that pause, without the button |
| `{auto 3C}` | The same, unless the player presses the button first (timed choices) |
| `{speed FF}` | Shows that many letters at a time; FF shows the whole page at once |
| `{delay 01}` | Types slowly, one letter every few frames, until the message ends |
| `{name 00}` | A character's or Gear's name (`{name 80}` and up: the party's members in order) |
| `{item}`, `{weapon 03}` | The name of the item the scene is about, or of a weapon |
| `{digits 00}`, `{signed 00}`, `{number 00}` | A number the scene fills in |
| `{Circle}`, `{Cross}`, `{Triangle}`, `{Square}` | A button, drawn as the player has it bound |
| `{box}` | The box the game draws for a missing character |
| `{8F}`, `{FE12}` | Any other character, by its code in the font |

To change a message, copy its block into a `.txt` file of your own anywhere under `Assets/text` (not in `original`)
and edit the text. One file can hold messages from any table, since each block's id says which message it replaces.
Turn on External assets to use your messages. A message applies the next time the game reads its table: a field
map's when you enter the map, the menu's when you open the menu, an enemy's when a battle starts and the world map's
when you go out onto it. The names, the battles' messages, help and dialogue, the arena's text and the chapter titles
apply when the game starts. After editing, turn External assets off and on again, then enter the map again or restart
the game. If you use Perfect Works Build's script, the ids refer to the mod's messages, which may not match the
original's. The export always saves the original game's text.

Write text with the letters, digits and marks of the game's font, and the Japanese symbols and kana it also has
(`…`, `～`, `※`, `→` and so on), as UTF-8. To use a letter the font doesn't have, such as an accented one, add it to
the font first (see the next section). The game's log (`Weltall.log`) names any message that has a character
the font lacks or a mark it doesn't know, and the game keeps the original text for that message.

The numbers after a field message's id (`30 x 3`) are the size of its window: 30 wide and 3 lines tall. In these
units a letter is 2 wide, and a symbol or button is 3. When you change a field message, the game resizes its window to
fit your text, keeping any extra room the original left for a name. The largest window the screen holds is 70 wide
and 13 lines tall. A message bigger than that keeps the original text, and the log says so. A face beside the text
takes up 17 of the width, so keep those lines to 53 at most. Some scenes set a window's size themselves, and those
windows keep their size. If your message doesn't fit its window or runs off the screen, the log names it, so you can
shorten it or break its lines differently.

In the menus, in battle and on the world map, the text goes where the game puts it. The log mentions any message
that's wider or taller than every original message in its table, since it may not fit.

The log also names messages whose marks differ from the original's: a missing `{name}` or `{item}`, a different
number of pages, or a message that no longer closes by itself. They're used anyway, but check them. A choice's
options have to stay on the same lines, and a scene may wait for a window to close by itself.

A chapter title is one line of up to 15 letters, digits, spaces and the marks `+ , - . / ! " # % & ' ( ) : ? [ ] =`,
because a save keeps its title that way.

## The font

The text export also saves the game's font as `font.png`, with every character on a grid of 12 x 11 pixel cells, 16 to
a row, white on transparent, and `font.tsv`, with a line for each cell: the character's code, the cell's number on the
grid, its width (8 or 12 pixels), the character it stands for and how many of the game's messages use it.

To change the font, copy both files from `original` to `Assets/font` and edit them there. Pixels with an alpha of 128
or more are drawn. Draw only the letter, because the game adds the dark outline around it. A letter 8 pixels wide has
to stay within the first 6 columns of its cell, or the next letter cuts off its outline (the log says if one doesn't).
The picture can also be the same grid 2 to 16 times as large, and the game reduces each cell to its 12 x 11 pixels.

To add a character, draw it in a cell the English text doesn't use and type the character in that cell's line of
`font.tsv`. Your messages can then use it. The Japanese kana (the cells from 131 on) only appear in the game's debug
rooms and a few placeholder names, so they're the ones to redraw. A kana cell is 12 pixels wide, wider than the
English letters. To make an added letter as narrow as the others, change its width to 8. Don't use the cells of the
four button symbols (○, □, △ and ×) for new characters: wherever the text uses them, the game shows the player's
buttons instead. Font changes apply when the game starts.

The Battling arena's large capital letters are a picture, not this font. To redraw them, replace the 256 x 39 texture
in `files/48/0005` of the texture export.

## Scripts and models

You can change field scripts, messages and story behavior with plain text files in `Content/Story`.
[The story guides](Story/README.md) explain the instructions and formats. The settings file lists the game's
options as `key = value` lines, but it can't add new gameplay rules.

Models are only exported in the game's own formats, with their skeletons, animations and textures in the dumped
files. You can't replace a Gear with an Unreal skeletal mesh or import glTF or FBX models.

## Perfect Works Build

Weltall can use parts of [Perfect Works Build](https://github.com/PWBuild-Team/Perfect_Works_Build), a fan mod
with a new translation of the script, rebalanced gameplay and fixes of its own. Weltall doesn't include any of it, and
Weltall isn't affiliated with or endorsed by the Perfect Works Build team. Please report problems you have with the
mod in Weltall on Weltall's issues page, not to the mod's team. To use it:

1. Download `Xenogears_Perfect_Works_Edition_<version>.7z` from the mod's releases page.
2. Unpack it into a folder named `PerfectWorks` inside the `Mods` folder next to `Games`, so that
   `Mods/PerfectWorks/gamefiles` exists.
3. Start Weltall. The Modding page in Options now has a Perfect Works row. Turn it on, then press Confirm to choose
   which parts of the mod to use.

Your choices apply the next time you start the game. You can use:

- **Script**: the mod's new translation of the names and the story script, including the dialogue during battles.
- **Story mode**: easy battles and no random battles on the field.
- **Music**: the mod's new music for some scenes.
- **Party and items**: the mod's stats and abilities for the party, its items and its battle rules.
- **Enemies**: the mod's stats and AI for some enemies and bosses.
- **Battling**: the mod's rebalance of the Battling arena. Basic gives each Gear its own HP, speed and heat, with the
  mod's rules for hits, knockdowns and the computer's play. Expert adds the mod's tweaks for two players.
- **Portraits**: Fixed uses the mod's portrait fixes, and Resized also narrows them for 4:3. **Roni's portrait** uses
  the mod's portrait of Roni. The mod's portraits are pictures of its own, so a texture pack made for the game's
  portraits, such as the HD Portraits Project, only replaces them while these rows are on Original.
- **Emeralda**: the mod's fix for Emeralda's sprites when she sits down at the cafe.
- **Title screen**: the mod's title screen.

As in the mod's own installer, story mode can't be used together with the rebalances or the arena modes: turning one
on turns the other off. Turn the rebalances on or off when you start a new game, because a save keeps the stats and
items it was made with.

In the mod, some of these parts change the game's program code: the battle dialogue's window sizes, the rebalanced
party and items, and the arena modes. Weltall makes the same changes in its own code. They were checked against
version 0.11.4 of the mod. With a different version, the battle dialogue, the rebalances and the arena modes stay as
in the original game, and the log says so. The other parts use the mod's files as they are.

The experience, gold and encounter options, faster text, battle flashes, Japanese voices and controls are Weltall's
own settings, so they work with or without the mod.

The mod's movie subtitles are a choice on the Sound page, **Subtitles from**, which works even while the mod is off.
They show its own wording and timing over the movies, as the mod's FMV undub does. Japanese voices in the movies still
come from the undub's discs.

The fixes the mod makes in the program code are rows of Weltall's Fixes page, on by default with or without the mod:
Billy's ether guns, Deus's first fight, the shop pointer, EP in Member Change, names ending in s, Def UP on foot and
the Gear shop parts. The first four follow fixes first made in the mod.

## Sharing mods

Only share material you have permission to share. Exported textures, music, models, scripts and movies, and edited
versions of them, can still contain copyrighted game content. Weltall doesn't download mod packs or upload your
exports. This repository ignores the `Games`, `Assets` and `Mods` folders, and the release downloads don't include
their contents. If you build packages yourself, rebuilding keeps your existing `Assets` and `Mods` folders.

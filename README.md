# Weltall

Weltall is a native port of *Xenogears* (PlayStation, 1998), rebuilt in Unreal Engine 5. The goal is to
make the game feel like a modern PC release while keeping the original gameplay intact.

You'll need your own copy of the USA release or a supported English undub. Weltall doesn't include
any game data. Put your disc images in the `Games` folder and the game will find them automatically.
BIN/CUE and CHD preserve movie audio; ISO/IMG also work, but the movies will be silent. Convert PBP,
ECM or other formats to BIN/CUE first. You can add Disc 2 while the game is running when it asks for
it. You can also start with only Disc 2, as on the PlayStation, to load your Disc 2 saves.

The undub plays with English text and Japanese voices, and its subtitles appear in the movies. If you
have both versions, choose which voices you hear under **Options → Sound → Voices**. **Options → Sound →
Subtitles** can also show the subtitles with the English voices, or turn them off. If you have Perfect
Works Build too, **Subtitles from** picks the undub's subtitles or the mod's. The Sound page shows what it
found, and says what you need for anything that's missing. Japanese retail discs aren't playable, but
you can dump their files (see [the modding guide](Docs/Modding.md)).

Fan translations that only change the game's text, pictures and movies also play, such as the Spanish
translation by Romhack Hispano. So does the French translation by Atelier Traduction (version 1.1), with
English or Japanese voices. Patch your USA discs and put the patched images in `Games`. If the originals
are there too, the patched ones play. Weltall's own screens, such as Options, stay in English.

**Status:** Both discs are playable, including field exploration, character and Gear battles, the
world map, menus, the Battling arena and movies. Gameplay behavior has been checked against the
original game's code. My own playthrough is still in progress, with about 25 hours played without
a hang. Automated tests have run every map and story battle, along with long stretches of the story
from save to save. You should be able to play from start to finish, and I'm fixing reported issues as
they come in. If something goes wrong, the game displays an error and tells you which log to include
in a bug report.

## What Weltall adds

- **Higher resolutions and widescreen** up to 32:9, with scenery extending beyond the original 4:3 view.
- **Smooth motion at any refresh rate**, including 120 Hz and above, without changing the game's pace.
- **A free camera** on the field and world map, with rotation, tilt and zoom using a mouse or right stick.
- **Per-pixel depth sorting** to fix characters, Gears and scenery clipping or flickering over each other.
- **Graphics options** for sharper or smoother scenery and sprites, anti-aliasing, upscaled movies and a CRT filter.
- **Softer battle flashes**, or none at all, for the bright screen flashes from attacks and Ether.
- **Separate volumes** for the music, sound effects, battle voices and movies, plus a master volume.
- **3D Gears** on the field and world map, using their battle models, plus full-size party sprites in Gear areas.
- **Battle speeds of 2x and 4x**, and faster text up to instant. Timed dialogue still leaves you time to read it.
- **Balance options** to earn up to four times the experience and gold, and to have fewer random battles, more of
  them, or none at all.
- **Chapter titles** displayed when each chapter begins.
- **Custom party order**, so any character can lead the party (see below).
- **Always run**, **analog movement** that walks in any direction and slowly with a light push of the stick, and
  doors that open when you walk into them.
- **Keyboard, mouse and controller support**: Xbox, PlayStation and Switch Pro controllers and most other
  gamepads, with rebinding and controller hot-plugging. Button prompts match your current bindings.
- **An Android version** for phones and tablets, with a PlayStation controller drawn on the screen (see
  [Android](#android)).
- **Save options** to load from the menu anywhere, import PlayStation memory-card or emulator saves, and export
  memory-card images.
- **Fixes for bugs in the original game**.
- **Modding options** to export the game's textures as PNG, its music as WAV and its text and font to edit, dump
  the disc's files, and load your replacements from the `Assets` folder, including texture packs made for
  RetroArch's Beetle PSX HW core. You can also use parts of the fan mod Perfect Works Build from your own copy. See
  [the modding guide](Docs/Modding.md) for the details.

You can adjust these in Options from the title screen or in-game menu, including switching back to the
original behavior.

The source also includes tools for turning Disc 2's narrated sections into playable sequences, with
two examples. These are separate from the main port and aren't included in the downloadable builds.

Weltall is not affiliated with or endorsed by the owners of *Xenogears*. All game content and
trademarks belong to their respective owners.

## Playing

Download the build for your system and unzip it wherever you want. Put the USA or undub disc images in
the `Games` folder (subfolders work too), then run `Weltall.exe` on Windows or `Weltall.sh` on Linux.
Windows requires 64-bit Windows 10 or 11 and a DirectX 12 graphics card. Linux requires a 64-bit
system with a Vulkan graphics driver. If neither disc is there, the game shows where it looked and what
it found, then starts once you add one. If you start with only Disc 2, the game asks for Disc 1 when you
choose New Game or load a Disc 1 save, as the original does. For phones and tablets, see
[Android](#android) below.

Players have run the Linux build on a Steam Deck and on CachyOS with an AMD graphics card.

Options covers display, sound, camera, text speed, bug fixes, controls, and a Modding page with the
balance options and the Assets page. During battles, R1 increases the speed from 1x to 2x to 4x, and
L1 decreases it.

To keep the original PlayStation button symbols in prompts and menus, turn on **Original PSX icons**
under **Options → Controls**. Your bindings still apply, and the rebinding rows show your actual keys.
**Button layout**, just below it, moves the controller's buttons. With **Circle confirms**, ○ confirms and is the
3-point attack, and × cancels. **Japan** also swaps □ and △ as the Japanese release does: □ jumps and △ opens the
menu. Buttons you rebound yourself stay where you put them, and with Original PSX icons on, prompts show the
layout's symbols. In battle, the attack buttons are laid out as they sit on your controller.

To rearrange the party, open the menu and press Right from the command list. Select a character with
×, then select another to swap them. Press △ to restore the story's order. The first character leads
on the field and world map, and your order also applies in menus and ordinary battles.

Your order is saved for that group of characters. If someone joins or leaves during the story, the
party returns to the story's order; when the same group is together again, your order returns. In
Member Change, a replacement takes the outgoing character's position. Cutscenes, scripted battles,
and field scenes where party members are speaking or placed around the map use the story's order.

### Android

The Android version runs on 64-bit phones and tablets with Android 10 or later and Vulkan graphics.
It isn't on Google Play and isn't registered with Google: it's only available as an APK from this
page, which you install yourself. Download it, open it on your device and allow it to be installed.
On Samsung devices, turn off Auto Blocker first (Settings → Security and privacy).

Android is starting to block apps from developers who aren't registered with Google: in Brazil,
Indonesia, Singapore and Thailand since September 2026, and everywhere from 2027. If yours refuses
the APK, turn on **Allow apps from unverified developers** in Developer options first (the first time,
Android asks you to wait a day), or install it from a computer with `adb install`.

To give Weltall your discs, first put the disc images on the device in a folder inside Download, such
as `Download/Xenogears`: connect it to a computer by USB and copy them there. Then start Weltall, tap
**Add folder** and pick that folder: every disc image in it and in its subfolders is added. Android
doesn't let apps pick the Download folder itself, which is why they go in a folder of their own. You
can also tap **Add disc images** and pick the files: for a BIN/CUE disc, pick its `.cue` file together
with its `.bin` files. Weltall copies them into its own folder and starts once both discs are there. If you add only one, add
the other the same way, or tap **Continue anyway** to play with that one for now. You can delete the
copies in Download afterwards. To add more later, such as Disc 2 or the undub, use **Add disc images**
or **Add disc folder** under **Options → Saves**. Picking the same folder again only adds the disc
images that are new. If the game asks for a disc it doesn't have yet, such as Disc 2 at the end of
Disc 1, confirm and pick it right there.

While you play by touch, a PlayStation controller is drawn at the sides of the screen. A Bluetooth or
USB controller and a keyboard also work: the on-screen controller hides while you use one and comes
back when you touch the screen. Under **Options → Controls → Touch screen** you can swap its d-pad for
an analog stick, make it more see-through, and turn off its vibration.

Android keeps Weltall's own folder out of reach of computers and other apps, so **Options → Saves** has
a **This device** section. **Back up saves** writes all your saves into one zip file wherever you
choose, and **Restore saves** puts them back from one. A zip of a computer's `Saves` folder restores
the same way, so you can move a game from a computer to the phone. **Import saves** and **Export
card** pick and save their files with the same file picker. To update, install the new APK over the
old one. Your discs, saves, mods and settings stay where they are.

**Options → Modding** has a **This device** section as well. **Add zip file** and **Add folder** add
a texture pack, such as Perfect ART Works, or Perfect Works Build. Unpack a `.7z` or `.rar` file first,
then copy the folder to the device and pick it. Adding a texture pack turns on **External assets**, and
Perfect Works is turned on and used from the next time you start Weltall. Each mod and pack is listed
there with its size, and you can remove it. See [Docs/Modding.md](Docs/Modding.md#on-android).

### Default controls

| PlayStation | Keyboard | Gamepad |
|---|---|---|
| Directions | W A S D, arrow keys | D-pad, left stick |
| × | E, Enter | A |
| ○ | Q, Escape | B |
| □ | F | X |
| △ | Space | Y |
| L1, R1 | Tab, R | LB, RB |
| L2, R2 | 1, 4 | LT, RT |
| Start, Select | P, Backspace | Menu, View |
| Turn the view | Mouse | Right stick |
| Zoom | Mouse wheel | Right stick up and down |
| Reset the view | Middle mouse button | Right stick click |

On a PlayStation controller, A is ×, B is ○, X is □ and Y is △. On a Switch Pro Controller the buttons go by where
they sit, so B is ×, A is ○, Y is □ and X is △. You can rebind every control on the Controls page. Wherever the
game mentions a PlayStation button, it shows the key or controller button you've assigned to it.

### Saves and settings

On Windows, settings and saves are in `%LOCALAPPDATA%\Weltall`. On Linux, settings are in
`~/.config/Weltall` and saves are in `~/.local/share/Weltall/Saves`.

Use the Saves page in Options to import a PlayStation memory-card image or an individual save file,
including emulator saves. You can also export a card as an `.mcr` image to `Saves\Cards`.

### Reporting a problem

If the game stops with an error on screen, attach the log file it names to a bug report and describe
what you were doing. If it crashes, zip and attach the newest crash folder:

- Windows: `%LOCALAPPDATA%\Weltall\Saved\Crashes`
- Linux: `~/.config/Epic/Weltall/Saved/Crashes`

That folder includes the log and crash dump. A save from just before the problem is helpful too. On
Android, tap **Save the log** on the error screen, or use **Save the log** under **Options → Saves**
after a crash, and attach the zip file it saves. It holds the logs of your last few sessions.

## Development

The core (`Source/WeltallCore`) has no Unreal dependencies, so you can build and run its tests
separately. You'll need CMake 3.25+ and a C++20 compiler:

```sh
cd Tests
cmake --preset windows        # or: linux, linux-clang
cmake --build --preset windows
ctest --preset windows
```

Tests that need game data look for disc images in `Games/` and report a skip if a disc is missing.
Oracle tests compare the port's results with stored digests from the original code, run in an
emulated CPU with inputs from the same generator used by the tests.

Story additions are plain text files in `Content/Story`, loaded at startup. Each folder is a pack
and each `.txt` file is a document. They can patch field scripts and messages, create maps from
existing map assets, change battle narration, add world map locations and define subchapter titles.
Scripts use the original game's instruction names and message markup. Each document references
the original content it needs and contains only the additions.

The `disc2` pack includes two playable examples: the rematch above the forest, and Elly's trip to the
Mass Driver with a short dungeon. Both rejoin the original story afterward. The pack is only used if
the player selects it when Disc 2 begins. [Docs/Story](Docs/Story/README.md) explains how to write these
documents using the existing files as examples.

`WeltallMapText` is built alongside the tests and prints a disc map in this text format, for example
`WeltallMapText 613`. Use it to inspect the scripts you're patching. Its output contains original game
scripts and text, so don't commit it. The tests check each document against its map or battle.

### Packaging

`Scripts/package.py` builds standalone packages from a commit. It needs an Unreal Engine 5.8 source
build, with `UE_ROOT` set in the environment or in a `local.env` file at the repository root:

```sh
python Scripts/package.py                     # Development
python Scripts/package.py DebugGame Shipping  # any of DebugGame, Development, Shipping
python Scripts/package.py --platform Linux    # for Linux, with the engine's toolchain in LINUX_MULTIARCH_ROOT
python Scripts/package.py --platform Android  # for Android
```

Packages are written to `out/builds/<Configuration>/<Platform>`. Run `Weltall.exe` or `Weltall.sh`
from there. The script links your checkout's disc images into the package's `Games` folder. All
builds share the same settings and saves in your user folders.

An Android package needs the Android SDK in `ANDROID_HOME`, its NDK r27c in `NDKROOT` and a JDK 21
in `ANDROID_JAVA_HOME`. It holds two APKs: `Weltall-arm64.apk` for devices and `Weltall-x64.apk`,
which the script runs on the SDK's emulator as its check (the one running, or the virtual device
named in `ANDROID_AVD`), with the disc image in `ANDROID_CHECK_DISC` copied into the app's `Games`
folder. Both are signed with the key in the keystore `ANDROID_KEYSTORE` (its key named `weltall`,
the password in `ANDROID_KEYSTORE_PASSWORD`). Every later version has to be signed with the same
key, or Android won't install it as an update. A Shipping package needs the key; without one, the
other configurations use the SDK's debug key.

`python Scripts/package.py --archive Shipping` also creates a release zip in `out/releases`, without
disc images, and a separate debug symbols archive. For Android, the release file is the arm64 APK.

`python Scripts/release.py 0.8.5` builds the Windows, Linux and Android Shipping packages from a
pushed commit whose `ProjectVersion` matches that version. It creates the archives and a GitHub draft
release with the player downloads and release notes, using the GitHub CLI. Publish the draft from
the release page.

Packages leave out the `disc2` pack and play the original Disc 2 without asking you to choose a mode.
Build with `--expansion` to include the examples; those archives are marked in the filename.

## Credits

Four of Weltall's bug fixes follow fixes first made in the fan mod
[Perfect Works Build](https://github.com/PWBuild-Team/Perfect_Works_Build). When you use your own copy of the mod,
Weltall also recreates the mod's changes to the game's code: the battle dialogue windows, the rebalanced party and
items, and the Battling arena modes.

Weltall is not affiliated with or endorsed by the Perfect Works Build team. Like a texture pack, the mod is something
you add from your own copy, so please report problems you have with it in Weltall on Weltall's issues page, not to
the mod's team.

## License

Weltall's code is under the MIT License; see `LICENSE`. Third-party code keeps its own license, listed
in `ThirdPartyNotices.md`. This license applies only to Weltall, not to *Xenogears* or its game content
and trademarks.

## Support

If you'd like to help support Weltall or my future ports, you can do so through
[GitHub Sponsors](https://github.com/sponsors/Roman215) or [Ko-fi](https://ko-fi.com/roman215).

"""Builds a release of the game and drafts it on GitHub.

    python Scripts/release.py 0.8.0                      # the three Shipping packages, their files, a draft release
    python Scripts/release.py 0.8.0 --archives-only      # the files alone, to look at before drafting
    python Scripts/release.py 0.8.0 --platforms Windows  # one platform

The release is built from HEAD, which must be clean, already pushed to GitHub (the draft points at it) and carry the
version in Config/DefaultGame.ini's ProjectVersion. Each platform's Shipping package is built, run and archived by
package.py; its files are then named by the version in out/releases: Weltall-<version>-<Platform>.zip for players
(Android's is the APK itself, Weltall-<version>-Android.apk, signed with the project's key) and ...-symbols.zip, which
stays with whoever builds releases (it turns a player's crash report into source lines). The notes go beside them as
Weltall-<version>-notes.md.

Without --archives-only a draft release is made with the GitHub CLI (gh, logged in), the players' files attached and
the notes as its text, and the commit is tagged v<version> here. Nothing is published: the draft waits on the
release page to be read, edited and published by hand, which creates the tag on GitHub. GitHub attaches the tag's
source code to the release by itself.
"""

import argparse
import re
import subprocess
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
RELEASES = REPO / "out" / "releases"
PLATFORMS = ["Windows", "Linux", "Android"]
# What players download for each platform: a zip of the package, but Android's APK, which installs as it is.
PLAYERS_FILE = {"Android": ".apk"}
VERSION = re.compile(r"\d+\.\d+\.\d+")
PROJECT_VERSION = re.compile(r"^ProjectVersion=(.*)$", re.MULTILINE)

NOTES = """\
Weltall {version} is a native port of *Xenogears* (PlayStation, 1998), rebuilt in Unreal Engine 5.
You'll need your own copy of the USA release or a supported English undub. No game data is included.

## What's new

### After the ending

- Added a clear save. On the ending's last picture, wait a few seconds and press Confirm to save. Loading it puts
  you back before the final dungeon with your endgame progress.
- Added New Game+. With a clear save available, choose New Game on the title screen, then pick the clear save to
  start from. Choose whether to keep money, equipment, items, Deathblows and levels, and whether to remove the
  7-AP limiter. Story items stay behind.
- Clearing the game also unlocks additional character looks and party options for repeat playthroughs.
  Their details are shown in the game after you unlock them.

### Playing

- Added a message log: press L, or a controller's L3, to read the dialogue so far. The game waits while it is open.
- Added auto-advancing dialogue under Options → Display → Auto-advance text. It's off by default, and choices
  always wait for you.
- Added save states on the field and world map. These are Weltall's own format; use Options → Saves → Export card
  to move a game to a PlayStation emulator.
- Added integer scaling under Options → Display, to keep the CRT filter's scanlines evenly spaced.
- The battle HUD's moving parts now move smoothly on displays above 60 Hz, alongside the battle scene.
- Options → Controls → Stop at doors pauses movement briefly after a map change, so a held or fidgeted stick
  doesn't carry you straight into the next scene. It defaults to half a second; you can change it or turn it off.
- Options → Controls → Camera buttons lets the shoulder buttons zoom instead of turning the camera.
- On the Assets page, choose which parts of a texture pack to use: fonts, faces, menus and HUD, characters,
  scenery, or models and effects.

### Translations and modding

- Added support for SadNES cITy's Italian translation (version 1.2.1), including its subtitled movies and Japanese
  commands. Use patched USA images, or put the translation's `.xpt` and movie `.ppf` files in a folder under `Mods`.
  Options → Modding → Italian lets you choose its voices, commands and movies.
- Disc 2 expansions now load from the `Mods` folder, with a choice of expansion when Disc 2 begins. Two example
  scenes are included to extract and edit. Export Disc 2 writes the disc's maps and battle text as editable
  documents; Make shareable turns your edits into patches containing only your changes. See
  [Writing an expansion](https://github.com/Roman215/Weltall/blob/main/Docs/Story/writing-an-expansion.md) to get started.

### Fixes

- Fixed texture packs not loading on Linux when their folder's capitalization differs, or when a parent folder
  is reached through a link, such as on an SD card.
- Fixed 3D Gears appearing dark red in maps such as the Yggdrasil's hangar and the Battling arena's entrance.
- Fixed movies falling out of sync with their sound after a pause in rendering.
- Fixed battles stopping on missing enemy or attack names in the Brazilian Portuguese translation.
- Fixed the Gear menu leaving out defense from Perfect Works Build's rebalanced parts.
- Fixed 3D scenes being too large for their HUD in 4:3 and narrower windows, and map textures missing after a
  movie shown over the game screen.
- Fixed message portraits sometimes using a previous face, and party followers landing late after a jump.
- Fixed missing attack effects and gaps at the arm and leg joints of postgame Gear models.
- Android now saves changed settings when the app goes into the background.

## Playing

1. Download the Windows or Linux zip below and unzip it wherever you want.
2. Put both disc images in the `Games` folder. BIN/CUE and CHD preserve movie audio; ISO/IMG also work,
   but the movies will be silent.
3. Run `Weltall.exe` on Windows 10 or 11 (64-bit, DirectX 12), or `Weltall.sh` on Linux (64-bit, Vulkan).

On Android 10 or later (64-bit, Vulkan), install the APK instead. Copy the disc images from a computer
by USB into a folder inside the device's Download folder, such as `Download/Xenogears` (Android doesn't
let apps pick Download itself), then start Weltall, tap **Add folder** and pick that folder. Weltall
isn't on Google Play or registered with Google. When you open the APK, your phone may ask you to allow
your browser or file manager to install apps. The
[README](https://github.com/Roman215/Weltall#android) explains how.

On Windows, settings and saves are in `%LOCALAPPDATA%\\Weltall`. On Linux, settings are in
`~/.config/Weltall` and saves are in `~/.local/share/Weltall/Saves`. On Android, the Saves page in
Options backs them up to a file and restores them. You can import PlayStation memory-card saves from the
Saves page in Options. See the
[README](https://github.com/Roman215/Weltall#playing) for controls and more details.

## What to expect

Both discs are playable, and gameplay behavior has been checked against the original game's code.
I've finished my own playthrough, including the ending and a clear save. Automated tests have run
every map and story battle, along with long stretches of the story from save to save. You can play
from start to finish, and I'm fixing reported issues as they come in.

Players have run the Linux build on a Steam Deck and on CachyOS with an AMD graphics card. If you run
into problems on Linux, please report them.

I've played the Android version on my own phone, and it's also been checked on Android's emulator.
If you run into problems on Android, please report them with your phone or tablet's model.

If the game stops with an error on screen, attach the log file it names to a
[bug report](https://github.com/Roman215/Weltall/issues/new?template=bug_report.md) and describe what you
were doing. If it crashes, zip and attach the newest folder in `%LOCALAPPDATA%\\Weltall\\Saved\\Crashes`
(Windows) or `~/.config/Epic/Weltall/Saved/Crashes` (Linux). On Android, tap **Save the log** on the
error screen, or under **Options → Saves** after a crash, and attach the zip file it saves. A save from
just before the problem helps too.

## Support

If you'd like to help support Weltall or my future ports, you can do so through
[GitHub Sponsors](https://github.com/sponsors/Roman215) or [Ko-fi](https://ko-fi.com/roman215).
"""


def unwrapped(text):
    """Text with each paragraph and list item on one line: GitHub shows a release's notes with every line break kept."""
    lines = []
    for line in text.splitlines():
        joins = lines and lines[-1].strip() and line.strip() and not line.startswith("#") and not lines[-1].startswith("#")
        if joins and not re.match(r"(\d+\.|[-*]) ", line):
            lines[-1] += " " + line.strip()
        else:
            lines.append(line)
    return "\n".join(lines) + "\n"


def git(*arguments):
    return subprocess.run(["git", "-C", str(REPO), *arguments], capture_output=True, text=True, check=True).stdout.strip()


def refuse(message):
    sys.exit(f"release: {message}")


def check_source(version):
    """Refuses unless HEAD is clean, carries the version and has no release tag yet."""
    if git("status", "--porcelain"):
        refuse("the working tree has changes: commit them first")
    ini = (REPO / "Config" / "DefaultGame.ini").read_text(encoding="utf-8")
    found = PROJECT_VERSION.search(ini)
    if found is None or found.group(1).strip() != version:
        refuse(f"Config/DefaultGame.ini's ProjectVersion is {found.group(1).strip() if found else 'missing'}, not {version}")
    if subprocess.run(["git", "-C", str(REPO), "rev-parse", "-q", "--verify", f"refs/tags/v{version}"],
                      capture_output=True).returncode == 0:
        refuse(f"v{version} is tagged already")


def check_pushed(commit):
    """Refuses unless gh is logged in and GitHub has the commit the draft will point at."""
    try:
        found = subprocess.run(["gh", "api", f"repos/{{owner}}/{{repo}}/commits/{commit}", "--silent"], cwd=REPO,
                               capture_output=True, text=True)
    except FileNotFoundError:
        refuse("the GitHub CLI (gh) is not installed")
    if found.returncode != 0:
        refuse(f"GitHub does not have {commit[:7]} (push it first; is gh logged in?): {found.stderr.strip()}")


def build(platform, commit):
    """The platform's Shipping package of commit built, run and archived by package.py; the archives' paths."""
    print(f"release: building the Shipping package for {platform}")
    status = subprocess.run([sys.executable, str(REPO / "Scripts" / "package.py"), "Shipping", "--archive",
                             "--platform", platform, "--revision", commit]).returncode
    if status != 0:
        refuse(f"the {platform} package failed (exit status {status}): see package.py's output above")
    revision = (REPO / "out" / "builds" / "Shipping" / platform / "Revision.txt").read_text(encoding="utf-8").strip()
    if revision != git("rev-parse", "--short", commit):
        refuse(f"the {platform} package is of {revision}, not of {commit[:7]}")
    return RELEASES / f"Weltall-{revision}-{platform}{players_file(platform)}", RELEASES / f"Weltall-{revision}-{platform}-symbols.zip"


def players_file(platform):
    return PLAYERS_FILE.get(platform, ".zip")


def main():
    sys.stdout.reconfigure(line_buffering=True)
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("version", help="the release's version, as ProjectVersion has it (0.8.0)")
    parser.add_argument("--platforms", nargs="+", default=PLATFORMS, choices=PLATFORMS, help="the platforms to release (default: all three)")
    parser.add_argument("--archives-only", action="store_true", help="write the players' files and the notes, but tag and draft nothing")
    args = parser.parse_args()
    if not VERSION.fullmatch(args.version):
        parser.error(f"a version is three numbers, as 0.8.0: not {args.version}")

    check_source(args.version)
    commit = git("rev-parse", "HEAD")
    if not args.archives_only:
        check_pushed(commit)

    players = []
    for platform in args.platforms:
        archive, symbols = build(platform, commit)
        named = RELEASES / f"Weltall-{args.version}-{platform}{players_file(platform)}"
        archive.replace(named)
        if symbols.is_file():
            symbols.replace(RELEASES / f"Weltall-{args.version}-{platform}-symbols.zip")
        players.append(named)
        print(f"release: {named.relative_to(REPO)}")
    notes = RELEASES / f"Weltall-{args.version}-notes.md"
    notes.write_text(unwrapped(NOTES.format(version=args.version)), encoding="utf-8")
    print(f"release: {notes.relative_to(REPO)}")
    if args.archives_only:
        return 0

    drafted = subprocess.run(["gh", "release", "create", f"v{args.version}", *map(str, players), "--draft", "--target", commit,
                              "--title", f"Weltall {args.version}", "--notes-file", str(notes)], cwd=REPO)
    if drafted.returncode != 0:
        refuse(f"gh could not make the draft (exit status {drafted.returncode})")
    git("tag", f"v{args.version}", commit)
    print(f"release: drafted v{args.version}: read it on the release page, then publish it there")
    return 0


if __name__ == "__main__":
    sys.exit(main())

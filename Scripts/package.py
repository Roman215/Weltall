"""Builds standalone packages of the game for Windows, Linux or Android, one folder per configuration.

    python Scripts/package.py                         # Development, for Windows
    python Scripts/package.py DebugGame Shipping      # several configurations
    python Scripts/package.py --platform Linux        # for Linux
    python Scripts/package.py --platform Android      # for Android (Scripts/android.py)
    python Scripts/package.py --revision my-branch    # another commit than HEAD
    python Scripts/package.py --check-only Shipping   # start an existing package and read its log
    python Scripts/package.py --archive Shipping      # also write the package as a zip archive for a release
    python Scripts/package.py --expansion             # with the expanded second disc as far as it is written

Each package is built from a committed revision, exported into out/package/Weltall: a project tree of
its own, so its build, cook and staging never lock the files of the editor or of another checkout. The
result lands in out/builds/<Configuration>/<Platform> (Windows with Weltall.exe at its top, Linux with
Weltall.sh), and the discs of the checkout's Games folder are hard-linked into a Games folder beside it.
The full log of each build is out/builds/<Configuration>.log (Windows) or <Configuration>-Linux.log.

The port's own story documents (Content/Story) are exported with the sources and staged as loose text files
beside the cooked content; the check counts them in the export and finds that count in the package's log.
The expanded second disc (Content/Story/disc2) is not written to its end yet, so a package leaves it out and
plays the original's second disc; --expansion keeps it, for trying what there is, and marks the archive's name.

A package holds no debug files: the executable names its symbols where the build left them, in
out/package/Weltall/Binaries. With --archive, each package that built and ran is also written to
out/releases as Weltall-<revision>-<Platform>.zip (other configurations than Shipping add their name), without the
discs (its Games folder empty), and its symbols beside it as ...-symbols.zip, which stays with whoever builds
releases: it turns a player's crash report into source lines. The zips are Deflate at its highest level, which any
unzip reads; a Linux package's launcher and executable keep their execute bits in them.

UE_ROOT names an Unreal Engine source build, from the environment or local.env; a Linux package also needs
LINUX_MULTIARCH_ROOT (the engine's cross-compile toolchain), and its check runs it in WSL_DISTRO. An Android package
needs ANDROID_HOME (the SDK), NDKROOT and ANDROID_JAVA_HOME (the JDK its Gradle runs on), lands in
out/builds/<Configuration>/Android as APKs, and is checked on the SDK's emulator; its zip is the released APK itself.
"""

import argparse
import io
import os
import re
import shutil
import subprocess
import sys
import tarfile
import time
import zipfile
from dataclasses import dataclass
from pathlib import Path

import android

REPO = Path(__file__).resolve().parent.parent
STORY = "Content/Story"
EXPANSION = STORY + "/disc2"
PROJECT_PATHS = ["Weltall.uproject", "Config", "Content/Story", "Shaders", "Source", "Docs", "Build/Windows/Application.ico", "Build/Android/res"]
PROJECT = REPO / "out" / "package" / "Weltall"
BUILDS = REPO / "out" / "builds"
RELEASES = REPO / "out" / "releases"
CONFIGURATIONS = ["DebugGame", "Development", "Shipping"]
CREATE_NO_WINDOW = 0x08000000
# The player's own folders beside the game, carried into every new build of a package.
PLAYER_FOLDERS = ["Assets", "Mods"]
# How long a finished package's move into place waits for a file in it that another program (a virus scan of the fresh
# files) still holds open: Windows refuses to move a folder meanwhile.
MOVE_TRIES = 30
MOVE_WAIT_SECONDS = 2
# A release's zips: Deflate, which every unzip reads (Windows' own among them), at its highest level.
ZIP_LEVEL = 9
UNIX = 3
MSDOS_DIRECTORY = 0x10

# What a package's log must show after its first seconds (and that it read every story document), and what it must never show.
STARTED = ["Now running"]
FAILED = ["The game cannot start", "The game stopped", "Fatal error", "LogWindows: Error", "LogLinux: Error", "LogAndroid: Error"]


@dataclass(frozen=True)
class Platform:
    name: str  # the folder its package lands in
    target: str  # UnrealBuildTool's and BuildCookRun's name for it
    binaries: str  # where the game's executable lies in the package
    launcher: str  # what starts the game, at the package's top
    log_suffix: str  # after the configuration in the logs' names
    symbols: tuple  # the suffixes of the executable's debug files beside it in the build


PLATFORMS = {
    "Windows": Platform("Windows", "Win64", "Weltall/Binaries/Win64", "Weltall.exe", "", (".pdb",)),
    "Linux": Platform("Linux", "Linux", "Weltall/Binaries/Linux", "Weltall.sh", "-Linux", (".debug", ".sym")),
    "Android": Platform("Android", "Android", ".", "", "-Android", ()),
}


def setting(key, required=True):
    if os.environ.get(key):
        return os.environ[key]
    env_file = REPO / "local.env"
    if env_file.is_file():
        for line in env_file.read_text(encoding="utf-8").splitlines():
            name, _, value = line.partition("=")
            if name.strip() == key and not line.lstrip().startswith("#"):
                return value.strip()
    if required:
        sys.exit(f"package: {key} is not set in the environment or local.env")
    return None


def project_version():
    """The exported project's ProjectVersion (Config/DefaultGame.ini)."""
    found = re.search(r"^ProjectVersion=(.*)$", (PROJECT / "Config" / "DefaultGame.ini").read_text(encoding="utf-8"), re.MULTILINE)
    if found is None:
        sys.exit("package: Config/DefaultGame.ini names no ProjectVersion")
    return found.group(1).strip()


def export_revision(revision, expansion):
    """Writes the project's files at revision into PROJECT, touching only those whose bytes changed; the expanded
    second disc's documents only with expansion."""
    # An older revision may lack a path (the icons came later): git archive refuses one it does not have.
    present = [path for path in PROJECT_PATHS
               if subprocess.run(["git", "-C", str(REPO), "cat-file", "-e", f"{revision}:{path}"], capture_output=True).returncode == 0]
    archive = subprocess.run(["git", "-C", str(REPO), "archive", "--format=tar", revision, *present],
                             capture_output=True, check=True).stdout
    wanted = set()
    with tarfile.open(fileobj=io.BytesIO(archive)) as tar:
        for member in tar.getmembers():
            if not member.isfile() or (not expansion and member.name.startswith(EXPANSION + "/")):
                continue
            target = PROJECT / member.name
            wanted.add(target)
            data = tar.extractfile(member).read()
            if not target.is_file() or target.read_bytes() != data:
                target.parent.mkdir(parents=True, exist_ok=True)
                target.write_bytes(data)
    for root in PROJECT_PATHS:
        base = PROJECT / root
        for path in [base] if base.is_file() else sorted(base.rglob("*"), reverse=True):
            if path.is_file() and path not in wanted:
                path.unlink()
            elif path.is_dir() and not any(path.iterdir()):
                path.rmdir()


# The cook's editor asks UnrealBuildTool for the platforms' SDKs without waiting on its mutex: while another build
# holds it, the platform's SDK is not found and the cook refuses the platform, or only the mutex's refusal is logged.
# That run is tried again a few times.
SDK_UNSEEN = ("Invalid target platform specified", "A conflicting instance of Global\\UnrealBuildTool_Mutex")
SDK_TRIES = 5
SDK_WAIT_SECONDS = 120


def run_logged(commands, log_path):
    """Runs each command in turn into one log; the first that fails ends the run with its status."""
    with open(log_path, "w", encoding="utf-8", errors="replace", buffering=1) as log:
        for command in commands:
            for attempt in range(1, SDK_TRIES + 1):
                log.write("> " + " ".join(command) + "\n")
                process = subprocess.Popen(command, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True,
                                           encoding="utf-8", errors="replace", creationflags=CREATE_NO_WINDOW)
                sdk_unseen = False
                for line in process.stdout:
                    log.write(line)
                    sdk_unseen = sdk_unseen or any(mark in line for mark in SDK_UNSEEN)
                    if "BUILD SUCCESSFUL" in line or "ERROR:" in line or "Error:" in line:
                        print("  " + line.rstrip())
                status = process.wait()
                if status == 0 or not sdk_unseen or attempt == SDK_TRIES:
                    break
                print(f"  another build held UnrealBuildTool as the cook looked for the SDKs: again in {SDK_WAIT_SECONDS} s")
                time.sleep(SDK_WAIT_SECONDS)
            if status:
                return status
    return 0


def build(configuration, platform, ue_root):
    """Compiles, cooks and archives a configuration, replacing its previous package only once all succeeded."""
    engine = Path(ue_root) / "Engine"
    project = PROJECT / "Weltall.uproject"
    # UBT is called directly to wait on its mutex: another project's build on the same engine may hold it,
    # and the builds BuildCookRun starts itself would fail rather than wait.
    ubt = str(engine / "Binaries" / "DotNET" / "UnrealBuildTool" / "UnrealBuildTool.exe")
    uat = str(engine / "Build" / "BatchFiles" / "RunUAT.bat")
    staging = BUILDS / f"{configuration}{platform.log_suffix}.new"
    shutil.rmtree(staging, ignore_errors=True)
    commands = [[ubt, "WeltallEditor", "Win64", "Development", f"-Project={project}", "-WaitMutex"]]
    if platform.name == "Android":
        # Android's archive is the folder given, where the desktops' is a folder named after the platform inside it.
        commands += android.commands(ubt, uat, project, configuration, staging / platform.name)
    else:
        commands.append([ubt, "Weltall", platform.target, configuration, f"-Project={project}", "-WaitMutex"])
        if platform.name == "Windows":
            commands.append([ubt, "BootstrapPackagedGame", "Win64", "Shipping", "-WaitMutex"])
        commands.append([uat, "-WaitForUATMutex", "BuildCookRun", f"-project={project}", f"-platform={platform.target}",
                         f"-clientconfig={configuration}", "-skipbuild", "-cook", "-stage", "-pak", "-package", "-archive",
                         f"-archivedirectory={staging}", "-nodebuginfo", "-SkipCookingEditorContent", "-noP4", "-unattended", "-utf8output"])
    log_path = BUILDS / f"{configuration}{platform.log_suffix}.log"
    print(f"{configuration}: building for {platform.name} (log: {log_path.relative_to(REPO)})")
    started = time.monotonic()
    status = run_logged(commands, log_path)
    minutes = (time.monotonic() - started) / 60
    if status != 0:
        print(f"{configuration}: FAILED with exit status {status} after {minutes:.0f} min")
        return False
    # Only this platform's package is replaced: another platform's beside it stays. A package that is being played
    # is left whole: its folder cannot be moved aside while the game holds its files, and the new one waits beside it.
    package = BUILDS / configuration / platform.name
    package.parent.mkdir(parents=True, exist_ok=True)
    previous = package.with_name(package.name + ".old")
    for kept in PLAYER_FOLDERS:
        if (previous / kept).exists():
            print(f"Local {kept.lower()} remain in {previous / kept}; move them to a safe folder before rebuilding")
            return False
    shutil.rmtree(previous, ignore_errors=True)
    if package.exists():
        try:
            package.rename(previous)
        except PermissionError:
            print(f"{configuration}: built in {minutes:.0f} min, but {package.relative_to(REPO)} is in use (is the game running?): "
                  f"the new package stays in {(staging / platform.name).relative_to(REPO)}; close the game and run this again")
            return False
    move_into_place(staging / platform.name, package)
    if not retain_player_folders(previous, package):
        return False
    if platform.name != "Android":
        shutil.copytree(PROJECT / "Docs", package / "Docs", dirs_exist_ok=True)
    shutil.rmtree(previous, ignore_errors=True)
    shutil.rmtree(staging, ignore_errors=True)
    launcher = android.apk(package, configuration, android.RELEASED) if platform.name == "Android" else package / platform.launcher
    print(f"{configuration}: built in {minutes:.0f} min: {launcher.relative_to(REPO)}")
    return True


def retain_player_folders(previous, package):
    """Moves the player's own replacements and mods out of the old build before that build is removed."""
    for kept in PLAYER_FOLDERS:
        source = previous / kept
        if not source.exists():
            continue
        target = package / kept
        try:
            if target.exists():
                raise FileExistsError(f"the new package already has a {kept} folder")
            source.rename(target)
        except OSError as error:
            print(f"{kept} stay in {source}: {error}")
            return False
    return True


def move_into_place(built, package):
    for _ in range(MOVE_TRIES - 1):
        try:
            built.rename(package)
            return
        except PermissionError:
            time.sleep(MOVE_WAIT_SECONDS)
    built.rename(package)


def link_games(configuration, platform):
    """Hard-links the checkout's discs beside the package: removing either side never removes the other's."""
    source = REPO / "Games"
    target = BUILDS / configuration / platform.name / "Games"
    for disc in (p for p in source.rglob("*") if p.is_file()):
        link = target / disc.relative_to(source)
        if link.exists():
            continue
        link.parent.mkdir(parents=True, exist_ok=True)
        try:
            os.link(disc, link)
        except OSError:
            print(f"{configuration}: could not link the discs (another drive?); copy them into {target}")
            return


def wsl_path(path):
    """A Windows path as WSL mounts it: E:\\a\\b is /mnt/e/a/b."""
    resolved = path.resolve()
    return "/mnt/" + resolved.drive[0].lower() + resolved.as_posix()[2:]


def check(configuration, platform, seconds):
    """Starts the package for some seconds, then reads its log: a desktop one without a window or sound (a Linux one in
    WSL), an Android one on the emulator."""
    package = BUILDS / configuration / platform.name
    log_path = BUILDS / f"{configuration}{platform.log_suffix}-check.log"
    log_path.unlink(missing_ok=True)
    try:
        ended = run_on_emulator(package, configuration, seconds, log_path) if platform.name == "Android" else run_desktop(package, platform, seconds, log_path)
    except RuntimeError as error:
        print(f"{configuration}: FAILED: {error}")
        return False
    text = log_path.read_text(encoding="utf-8", errors="replace") if log_path.is_file() else ""
    documents = len(list((PROJECT / STORY).glob("*/*.txt")))
    missing = [line for line in [*STARTED, f"Story documents read: {documents}"] if line not in text]
    if not re.search(r"Found [1-9][0-9]* of the game's discs", text):
        missing.append("a recognized game disc")
    failures = [line for line in text.splitlines() if any(mark in line for mark in FAILED)]
    for line in failures[:10]:
        print("  " + line)
    passed = ended is None and not missing and not failures
    if passed:
        print(f"{configuration}: ran for {seconds} s (log: {log_path.relative_to(REPO)})")
    else:
        print(f"{configuration}: FAILED: {ended or 'ran on'}, its log lacks {missing} (log: {log_path.relative_to(REPO)})")
    return passed


def run_on_emulator(package, configuration, seconds, log_path):
    """Runs an Android package's emulator APK for some seconds, its log in log_path: None if it still ran, else how it
    ended."""
    if not android.apk(package, configuration, android.CHECKED).is_file():
        raise RuntimeError(f"no Android package to check in {package.relative_to(REPO)}")
    running = android.check(package, configuration, PROJECT, setting("ANDROID_HOME"), setting("ANDROID_AVD", required=False),
                            setting("ANDROID_CHECK_DISC"), seconds, log_path)
    return None if running else "stopped on the emulator"


def run_desktop(package, platform, seconds, log_path):
    """Runs a desktop package without a window or sound for some seconds, its log in log_path: None if it still ran,
    else how it ended."""
    binaries = package / platform.binaries
    executable = next((path for path in binaries.glob("Weltall*") if path.suffix in ("", ".exe")), None) if binaries.is_dir() else None
    if executable is None:
        raise RuntimeError(f"no {platform.name} package to check in {package.relative_to(REPO)}")
    arguments = ["-nullrhi", "-nosound", "-unattended"]
    if platform.name == "Linux":
        # Killed by timeout inside WSL (which then exits with 124): ending the wsl process alone would leave the game running.
        command = ["wsl", "-d", setting("WSL_DISTRO"), "--", "timeout", "--signal=KILL", str(seconds), wsl_path(executable), *arguments,
                   f"-abslog={wsl_path(log_path)}"]
        limit = seconds + 30
    else:
        command = [str(executable), *arguments, f"-abslog={log_path}"]
        limit = seconds
    process = subprocess.Popen(command, creationflags=CREATE_NO_WINDOW)
    try:
        exited = process.wait(timeout=limit)
    except subprocess.TimeoutExpired:
        process.kill()
        process.wait()
        return None
    return None if platform.name == "Linux" and exited == 124 else f"exited with status {exited}"


def write_zip(release, root, files, folders=(), programs=None):
    """A zip of files (their paths in it relative to root) and of empty folders. With programs, a Linux package's files
    that run, its entries are marked as made on Unix with their modes, which an unzip there restores: execute bits kept."""
    release.unlink(missing_ok=True)
    with zipfile.ZipFile(release, "w") as written:
        for path in files:
            entry = zipfile.ZipInfo.from_file(path, path.relative_to(root).as_posix())
            if programs is not None:
                entry.create_system = UNIX
                entry.external_attr = (0o100755 if path in programs else 0o100644) << 16
            written.writestr(entry, path.read_bytes(), compress_type=zipfile.ZIP_DEFLATED, compresslevel=ZIP_LEVEL)
        for folder in folders:
            entry = zipfile.ZipInfo(folder + "/", time.localtime()[:6])
            if programs is not None:
                entry.create_system = UNIX
            entry.external_attr = 0o40755 << 16 | MSDOS_DIRECTORY
            written.writestr(entry, b"")


def archive(configuration, platform):
    """Writes the package, its discs left out, and its symbols as zip archives in RELEASES; false if it cannot."""
    package = BUILDS / configuration / platform.name
    if platform.name == "Android":
        return archive_android(configuration, package)
    revision_file = package / "Revision.txt"
    binaries = package / platform.binaries
    executable = next((path for path in binaries.glob("Weltall*") if path.suffix in ("", ".exe")), None) if binaries.is_dir() else None
    if not revision_file.is_file() or executable is None:
        print(f"{configuration}: no {platform.name} package to archive")
        return False
    revision = revision_file.read_text(encoding="utf-8").strip()
    name = f"Weltall-{revision}-{platform.name}" + ("" if configuration == "Shipping" else f"-{configuration}")
    if (package / "Weltall" / EXPANSION).is_dir():
        name += "-Expansion"
    RELEASES.mkdir(parents=True, exist_ok=True)
    # Local disc images and exports never enter a release archive.
    kept = {"games"} | {folder.casefold() for folder in PLAYER_FOLDERS}
    files = sorted(path for path in package.rglob("*") if path.is_file() and path.relative_to(package).parts[0].casefold() not in kept)
    release = RELEASES / f"{name}.zip"
    programs = None
    if platform.name == "Linux":
        programs = {package / platform.launcher} | {path for path in binaries.iterdir() if path.suffix == ""}
    write_zip(release, package, files, ["Games"] + PLAYER_FOLDERS, programs)
    built = PROJECT / "Binaries" / Path(platform.binaries).name
    symbols = [path for path in built.glob(executable.stem + ".*") if path.suffix in platform.symbols]
    symbols_archive = RELEASES / f"{name}-symbols.zip"
    symbols_archive.unlink(missing_ok=True)
    if symbols:
        write_zip(symbols_archive, built, symbols)
    unpacked = sum(path.stat().st_size for path in files)
    print(f"{configuration}: {release.relative_to(REPO)}: {release.stat().st_size / 2**20:.1f} MiB, {unpacked / 2**20:.1f} MiB unpacked; "
          + (f"symbols {symbols_archive.stat().st_size / 2**20:.1f} MiB" if symbols else f"no symbols in {built.relative_to(REPO)}"))
    return True


def archive_android(configuration, package):
    """Copies the package's released APK into RELEASES as it is (players install the APK itself) and zips its symbols
    beside it."""
    revision_file = package / "Revision.txt"
    apk, folder, symbols = android.release_files(package, configuration)
    if not revision_file.is_file() or not apk.is_file():
        print(f"{configuration}: no Android package to archive")
        return False
    name = f"Weltall-{revision_file.read_text(encoding='utf-8').strip()}-Android" + ("" if configuration == "Shipping" else f"-{configuration}")
    # The story documents are inside the APK: whether they hold the expanded second disc is the export's to say.
    if (PROJECT / EXPANSION).is_dir():
        name += "-Expansion"
    RELEASES.mkdir(parents=True, exist_ok=True)
    release = RELEASES / f"{name}.apk"
    shutil.copyfile(apk, release)
    symbols_archive = RELEASES / f"{name}-symbols.zip"
    symbols_archive.unlink(missing_ok=True)
    if symbols:
        write_zip(symbols_archive, folder, symbols)
    print(f"{configuration}: {release.relative_to(REPO)}: {release.stat().st_size / 2**20:.1f} MiB; "
          + (f"symbols {symbols_archive.stat().st_size / 2**20:.1f} MiB" if symbols else "no symbols in the package"))
    return True


def main():
    sys.stdout.reconfigure(line_buffering=True)
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("configurations", nargs="*", default=["Development"], help=", ".join(CONFIGURATIONS))
    parser.add_argument("--platform", default="Windows", choices=sorted(PLATFORMS), help="the platform to package for (default: Windows)")
    parser.add_argument("--revision", default="HEAD", help="the commit to build (default: HEAD)")
    parser.add_argument("--check-only", action="store_true", help="check existing packages without building")
    parser.add_argument("--check-seconds", type=int, default=60, help="how long the check runs each package")
    parser.add_argument("--archive", action="store_true", help="write each package that ran as a zip archive in out/releases")
    parser.add_argument("--expansion", action="store_true", help="keep the expanded second disc's documents, unfinished as they are")
    args = parser.parse_args()
    for configuration in args.configurations:
        if configuration not in CONFIGURATIONS:
            parser.error(f"unknown configuration {configuration}: choose from {', '.join(CONFIGURATIONS)}")

    platform = PLATFORMS[args.platform]
    ok = True
    if not args.check_only:
        ue_root = setting("UE_ROOT")
        if platform.name == "Linux":
            os.environ["LINUX_MULTIARCH_ROOT"] = setting("LINUX_MULTIARCH_ROOT")
        keystore = None
        if platform.name == "Android":
            os.environ.update(ANDROID_HOME=setting("ANDROID_HOME"), NDKROOT=setting("NDKROOT"), JAVA_HOME=setting("ANDROID_JAVA_HOME"))
            keystore = setting("ANDROID_KEYSTORE", required="Shipping" in args.configurations)
        revision = subprocess.run(["git", "-C", str(REPO), "rev-parse", "--short", args.revision],
                                  capture_output=True, text=True, check=True).stdout.strip()
        print(f"Exporting {revision} into {PROJECT.relative_to(REPO)}" + (" with the expanded second disc" if args.expansion else ""))
        export_revision(args.revision, args.expansion)
        if platform.name == "Android":
            android.prepare(PROJECT, project_version(), keystore, setting("ANDROID_KEYSTORE_PASSWORD") if keystore else None)
        BUILDS.mkdir(parents=True, exist_ok=True)
        for configuration in args.configurations:
            built = build(configuration, platform, ue_root)
            ok &= built
            if built:
                (BUILDS / configuration / platform.name / "Revision.txt").write_text(revision + "\n", encoding="utf-8")
                if platform.name != "Android":
                    link_games(configuration, platform)
    for configuration in args.configurations:
        checked = check(configuration, platform, args.check_seconds)
        ok &= checked
        if checked and args.archive:
            ok &= archive(configuration, platform)
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())

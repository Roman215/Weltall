"""The Android parts of package.py: the settings a package is built with, its commands, its check on the emulator, and
the files a release takes from it.

An Android package holds two APKs: one for 64-bit Arm, the phones' architecture and the one released, and one for
x86-64, which only the check runs, because the emulator cannot run the engine's Arm code. Both are signed with the key
in ANDROID_KEYSTORE (a keystore whose key is named weltall), its password in ANDROID_KEYSTORE_PASSWORD. Every later
version of the app must be signed with that same key, or Android refuses it as an update and the player has to
uninstall, losing the app's folder. A Shipping package is a release's and needs the key; without it, the other
configurations are signed with the SDK's debug key. The version code and name come from ProjectVersion.

The check runs the x86-64 APK on an emulator: the one already running, or else ANDROID_AVD started without a window
and shut down again afterwards. The disc image in ANDROID_CHECK_DISC is copied into the app's Games folder unless it is
there already, and the game's log is read back from the app's folder. A phone joined over USB is never used.
"""

import re
import shlex
import shutil
import subprocess
import time
from pathlib import Path

ARCHITECTURES = ("arm64", "x64")
RELEASED = "arm64"
CHECKED = "x64"
SETTINGS = "/Script/AndroidRuntimeSettings.AndroidRuntimeSettings"
KEYSTORE = "weltall.keystore"
KEY_ALIAS = "weltall"
CREATE_NO_WINDOW = 0x08000000
BOOT_SECONDS = 300
FOLDER_SECONDS = 30
# Where a disc's file waits on the device before the shell copies it into the app's folder.
STAGING = "/data/local/tmp"


def version_code(version):
    """Android's version code for a version, which grows with every release: 0.8.5 is 805, 1.0.0 is 10000."""
    major, minor, patch = (int(part) for part in version.split("."))
    if minor > 99 or patch > 99:
        raise ValueError(f"{version} has no version code: its second and third numbers must stay below 100")
    return major * 10000 + minor * 100 + patch


def prepare(project, version, keystore, password):
    """Gives the project's Android build its version and, with a keystore, the key that signs it: the keystore copied
    into the project, where the build looks for it, and its password in a generated settings file, which the engine
    leaves out of the packages it stages."""
    lines = [f"[{SETTINGS}]", f"StoreVersion={version_code(version)}", f"VersionDisplayName={version}"]
    copied = project / "Build" / "Android" / KEYSTORE
    if keystore:
        copied.parent.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(keystore, copied)
        for kind in ("", "Debug"):
            lines += [f"{kind}KeyStore={KEYSTORE}", f"{kind}KeyAlias={KEY_ALIAS}", f"{kind}KeyStorePassword={password}",
                      f"{kind}KeyPassword=_sameaskeystore_"]
    else:
        copied.unlink(missing_ok=True)
    (project / "Config" / "Android" / "GeneratedAndroidEngine.ini").write_text("\n".join(lines) + "\n", encoding="utf-8")


def commands(ubt, uat, project, configuration, archive):
    """The game compiled for each architecture, then cooked once and packaged for each into archive."""
    compiled = [[ubt, "Weltall", "Android", configuration, f"-Project={project}", f"-Architecture={architecture}", "-WaitMutex"]
                for architecture in ARCHITECTURES]
    packaged = []
    for architecture in ARCHITECTURES:
        command = [uat, "-WaitForUATMutex", "BuildCookRun", f"-project={project}", "-platform=Android",
                   f"-clientarchitecture={architecture}", "-cookflavor=ETC2", f"-clientconfig={configuration}", "-skipbuild",
                   "-cook" if architecture == ARCHITECTURES[0] else "-skipcook", "-stage", "-pak", "-package", "-archive",
                   f"-archivedirectory={archive}", "-SkipCookingEditorContent", "-noP4", "-unattended", "-utf8output"]
        if configuration == "Shipping":
            command.append("-distribution")
        packaged.append(command)
    return compiled + packaged


def apk(package, configuration, architecture):
    """A package's APK as the engine names it: by the configuration, except Development's."""
    named = "" if configuration == "Development" else f"-Android-{configuration}"
    return package / f"Weltall{named}-{architecture}.apk"


def release_files(package, configuration):
    """The released APK, and its symbols' folder with the files in it that turn a crash report into source lines."""
    folder = next(package.glob("Weltall_Symbols_v*"), None)
    symbols = []
    if folder is not None:
        symbols = sorted(path for path in (folder / f"Weltall{RELEASED}").rglob("*") if path.is_file())
        symbols += [path for path in [folder / "mapping.txt"] if path.is_file()]
    return apk(package, configuration, RELEASED), folder, symbols


def package_name(project):
    found = re.search(r"^PackageName=(.+)$", (project / "Config" / "DefaultEngine.ini").read_text(encoding="utf-8"), re.MULTILINE)
    return found.group(1).strip()


def disc_files(disc):
    """A disc image's files: a cue sheet and the tracks it names, or the one file of any other image."""
    if disc.suffix.lower() != ".cue":
        return [disc]
    named = re.findall(r'^\s*FILE\s+(?:"([^"]+)"|(\S+))', disc.read_text(encoding="utf-8", errors="replace"), re.MULTILINE | re.IGNORECASE)
    return [disc] + [disc.parent / (quoted or bare) for quoted, bare in named]


class Emulator:
    """An emulator adb reaches, by its serial; started from an AVD when none runs, and then shut down by close()."""

    def __init__(self, sdk, avd, log_path):
        self.adb = str(Path(sdk) / "platform-tools" / "adb.exe")
        self.started = None
        self.serial = self.running()
        if self.serial is None:
            if not avd:
                raise RuntimeError("no emulator is running and ANDROID_AVD names none to start")
            log = open(log_path, "w", encoding="utf-8", errors="replace")
            self.started = subprocess.Popen([str(Path(sdk) / "emulator" / "emulator.exe"), "-avd", avd, "-no-window", "-no-audio",
                                             "-gpu", "host", "-no-boot-anim"], stdout=log, stderr=subprocess.STDOUT,
                                            stdin=subprocess.DEVNULL, creationflags=CREATE_NO_WINDOW)
            self.serial = self.booted()

    def running(self):
        listed = subprocess.run([self.adb, "devices"], capture_output=True, text=True, creationflags=CREATE_NO_WINDOW).stdout
        return next((line.split()[0] for line in listed.splitlines() if line.startswith("emulator-") and line.split()[-1] == "device"), None)

    def booted(self):
        deadline = time.monotonic() + BOOT_SECONDS
        while time.monotonic() < deadline:
            if self.started.poll() is not None:
                raise RuntimeError(f"the emulator exited with status {self.started.returncode} as it started")
            serial = self.running()
            if serial and subprocess.run([self.adb, "-s", serial, "shell", "getprop", "sys.boot_completed"], capture_output=True,
                                         text=True, creationflags=CREATE_NO_WINDOW).stdout.strip() == "1":
                return serial
            time.sleep(5)
        self.close()
        raise RuntimeError(f"the emulator did not finish starting in {BOOT_SECONDS} s")

    def run(self, *arguments, binary=False):
        return subprocess.run([self.adb, "-s", self.serial, *arguments], capture_output=True, text=not binary,
                              creationflags=CREATE_NO_WINDOW)

    def shell(self, command):
        return self.run("shell", command).stdout.strip()

    def close(self):
        if self.started is None:
            return
        if self.serial:
            self.run("emu", "kill")
        try:
            self.started.wait(timeout=60)
        except subprocess.TimeoutExpired:
            self.started.kill()
            self.started.wait()
        self.started = None


def install(emulator, apk_path, name):
    """Installs the APK over the app's last version; one signed with another key replaces it, the app's folder lost."""
    result = emulator.run("install", "-r", str(apk_path))
    if "INSTALL_FAILED_UPDATE_INCOMPATIBLE" in result.stdout + result.stderr:
        print(f"  the emulator's {name} was signed with another key: uninstalled, its folder with it")
        emulator.run("uninstall", name)
        result = emulator.run("install", str(apk_path))
    if result.returncode != 0 or "Success" not in result.stdout:
        raise RuntimeError(f"the APK did not install: {(result.stdout + result.stderr).strip()[-300:]}")


def start(emulator, name):
    emulator.shell(f"am force-stop {name}")
    emulator.shell(f"monkey -p {name} -c android.intent.category.LAUNCHER 1")


def provide_disc(emulator, name, disc):
    """Copies the disc's files into the app's Games folder unless they are there at their size. adb can be refused a new
    folder in an app's own, and the app makes Games as it starts, open to adb's group: the app is started once before a
    copy, until the folder is writable."""
    games = Path(f"/sdcard/Android/data/{name}/files/Games")
    wanted = [local for local in disc_files(disc)
              if emulator.shell(f"stat -c %s {shlex.quote((games / local.name).as_posix())} 2>/dev/null") != str(local.stat().st_size)]
    if not wanted:
        return
    start(emulator, name)
    deadline = time.monotonic() + FOLDER_SECONDS
    while emulator.shell(f"test -w {shlex.quote(games.as_posix())} && echo yes") != "yes" and time.monotonic() < deadline:
        time.sleep(1)
    emulator.shell(f"am force-stop {name}")
    for local in wanted:
        remote = (games / local.name).as_posix()
        print(f"  copying {local.name} into the emulator's Games folder")
        # adb's push into the app's folder also changes the file's owner, which the folder refuses: the file goes to the
        # shell's own folder first, then the shell copies it in, readable by the app.
        staged = f"{STAGING}/{local.name}"
        pushed = emulator.run("push", str(local), staged)
        emulator.shell(f"cp {shlex.quote(staged)} {shlex.quote(remote)} && chmod 644 {shlex.quote(remote)}; rm -f {shlex.quote(staged)}")
        if pushed.returncode != 0 or emulator.shell(f"stat -c %s {shlex.quote(remote)}") != str(local.stat().st_size):
            raise RuntimeError(f"{local.name} could not be copied: {pushed.stderr.strip()[-300:]}")


def check(package, configuration, project, sdk, avd, disc, seconds, log_path):
    """Runs the package's x86-64 APK on an emulator for some seconds and writes the game's log to log_path; whether
    the game still ran at the end."""
    name = package_name(project)
    game_log = f"/sdcard/Android/data/{name}/files/UnrealGame/Weltall/Weltall/Saved/Logs/Weltall.log"
    emulator = Emulator(sdk, avd, log_path.with_name(log_path.stem + "-emulator.log"))
    try:
        install(emulator, apk(package, configuration, CHECKED), name)
        provide_disc(emulator, name, Path(disc))
        emulator.shell(f"rm -f {shlex.quote(game_log)}")
        start(emulator, name)
        time.sleep(seconds)
        running = emulator.shell(f"pidof {name}") != ""
        log_path.write_bytes(emulator.run("exec-out", "cat", game_log, binary=True).stdout)
        emulator.shell(f"am force-stop {name}")
        return running
    finally:
        emulator.close()

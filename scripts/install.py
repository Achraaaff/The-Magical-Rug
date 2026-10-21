#!/usr/bin/env python3
"""Install this user's rug and shortcut without replacing loaded native libraries."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import platform
import shutil
import subprocess
import sys
import xml.etree.ElementTree as ET

ROOT = Path(__file__).resolve().parent.parent
COMPONENT = "org.local.desktoprug.lock.desktop"
WIDGET = "org.local.desktoprug.cloth"


def run(*args):
    return subprocess.run(args, check=True, text=True, capture_output=True).stdout.strip()


def copy_file(source, destination):
    destination.parent.mkdir(parents=True, exist_ok=True)
    if destination.exists() and source.read_bytes() == destination.read_bytes():
        return
    shutil.copy2(source, destination)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--prebuilt", action="store_true", help="Use the saved Fedora 43 x86_64 / Qt 6.10.3 renderer")
    parser.add_argument("--no-activate", action="store_true", help="Stage files only; do not contact the desktop or shortcut daemon")
    args = parser.parse_args()
    if args.prebuilt:
        saved = ROOT / "prebuilt/fedora43-x86_64-qt6.10.3"
        manifest = json.loads((saved / "manifest.json").read_text())
        if platform.machine() != manifest["architecture"]:
            raise RuntimeError("This prebuilt renderer requires x86_64. Build from source on this machine.")
        native = saved / "native-v5"
        for name, expected in manifest["sha256"].items():
            if hashlib.sha256((native / name).read_bytes()).hexdigest() != expected:
                raise RuntimeError(f"Prebuilt checksum mismatch: {name}")
        qtpaths = shutil.which("qtpaths6") or shutil.which("qtpaths-qt6")
        if not qtpaths or run(qtpaths, "--qt-version") != manifest["qt_version"]:
            raise RuntimeError("Prebuilt restore requires Qt 6.10.3; use scripts/build.sh for another Qt version.")
        package = ROOT / "plasma"
    else:
        package = ROOT / "build/package"
        native = package / "contents/ui/native-v5"
    library = native / "libdesktopruglock.so"
    view = package / "contents/ui/ClothView-v5.qml"
    if not library.is_file():
        raise RuntimeError("Build first with scripts/build.sh, or use --prebuilt on the original laptop.")
    qdbus = shutil.which("qdbus-qt6") or shutil.which("qdbus6") or shutil.which("qdbus")
    if not qdbus:
        raise RuntimeError("Qt's qdbus command is required.")
    if not args.no_activate:
        if not shutil.which("gdbus"):
            raise RuntimeError("The gdbus command is required to register the global shortcut.")
        available = run(qdbus, "org.kde.kglobalaccel", "/kglobalaccel", "org.kde.KGlobalAccel.isGlobalShortcutAvailable", "201326674", COMPONENT)
        if available != "true":
            raise RuntimeError("Ctrl+Alt+R is already assigned to another global shortcut. Free it in KDE Settings first.")
    # Every build gets its own QML view, module URI, and native-library path.
    # Reinstalling never truncates a .so currently mapped by plasmashell.
    digest = hashlib.sha256(library.read_bytes() + view.read_bytes()).hexdigest()
    revision = int(digest[:7], 16) + 6
    module = "DesktopRugBuild" + digest[:12]
    plugin = "desktoprugbuild" + digest[:12]
    directory = "native-v5-" + digest[:12]
    data = Path(os.environ.get("XDG_DATA_HOME", Path.home() / ".local/share"))
    installed = data / "plasma/plasmoids" / WIDGET
    for name in ["metadata.json", "contents/ui/main.qml", "contents/assets/persian.jpg"]:
        copy_file(package / name, installed / name)
    copy_file(ROOT / "THIRD_PARTY_NOTICES.md", installed / "LICENSE.third-party")
    runtime = installed / "contents/ui" / directory
    runtime.mkdir(parents=True, exist_ok=True)
    destination = runtime / ("lib" + plugin + ".so")
    if destination.exists() and destination.read_bytes() != library.read_bytes():
        raise RuntimeError("Native build fingerprint collision; refusing to overwrite an existing renderer.")
    copy_file(library, destination)
    for name in ["cloth.vert.qsb", "cloth.frag.qsb"]:
        copy_file(native / name, runtime / name)
    (runtime / "qmldir").write_text(f"module {module}\nplugin {plugin}\n")
    (installed / "contents/ui" / f"ClothView-v{revision}.qml").write_text(view.read_text().replace('"native-v5"', json.dumps(directory)))
    schema = (package / "contents/config/main.xml").read_text().replace("<default>5</default>", f"<default>{revision}</default>")
    ET.fromstring(schema)
    config = installed / "contents/config/main.xml"
    config.parent.mkdir(parents=True, exist_ok=True)
    config.write_text(schema)
    command = '"' + qdbus.replace('\\', '\\\\').replace('"', '\\"') + '" org.kde.plasmashell /org/local/DesktopRug org.local.DesktopRug.ToggleLock'
    definition = (ROOT / "shortcuts" / COMPONENT).read_text()
    definition = "\n".join("Exec=" + command if line.startswith("Exec=") else line for line in definition.splitlines()) + "\n"
    shortcut = data / "kglobalaccel" / COMPONENT
    shortcut.parent.mkdir(parents=True, exist_ok=True)
    shortcut.write_text(definition)
    # The hidden application entry permits registering the same command now;
    # kglobalaccel/ supplies its definition automatically on subsequent logins.
    application = data / "applications" / COMPONENT
    application.parent.mkdir(parents=True, exist_ok=True)
    application.write_text(definition + "NoDisplay=true\n")
    if args.no_activate:
        print(f"Staged {installed}; activation skipped.")
        return
    cache = shutil.which("kbuildsycoca6")
    if cache:
        run(cache)
    script = f'''var ds=desktops();var d=ds.filter(function(x){{return x.screen===0}})[0]||ds[0];
if(!d)throw new Error("No Plasma desktop found");
var w=d.widgets().filter(function(x){{return x.type==={json.dumps(WIDGET)}}})[0]||d.addWidget({json.dumps(WIDGET)});
w.currentConfigGroup=["General"];w.writeConfig("rendererRevision",{revision});w.reloadConfig();print("The Magical Rug installed");'''
    print(run(qdbus, "org.kde.plasmashell", "/PlasmaShell", "org.kde.PlasmaShell.evaluateScript", script))
    action = "['" + COMPONENT + "', '_launch', 'Desktop Rug', 'Lock or unlock Desktop Rug']"
    run("gdbus", "call", "--session", "--dest", "org.kde.kglobalaccel", "--object-path", "/kglobalaccel", "--method", "org.kde.KGlobalAccel.doRegister", action)
    run("gdbus", "call", "--session", "--dest", "org.kde.kglobalaccel", "--object-path", "/kglobalaccel", "--method", "org.kde.KGlobalAccel.setShortcut", action, "[201326674]", "6")
    print("Ctrl+Alt+R locks/unlocks the rug. Plasma restores it automatically after login.")


if __name__ == "__main__":
    try:
        main()
    except (RuntimeError, OSError, subprocess.CalledProcessError) as error:
        sys.exit(str(error))

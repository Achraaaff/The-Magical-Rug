# The Magical Rug

An interactive cloth rug for the KDE Plasma 6 desktop. Lift a corner to uncover desktop files, fold it back over the top, or move the whole rug. It is a separate desktop widget, so your wallpaper stays independent.

**On the original laptop, it is already installed.** Plasma restores the rug automatically when you log in after a restart. You do not need to start a server, run a script, or keep this repository open. Its saved shape, position, size, and lock state are stored in Plasma's widget configuration.

## Controls

| Action | Control |
| --- | --- |
| Move the whole rug | Drag the middle |
| Lift and fold a corner | Drag an edge or corner |
| Lock or unlock | **Ctrl+Alt+R**, anywhere in the desktop session |
| Resize | Ctrl-drag, or use the hover toolbar |
| Rotate | Mouse wheel, Shift-drag, or the hover toolbar |
| Fold or flatten | Double-click, or use **Fold / flat** |

Locking freezes the current pose and disables dragging, scrolling, hover feedback, and the toolbar. There is no lock badge, notification, or change to the rug's appearance. Mouse input passes through to the desktop. Unlocking restores interaction without moving the rug.

The shortcut is named **Lock or unlock Desktop Rug** in KDE's global shortcut settings. It controls this rug, independently of the desktop session's screen lock.

## Supported environment

This version was developed and checked on **Fedora Linux 43, KDE Plasma 6.7.3, Wayland, Qt 6.10.3, x86_64**, using Intel Iris Xe graphics. An NVIDIA GPU is not required.

The widget uses Plasma, Qt Quick, and the session D-Bus. It requires a KDE Plasma 6 desktop session. GNOME and other desktop environments are outside this implementation's scope. The saved binary is specific to the original platform and Qt version; build from source for another environment. Compatibility with other Qt/Plasma versions has not been verified.

## Restore the saved installation

For the original Fedora/Qt environment:

```sh
git clone https://github.com/Achraaaff/The-Magical-Rug.git
cd The-Magical-Rug
python3 scripts/install.py --prebuilt
```

The repository is private, so cloning requires a GitHub account with access. Run the installer as your normal desktop user from inside the Plasma session. It installs the widget and shortcut, adds the rug if necessary, and reuses the existing rug when present. No `sudo` is needed for this step.

The prebuilt renderer is accompanied by SHA-256 checksums and its Qt/platform version in `prebuilt/fedora43-x86_64-qt6.10.3/manifest.json`. The installer verifies these before using it.

## Build from source

Install a C++17 compiler, Python 3, pkg-config, Qt 6 development headers, `moc`, `qsb`, `qdbus`, `gdbus`, and Plasma's `kbuildsycoca6`. On Fedora, the development dependencies can be installed with:

```sh
sudo dnf install gcc-c++ pkgconf-pkg-config python3 \
  qt6-qtbase-devel qt6-qtdeclarative-devel qt6-qtshadertools-devel \
  qt6-qttools glib2
```

Then build and install:

```sh
./scripts/build.sh
python3 scripts/install.py
```

The build runs one compiler process at a time and writes to the ignored `build/` directory. It compiles the native plugin and both shaders for Qt's rendering backends. It does not start a development server or a background service.

For a nonstandard Qt development installation, set `QT_INCLUDE_DIR`, `QT_MOC`, and optionally `QT_LIB_DIR` and `QT_QSB`. These are developer overrides; do not point them at headers from a different Qt version than your runtime.

To stage files without contacting your desktop or changing active shortcuts:

```sh
XDG_DATA_HOME=/path/to/staging python3 scripts/install.py --prebuilt --no-activate
```

## Verification

Run the focused native interaction check:

```sh
./scripts/build.sh --test
```

The test uses Qt's offscreen platform to avoid interfering with your mouse or open applications. It checks locking during a move or fold, frozen geometry, disabled input and hover, click-through hit testing, saved lock restoration, and restored movement after unlocking. This is an input/state test; Qt's offscreen software renderer does not validate the custom shaders visually.

The installed version has also been checked through KDE's registered shortcut action for both locking and unlocking. Earlier manual-fold checks covered outward folds remaining on top after release, exposed desktop hit testing, and mesh intersections during slow and extended pulls. Those checks do not guarantee that every possible cloth configuration will be free of artifacts.

## How it works

- `src/cloth.hpp` contains the cloth solver: distance constraints, bending, gravity, floor friction, self-contact, and a movable grab point.
- `src/desktoprug.cpp` provides the Qt Quick item, pointer handling, saved state, lock control, and scene graph geometry.
- `src/shaders/` contains the textured front/back material and shadow shaders. The underside is opaque.
- `plasma/` contains the desktop widget, hover controls, texture, and configuration schema.
- `shortcuts/` contains the persistent KDE shortcut definition.
- `prebuilt/` preserves the working native binary and shaders from the original laptop.
- `scripts/` builds and installs the project; `tests/` checks lock behavior.

The solver pauses when the rug settles and stops immediately when locked. Middle dragging directly moves the mesh. Corner dragging follows a gradual lift-over path, with self-contact keeping the folded surface separated from the base.

The session D-Bus endpoint is `/org/local/DesktopRug` on `org.kde.plasmashell`, with interface `org.local.DesktopRug`. `ToggleLock` changes the lock and `IsLocked` reports it. One rug per Plasma process is supported by this control endpoint.

## Installation and saved state

The stable widget ID is `org.local.desktoprug.cloth`, retained so the existing installation and layout remain usable.

User-level installation locations are:

- `~/.local/share/plasma/plasmoids/org.local.desktoprug.cloth/`
- `~/.local/share/kglobalaccel/org.local.desktoprug.lock.desktop`
- `~/.local/share/applications/org.local.desktoprug.lock.desktop`

`XDG_DATA_HOME` overrides the shared data root. Plasma keeps the rug's `clothState` JSON and selected `rendererRevision` in the widget's **General** configuration group, normally within `~/.config/plasma-org.kde.plasma.desktop-appletsrc`. This repository deliberately excludes that file, personal desktop files, screenshots, account credentials, and icon layouts.

Each installation selects a build-specific QML module and library filename. Upgrading avoids overwriting a native library already loaded by Plasma, and does not restart the desktop. Existing widget state is preserved. The installer refuses to take **Ctrl+Alt+R** from another global shortcut.

## Troubleshooting and removal

- **No rug after login:** Confirm you are in a Plasma 6 session and that the Desktop Rug widget is still present. Run the installer to add or repair it.
- **The rug does not respond:** Press **Ctrl+Alt+R** once; it may be locked. There is intentionally no visible lock indicator.
- **Shortcut conflict:** Free **Ctrl+Alt+R** in KDE's shortcut settings before installation, or customize this rug's shortcut there afterward.
- **Qt changed after a system upgrade:** Rebuild with the installed development headers and run the installer. The saved prebuilt library is a backup for Qt 6.10.3.
- **Remove it:** Unlock the rug, use Plasma's desktop edit mode to remove its widget, and remove the **Lock or unlock Desktop Rug** shortcut in KDE Settings. The user-level files listed above may then be deleted after logging out of Plasma. This project does not install a system service.

## Credits and license

The native solver and Persian rug texture are adapted from [siliconbag/under-the-rug](https://github.com/siliconbag/under-the-rug), the MIT-licensed original macOS project. The Linux implementation adds the Plasma integration, native Qt rendering and input, revised corner handling and contacts, saved state, and a persistent global lock.

See [LICENSE](LICENSE) and [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md). The repository's private visibility does not replace or remove the upstream MIT notice.

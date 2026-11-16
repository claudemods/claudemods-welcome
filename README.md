<div align="center">
  <h1>ClaudeMods Hello</h1>
  <p><strong>Welcome screen for ClaudeMods — Qt 6 / QML edition</strong></p>
</div>

A full port of the Rust + GTK3 *CachyOS Welcome* (`cachyos-hello`) to **C++ / Qt 6 with a QML user interface**.
Everything the original did is here: the welcome page, info pages, Apps/Tweaks (systemd tweaks and system
utilities), Troubleshooting, DNS settings (plain / DoT / DoH / DoQ via blocky), the live-ISO installer launcher,
language switching, autostart and the `fix | tweak | dns | launch` command line interface.

## Build

Requirements: Qt ≥ 6.5 (Core, Gui, Qml, Quick, QuickControls2, DBus, Network, Concurrent), CMake ≥ 3.21,
a C++20 compiler.

```sh
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
sudo cmake --install build        # installs to /usr (binary, .desktop, icon, helper scripts)
```

Arch / ClaudeMods: `packaging/PKGBUILD`.

Run `claudemods-hello` for the GUI, or `claudemods-hello --help` for the CLI.

## Layout

| Path | What |
|---|---|
| `qml/Main.qml` | Window, header bar, message + about dialogs, page switching |
| `qml/pages/*.qml` | Home, info text pages, Tweaks, Troubleshooting, DNS |
| `qml/components/*.qml` | Shared button and back bar |
| `qml/I18n.qml` | `I18n.t("id", {args})` – reactive translation helper |
| `src/backend.*` | The `Backend` QML singleton (UI ↔ system glue, installer checks, release feed) |
| `src/actions.*` | pacman maintenance, tweaks, DNS actions |
| `src/systemd.*` | systemd over D-Bus (QtDBus, polkit interactive auth) |
| `src/networkmanager.*` | NetworkManager DNS settings via `nmcli` |
| `src/dnsdata.*` | DNS provider table, blocky config, latency probes (UDP / DoT / DoH) |
| `src/fluent.*` | Small Fluent (`.ftl`) reader – the 36 original translations are used unchanged |
| `src/cli.*` | Command line interface |
| `i18n/<lang>/claudemods_hello.ftl` | Translations |
| `data/pages/<lang>/` | Read me / Get involved pages |

## What was renamed (cachyos → claudemods)

Application identity: binary `claudemods-hello`, id `org.claudemods.hello`, desktop file, icons, install paths
(`/usr/share/claudemods-hello`), config (`~/.config/claudemods-hello.json`, `~/.config/claudemods/claudemods-hello/`),
translation file names/keys (`tweak-claudemodsupdate-tooltip`), all visible "CachyOS" text in the UI, translations and
info pages, the tweak id `claudemods-update`, and the blocky config marker (configs written by the old app are still
recognised).

## What intentionally keeps its upstream name

These point at things that exist **outside** this program; renaming them would break the feature. They all live in one
file, `src/externals.h` (and `data/preferences.json` for the web links) so you can swap in your own:

- packages / binaries: `cachyos-pi`, `cachyos-kernel-manager`, `cachyos-rate-mirrors`, `cachyos-gaming-meta`,
  `cachyos-gaming-applications`, `cachy-update`, `calamares-online.sh`, the pacman signing key id
- websites and feeds: cachyos.org (versions.json, rss.xml), wiki/forum/packages/discord/reddit links
- copyright / licence: GPL-3.0 and the original CachyOS copyright notice are kept in the About dialog and `LICENSE`

## Differences from the Rust version

- **Release page** is fetched at runtime from the blog RSS feed (the Rust build downloaded it at compile time).
- **NetworkManager** DNS changes use `nmcli` (retrying through `pkexec` if needed) instead of hand-written D-Bus calls.
- **Package checks** use `pacman -Q`; **GPU detection** reads `/sys/bus/pci` (no libalpm / chwd crates).
- **DoQ latency**: Qt has no public QUIC client, so a DoQ probe is measured as plain UDP. Applying DoQ still works
  (it is done by the blocky proxy).
- **Installer edition check** needs the `chwd` tool and its handheld profile database; if they are missing the check is
  skipped rather than blocking the installation. Verify it on a live ISO.
- Single-instance behaviour (second launch raises the window) is done with a local socket.

## Status

Written as a source-level port; it has **not been compiled or run yet** (no Qt toolchain was available while writing it).
Expect to fix a few compile errors on the first build, and test the privileged paths (tweaks, DNS, installer) on a real
ClaudeMods/CachyOS system.

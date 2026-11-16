<div align="center">
  <h1>claudemods Hello</h1>
  <p><strong>Welcome screen for claudemods — Qt 6 / QML edition</strong></p>
</div>

A full port of the Rust + GTK3 *CachyOS Welcome* (`cachyos-hello`) to **C++ / Qt 6 with a QML user interface**.
Everything the original did is here: the welcome page, info pages, Apps/Tweaks (systemd tweaks and system
utilities), Troubleshooting, DNS settings (plain / DoT / DoH / DoQ via blocky), the live-ISO installer launcher,
language switching, autostart and the `fix | tweak | dns | launch` command line interface.

## Features

**Welcome screen**
- Welcome text and links to documentation, support and project resources (wiki, forum, donate, GitHub), plus the claudemods website (claudemodsreloaded.co.uk and github.com/claudemods in the About dialog),
- Info pages: *Read me*, *Get involved* and a live *Release info* page built from the latest blog post.
- 36 interface languages, switchable at runtime; the choice is remembered. Info pages are translated for 24 of them.
- "Launch at start" autostart switch, About dialog, single-instance window.

**Live ISO**
- "Launch installer" with pre-flight checks: internet connectivity (HTTP, then ping fallback), edition / hardware
  compatibility (desktop vs handheld) and outdated / testing ISO warnings, then starts the Calamares installer.

**Apps / Tweaks** (installed systems)
- Systemd tweaks, each toggled with a checkbox (installs the package if needed, enables/starts or stops/disables the
  unit, optionally uninstalls on disable): Profile-sync-daemon, Systemd-oomd, Bpftune, Bluetooth, Ananicy Cpp and
  Cachy Update (tray notifier).
- Utilities: system update, remove orphans, rank mirrors, install gaming packages, install Winboat (also enables
  `docker.socket` and adds you to the `docker` group), install VRAM management (shown on Intel / AMD GPUs).
- **Chris Titus Linux Tool** — opens Konsole and runs [Chris Titus Tech's Linux toolbox](https://christitus.com/linux)
  (`curl -fsSL https://christitus.com/linux | sh`). It is a TUI, so Konsole closes when you quit it. Because it downloads
  and runs a third-party script, the app asks for confirmation first. CLI: `claudemods-hello fix christitus`.
- Application launchers for the claudemods Package Installer and Kernel Manager when they are installed.

**Troubleshooting**
- Clear package cache, remove the pacman db lock, reset pacman keyrings, reinstall all packages and, on KDE Wayland,
  show the KWin debug console.

**DNS settings** (needs NetworkManager)
- Pick a connection and one of 16 preset providers (Cloudflare, Google, Quad9, AdGuard, OpenDNS, Yandex, AliDNS, …),
  a custom server, or automatic DHCP.
- Transports: plain, DNS over TLS, DNS over HTTPS and DNS over QUIC (DoH / DoQ run through a local `blocky` proxy that
  is installed and configured for you).
- Latency test for the selected server and transport, "select best server" (fastest non-filtering provider),
  provider region / homepage info, and validation of custom addresses, hostnames, DoH URLs and DoQ endpoints.

**Command line** (`claudemods-hello --help`)
- `fix` — update-system, reinstall-packages, reset-keyrings, remove-lock, clear-cache, remove-orphans, rank-mirrors,
  install-gaming, show-kwin-debug, install-winboat, install-vram-management, christitus
- `tweak` — enable / disable / list
- `dns` — set, set-custom, reset, list-connections, list-servers, test-latency
- `launch` — package-installer, kernel-manager

## Credits

- Original application: **CachyOS Welcome** by Vladislav Nepogodin and the CachyOS team (GPL-3.0). This is a Qt 6 / QML
  port with claudemods branding.
- **Chris Titus Linux Tool** is created and maintained by [Chris Titus Tech](https://christitus.com/linux); it is a
  third-party project that this app only launches. It is not part of, or reviewed by, claudemods.
- DNS over HTTPS / QUIC support uses [blocky](https://github.com/0xERR0R/blocky).

## Build

Requirements: Qt ≥ 6.5 (Core, Gui, Qml, Quick, QuickControls2, DBus, Network, Concurrent), CMake ≥ 3.21,
a C++20 compiler.

```sh
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
sudo cmake --install build        # installs to /usr (binary, .desktop, icon, helper scripts)
```

Arch / claudemods: `packaging/PKGBUILD`.

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
claudemods/CachyOS system.

## Look

The window uses a dark palette over a space-nebula background (`data/img/background.webp`). WebP needs the Qt image
format plugin (`qt6-imageformats` on Arch, already listed in the PKGBUILD).

## Links

- Website: https://claudemodsreloaded.co.uk
- Software: https://github.com/claudemods

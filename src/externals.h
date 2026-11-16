#pragma once

// ---------------------------------------------------------------------------
// Names of things that live OUTSIDE this application (other distro packages,
// binaries, files and websites). These keep their original upstream names on
// purpose: renaming them would point the app at packages/URLs that do not
// exist. Change them here if you ship your own equivalents.
// ---------------------------------------------------------------------------
namespace ext {

// Our own identity (renamed from cachyos-hello / org.cachyos.hello)
inline constexpr auto kAppId       = "org.claudemods.hello";
inline constexpr auto kBinaryName  = "claudemods-hello";
inline constexpr auto kScriptsDir  = "/usr/share/claudemods-hello/scripts";
inline constexpr auto kLogDir      = "~/.config/claudemods/claudemods-hello";
inline constexpr auto kLogFile     = "claudemods-hello.log";

// External programs / packages
inline constexpr auto kPackageInstallerBin = "cachyos-pi";
inline constexpr auto kKernelManagerBin    = "cachyos-kernel-manager";
inline constexpr auto kRateMirrorsBin      = "cachyos-rate-mirrors";
inline constexpr auto kGamingMetaPkg       = "cachyos-gaming-meta";
inline constexpr auto kGamingAppsPkg       = "cachyos-gaming-applications";
inline constexpr auto kUpdateNotifierPkg   = "cachy-update";
inline constexpr auto kKeyringKeyId        = "F3B607488DB35A47";
inline constexpr auto kInstallerScript     = "/usr/local/bin/calamares-online.sh";

// Chris Titus Tech's Linux toolbox (third-party script, run on demand in Konsole)
inline constexpr auto kChrisTitusUrl = "https://christitus.com/linux";

// External websites / feeds
inline constexpr auto kVersionsUrl = "https://cachyos.org/versions.json";
inline constexpr auto kConnectUrl  = "https://cachyos.org";
inline constexpr auto kRssUrl      = "https://cachyos.org/rss.xml";
inline constexpr auto kBlogUrl     = "https://blog.cachyos.org";

// chwd (hardware detection) database used by the installer edition check
inline constexpr auto kChwdPciConfigDir = "/var/lib/chwd/db/pci";

} // namespace ext

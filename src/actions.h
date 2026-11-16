#pragma once

#include "dnsdata.h"
#include "tweak.h"
#include "utils.h"

#include <QString>
#include <QStringList>

#include <functional>

namespace actions {

enum class MessageType { Info = 0, Warning = 1, Error = 2 };
QString messageTypeName(MessageType t);

/// Receives dialog messages (GUI: popup, CLI: printed line).
using MessageSink = std::function<void(MessageType, const QString &message)>;
using utils::RunCmdCallback;

// --- pacman / system maintenance -------------------------------------------------------------
void removeDbLock(const MessageSink &sink);
void updateSystem(const RunCmdCallback &run);
void clearPkgCache(const RunCmdCallback &run);
void reinstallPackages(const RunCmdCallback &run);
void removeOrphans(const RunCmdCallback &run, const MessageSink &sink);
void resetKeyring(const RunCmdCallback &run);
void rankMirrors(const RunCmdCallback &run);
void installNeededPackages(const RunCmdCallback &run, const QStringList &packages, const QString &alreadyInstalledMsg,
                           const MessageSink &sink);
void installGaming(const RunCmdCallback &run, const MessageSink &sink);
void installVramManagement(const RunCmdCallback &run, const MessageSink &sink);
void installWinboat(const RunCmdCallback &run, const MessageSink &sink);
void launchKwinDebugWindow();
/// Opens Konsole and runs `curl -fsSL https://christitus.com/linux | sh` (Chris Titus Tech's
/// Linux tool). Konsole closes when the tool exits. Falls back to the generic terminal helper
/// (which adds a "press enter" prompt) when Konsole is not installed.
void launchChrisTitusTool(const RunCmdCallback &run);

/// Enables/disables a tweak. Returns false when the required package could not be installed.
bool toggleTweak(tweak::Name tweak, bool enable, const RunCmdCallback &run);

// --- DNS --------------------------------------------------------------------------------------
void changeDnsServer(const QString &conn, const QString &ipv4, const QString &ipv6, bool enableDot,
                     const QString &dotHostname, const MessageSink &sink);
void resetDnsServer(const QString &conn, const MessageSink &sink);
void changeDnsServerBlocky(const RunCmdCallback &run, const QString &conn, dns::BlockyMode mode,
                           const QString &upstream, const QString &bootstrapIpv4, const QString &bootstrapIpv6,
                           const QString &dotHostname, const MessageSink &sink);
void stopBlocky();
bool isBlockyActive();

} // namespace actions

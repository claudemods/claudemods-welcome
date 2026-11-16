#pragma once

#include <QByteArray>
#include <QString>
#include <QStringList>

#include <functional>

namespace utils {

enum class PacmanWrapper { Pak, Yay, Paru, Aura, Pacman };

/// Runs a shell command (inside a terminal for the GUI, directly for the CLI).
using RunCmdCallback = std::function<bool(const QString &command, bool escalate)>;

QString fixPath(const QString &path);
bool checkRegularFile(const QString &path);
QString findExecutable(const QString &name);

/// Runs `program args...`, returns stdout (empty on failure).
QString cmdOutput(const QString &program, const QStringList &args);
/// Runs `program args...`, returns the exit code (-1 if it could not start).
int runProcess(const QString &program, const QStringList &args, bool forwardOutput = false);

/// `bash -c cmd`, optionally through pkexec.
bool runCmd(const QString &cmd, bool escalate, bool forwardOutput = true);
/// Directory holding terminal-helper and rootshell.sh (installed copy, or the source tree for dev runs).
QString scriptsDir();
/// Runs `command` in a terminal window (the user presses enter to close it); `escalate` = as root via pkexec.
bool runInTerminal(const QString &command, bool escalate);
/// Opens `shellCommand` in Konsole (for TUI programs: no "press enter" prompt, Konsole closes when it exits).
/// Returns false when Konsole is not installed.
bool runInKonsole(const QString &shellCommand);

/// `pkexec args...` (arguments passed directly, no shell).
bool pkexecCmd(const QStringList &args);
bool spawnDetached(const QString &path);

PacmanWrapper pacmanWrapper();
bool isPkgInstalled(const QString &pkg);
bool isKwinWayland();
bool isIntelAmdGpu(const QString &vendorId, const QString &classId);
bool hasIntelOrAmdGpu();

QString readTextFile(const QString &path);

} // namespace utils

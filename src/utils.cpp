#include "utils.h"

#include "externals.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QStandardPaths>

namespace utils {

QString fixPath(const QString &path)
{
    if (!path.startsWith(QLatin1Char('~'))) {
        return path;
    }
    return QDir::homePath() + path.mid(1);
}

bool checkRegularFile(const QString &path)
{
    const QFileInfo info(path);
    return info.exists() && info.isFile();
}

QString findExecutable(const QString &name)
{
    if (name.contains(QLatin1Char('/'))) {
        return QFileInfo(name).isExecutable() ? name : QString();
    }
    return QStandardPaths::findExecutable(name);
}

QString cmdOutput(const QString &program, const QStringList &args)
{
    QProcess proc;
    proc.setProcessChannelMode(QProcess::SeparateChannels);
    proc.start(program, args);
    if (!proc.waitForFinished(60000)) {
        proc.kill();
        return {};
    }
    return QString::fromUtf8(proc.readAllStandardOutput());
}

int runProcess(const QString &program, const QStringList &args, bool forwardOutput)
{
    QProcess proc;
    if (forwardOutput) {
        proc.setProcessChannelMode(QProcess::ForwardedChannels);
        proc.setInputChannelMode(QProcess::ForwardedInputChannel);
    }
    proc.start(program, args);
    if (!proc.waitForStarted()) {
        return -1;
    }
    proc.waitForFinished(-1);
    if (proc.exitStatus() != QProcess::NormalExit) {
        return -1;
    }
    return proc.exitCode();
}

bool runCmd(const QString &cmd, bool escalate, bool forwardOutput)
{
    if (escalate) {
        return runProcess(QStringLiteral("pkexec"), {QStringLiteral("bash"), QStringLiteral("-c"), cmd}, forwardOutput) == 0;
    }
    return runProcess(QStringLiteral("bash"), {QStringLiteral("-c"), cmd}, forwardOutput) == 0;
}

bool pkexecCmd(const QStringList &args)
{
    return runProcess(QStringLiteral("pkexec"), args) == 0;
}

bool spawnDetached(const QString &path)
{
    return QProcess::startDetached(path, {});
}

PacmanWrapper pacmanWrapper()
{
    // Same lookup order as the original implementation
    if (!findExecutable(QStringLiteral("aura")).isEmpty()) {
        return PacmanWrapper::Aura;
    }
    if (!findExecutable(QStringLiteral("pak")).isEmpty()) {
        return PacmanWrapper::Pak;
    }
    if (!findExecutable(QStringLiteral("yay")).isEmpty()) {
        return PacmanWrapper::Yay;
    }
    if (!findExecutable(QStringLiteral("paru")).isEmpty()) {
        return PacmanWrapper::Paru;
    }
    return PacmanWrapper::Pacman;
}

bool isPkgInstalled(const QString &pkg)
{
    // `pacman -Q` reads the same local database libalpm uses.
    return runProcess(QStringLiteral("pacman"), {QStringLiteral("-Qq"), pkg}) == 0;
}

bool isKwinWayland()
{
    return runProcess(QStringLiteral("pgrep"), {QStringLiteral("kwin_wayland")}) == 0;
}

bool isIntelAmdGpu(const QString &vendorId, const QString &classId)
{
    static const QStringList gpuClassIds = {QStringLiteral("0300")};
    static const QStringList vendorIds = {QStringLiteral("8086"), QStringLiteral("1002")};
    return gpuClassIds.contains(classId) && vendorIds.contains(vendorId);
}

QString readTextFile(const QString &path)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return {};
    }
    return QString::fromUtf8(f.readAll());
}

bool hasIntelOrAmdGpu()
{
    const QDir pci(QStringLiteral("/sys/bus/pci/devices"));
    const auto devices = pci.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
    for (const QString &dev : devices) {
        QString vendor = readTextFile(pci.filePath(dev + QStringLiteral("/vendor"))).trimmed();
        QString cls = readTextFile(pci.filePath(dev + QStringLiteral("/class"))).trimmed();
        vendor.remove(QStringLiteral("0x"));
        cls.remove(QStringLiteral("0x"));
        // PCI class is 0xCCSSPP: the first 4 hex digits are class+subclass (e.g. 0300 = VGA)
        if (isIntelAmdGpu(vendor.toLower(), cls.left(4).toLower())) {
            return true;
        }
    }
    return false;
}

} // namespace utils

namespace utils {

QString scriptsDir()
{
    const QString installed = QString::fromLatin1(ext::kScriptsDir);
    if (QFile::exists(installed + QStringLiteral("/terminal-helper"))) {
        return installed;
    }
#ifdef CLAUDEMODS_SOURCE_DIR
    const QString dev = QStringLiteral(CLAUDEMODS_SOURCE_DIR "/scripts");
    if (QFile::exists(dev + QStringLiteral("/terminal-helper"))) {
        return dev; // running from the build tree without `cmake --install`
    }
#endif
    return installed;
}

bool runInTerminal(const QString &command, bool escalate)
{
    const QString dir = scriptsDir();
    QStringList args = {dir + QStringLiteral("/terminal-helper")};
    if (escalate) {
        args << QStringLiteral("-s") << QStringLiteral("pkexec bash %1/rootshell.sh").arg(dir);
    }
    args << command + QStringLiteral("; read -p 'Press enter to exit'");
    return runProcess(QStringLiteral("bash"), args) == 0;
}

} // namespace utils

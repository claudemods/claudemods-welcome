#include "tweak.h"

#include "externals.h"

#include <QDir>
#include <QFile>

namespace tweak {

QList<Name> all()
{
    return {Name::Psd, Name::Oomd, Name::Bpftune, Name::Bluetooth, Name::Ananicy, Name::ClaudeUpdate};
}

Details details(Name t)
{
    switch (t) {
    case Name::Psd:
        return {QStringLiteral("user_service"), QStringLiteral("psd.service"), QStringLiteral("profile-sync-daemon")};
    case Name::Oomd:
        return {QStringLiteral("service"), QStringLiteral("systemd-oomd.service"), QString()};
    case Name::Bpftune:
        return {QStringLiteral("service"), QStringLiteral("bpftune.service"), QStringLiteral("bpftune-git")};
    case Name::Bluetooth:
        return {QStringLiteral("service"), QStringLiteral("bluetooth.service"), QStringLiteral("bluez")};
    case Name::Ananicy:
        return {QStringLiteral("service"), QStringLiteral("ananicy-cpp.service"), QStringLiteral("ananicy-cpp")};
    case Name::ClaudeUpdate:
        return {QStringLiteral("user_service"), QStringLiteral("arch-update.timer"), QString::fromLatin1(ext::kUpdateNotifierPkg)};
    }
    return {};
}

QString id(Name t)
{
    switch (t) {
    case Name::Psd: return QStringLiteral("psd");
    case Name::Oomd: return QStringLiteral("oomd");
    case Name::Bpftune: return QStringLiteral("bpftune");
    case Name::Bluetooth: return QStringLiteral("bluetooth");
    case Name::Ananicy: return QStringLiteral("ananicy");
    case Name::ClaudeUpdate: return QStringLiteral("claudemods-update");
    }
    return {};
}

bool fromId(const QString &s, Name *out)
{
    for (Name t : all()) {
        if (id(t) == s) {
            *out = t;
            return true;
        }
    }
    return false;
}

QString label(Name t)
{
    switch (t) {
    case Name::Psd: return QStringLiteral("Profile-sync-daemon");
    case Name::Oomd: return QStringLiteral("Systemd-oomd");
    case Name::Bpftune: return QStringLiteral("Bpftune");
    case Name::Bluetooth: return QStringLiteral("Bluetooth");
    case Name::Ananicy: return QStringLiteral("Ananicy Cpp");
    case Name::ClaudeUpdate: return QStringLiteral("Cachy Update");
    }
    return {};
}

QString tooltipId(Name t)
{
    switch (t) {
    case Name::Psd: return QStringLiteral("tweak-psd-tooltip");
    case Name::Oomd: return QStringLiteral("tweak-oomd-tooltip");
    case Name::Bpftune: return QStringLiteral("tweak-bpftune-tooltip");
    case Name::Bluetooth: return QStringLiteral("tweak-bluetooth-tooltip");
    case Name::Ananicy: return QStringLiteral("tweak-ananicycpp-tooltip");
    case Name::ClaudeUpdate: return QStringLiteral("tweak-claudemodsupdate-tooltip");
    }
    return {};
}

bool shouldUninstallOnDisable(Name t)
{
    return t != Name::Bluetooth && t != Name::Ananicy;
}

QStringList autostartFiles(Name t)
{
    if (t == Name::ClaudeUpdate) {
        return {QStringLiteral("arch-update-tray.desktop")};
    }
    return {};
}

QString autostartDir()
{
    const QByteArray xdg = qgetenv("XDG_CONFIG_HOME");
    const QString base = xdg.isEmpty() ? QDir::homePath() + QStringLiteral("/.config") : QString::fromLocal8Bit(xdg);
    return base + QStringLiteral("/autostart");
}

bool checkAutostartActive(Name t)
{
    const QDir dir(autostartDir());
    for (const QString &f : autostartFiles(t)) {
        if (dir.exists(f)) {
            return true;
        }
    }
    return false;
}

void removeAutostartFiles(Name t)
{
    const QDir dir(autostartDir());
    for (const QString &f : autostartFiles(t)) {
        QFile::remove(dir.filePath(f));
    }
}

bool isGloballyEnabled(const QString &units)
{
    const QDir global(QStringLiteral("/etc/systemd/user"));
    static const QStringList targetDirs = {
        QStringLiteral("default.target.wants"),
        QStringLiteral("timers.target.wants"),
        QStringLiteral("sockets.target.wants"),
        QStringLiteral("graphical-session.target.wants"),
    };
    const QStringList unitList = units.split(QLatin1Char(' '), Qt::SkipEmptyParts);
    for (const QString &unit : unitList) {
        for (const QString &d : targetDirs) {
            if (QFile::exists(global.filePath(d + QLatin1Char('/') + unit))) {
                return true;
            }
        }
    }
    return false;
}

} // namespace tweak

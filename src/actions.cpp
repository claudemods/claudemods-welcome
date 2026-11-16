#include "actions.h"

#include "externals.h"
#include "fluent.h"
#include "networkmanager.h"
#include "systemd.h"

#include <QDBusConnection>
#include <QDBusMessage>
#include <QDebug>
#include <QFile>
#include <QProcess>
#include <QRegularExpression>
#include <QTemporaryFile>
#include <QThread>

namespace actions {

namespace {

QStringList splitAddrs(const QString &addrs)
{
    return addrs.split(QLatin1Char(','), Qt::SkipEmptyParts);
}

void sendDnsResult(const MessageSink &sink, const QString &conn, nm::ApplyStatus status, const QString &error,
                   bool reset)
{
    switch (status) {
    case nm::ApplyStatus::Applied:
        sink(MessageType::Info, fl(reset ? QStringLiteral("dns-server-reset") : QStringLiteral("dns-server-changed")));
        break;
    case nm::ApplyStatus::PendingReconnect:
        sink(MessageType::Warning, fl(QStringLiteral("dns-server-pending")));
        break;
    case nm::ApplyStatus::Failed:
        qWarning() << "DNS operation failed for connection" << conn << ":" << error;
        sink(MessageType::Error,
             fl(reset ? QStringLiteral("dns-server-reset-failed") : QStringLiteral("dns-server-failed")));
        break;
    }
}

QStringList userGroups()
{
    static const QRegularExpression ws(QStringLiteral("\\s+"));
    return utils::cmdOutput(QStringLiteral("groups"), {}).split(ws, Qt::SkipEmptyParts);
}

} // namespace

QString messageTypeName(MessageType t)
{
    switch (t) {
    case MessageType::Info: return QStringLiteral("INFO");
    case MessageType::Warning: return QStringLiteral("WARNING");
    case MessageType::Error: return QStringLiteral("ERROR");
    }
    return {};
}

void removeDbLock(const MessageSink &sink)
{
    const QString lock = QStringLiteral("/var/lib/pacman/db.lck");
    if (QFile::exists(lock)) {
        utils::pkexecCmd({QStringLiteral("rm"), lock});
        if (!QFile::exists(lock)) {
            sink(MessageType::Info, fl(QStringLiteral("removed-db-lock")));
        }
    } else {
        sink(MessageType::Info, fl(QStringLiteral("lock-doesnt-exist")));
    }
}

void updateSystem(const RunCmdCallback &run)
{
    if (utils::pacmanWrapper() == utils::PacmanWrapper::Aura) {
        run(QStringLiteral("aura -Syu && aura -Akaxu"), false);
    } else {
        run(QStringLiteral("pacman -Syu"), true);
    }
}

void clearPkgCache(const RunCmdCallback &run)
{
    switch (utils::pacmanWrapper()) {
    case utils::PacmanWrapper::Pak: run(QStringLiteral("pak -Sc"), false); break;
    case utils::PacmanWrapper::Yay: run(QStringLiteral("yay -Sc"), false); break;
    case utils::PacmanWrapper::Paru: run(QStringLiteral("paru -Sc"), false); break;
    default: run(QStringLiteral("pacman -Sc"), true); break;
    }
}

void reinstallPackages(const RunCmdCallback &run)
{
    run(QStringLiteral("pacman -S $(pacman -Qnq)"), true);
}

void removeOrphans(const RunCmdCallback &run, const MessageSink &sink)
{
    QString orphans = utils::cmdOutput(QStringLiteral("pacman"), {QStringLiteral("-Qtdq")});
    orphans.replace(QLatin1Char('\n'), QLatin1Char(' '));
    if (orphans.trimmed().isEmpty()) {
        sink(MessageType::Info, fl(QStringLiteral("orphans-not-found")));
        return;
    }
    run(QStringLiteral("pacman -Rns %1").arg(orphans), true);
}

void resetKeyring(const RunCmdCallback &run)
{
    const QString key = QString::fromLatin1(ext::kKeyringKeyId);
    const QString cmd = QStringLiteral(
                            "rm -rf /etc/pacman.d/gnupg/ && "
                            "pacman-key --init && pacman-key --populate && "
                            "pacman-key --recv-keys %1 --keyserver keyserver.ubuntu.com && "
                            "pacman-key --lsign-key %1")
                            .arg(key);
    run(cmd, true);
}

void rankMirrors(const RunCmdCallback &run)
{
    run(QString::fromLatin1(ext::kRateMirrorsBin), true);
}

void installNeededPackages(const RunCmdCallback &run, const QStringList &packages, const QString &alreadyInstalledMsg,
                           const MessageSink &sink)
{
    QStringList toInstall;
    for (const QString &p : packages) {
        if (!utils::isPkgInstalled(p)) {
            toInstall << p;
        }
    }
    if (toInstall.isEmpty()) {
        sink(MessageType::Info, alreadyInstalledMsg);
        return;
    }
    run(QStringLiteral("pacman -S %1").arg(toInstall.join(QLatin1Char(' '))), true);
}

void installGaming(const RunCmdCallback &run, const MessageSink &sink)
{
    installNeededPackages(run, {QString::fromLatin1(ext::kGamingMetaPkg), QString::fromLatin1(ext::kGamingAppsPkg)},
                          fl(QStringLiteral("gaming-package-installed")), sink);
}

void installVramManagement(const RunCmdCallback &run, const MessageSink &sink)
{
    QStringList packages = {QStringLiteral("dmemcg-booster")};
    if (utils::isKwinWayland()) {
        packages << QStringLiteral("plasma-foreground-booster");
    }
    installNeededPackages(run, packages, fl(QStringLiteral("vram-management-package-installed")), sink);
}

void installWinboat(const RunCmdCallback &run, const MessageSink &sink)
{
    installNeededPackages(run, {QStringLiteral("winboat"), QStringLiteral("docker"), QStringLiteral("docker-compose")},
                          fl(QStringLiteral("winboat-package-installed")), sink);

    const QString dockerTarget = QStringLiteral("docker.socket");
    const bool dockerEnabled = systemd::checkSystemUnits(dockerTarget);
    if (utils::isPkgInstalled(QStringLiteral("docker")) && !dockerEnabled) {
        if (!systemd::enable({dockerTarget}, systemd::Scope::System, false)) {
            sink(MessageType::Error, fl(QStringLiteral("winboat-install-failed")));
        }
        systemd::refreshSystemCache();
    }

    const bool groupAdded = userGroups().contains(QStringLiteral("docker"));
    const QString user = qEnvironmentVariable("USER");
    if (utils::isPkgInstalled(QStringLiteral("docker")) && !groupAdded && !user.isEmpty()) {
        const bool ok = utils::pkexecCmd({QStringLiteral("usermod"), QStringLiteral("-aG"), QStringLiteral("docker"), user});
        if (!ok) {
            sink(MessageType::Error, fl(QStringLiteral("winboat-install-failed")));
        }
    }
}

void launchKwinDebugWindow()
{
    QDBusMessage msg = QDBusMessage::createMethodCall(QStringLiteral("org.kde.KWin"), QStringLiteral("/KWin"),
                                                      QStringLiteral("org.kde.KWin"), QStringLiteral("showDebugConsole"));
    const QDBusMessage reply = QDBusConnection::sessionBus().call(msg, QDBus::Block, 5000);
    if (reply.type() == QDBusMessage::ErrorMessage) {
        qWarning() << "Failed to launch kwin debug window:" << reply.errorMessage();
    }
}

void launchChrisTitusTool(const RunCmdCallback &run)
{
    const QString cmd = QStringLiteral("curl -fsSL %1 | sh").arg(QString::fromLatin1(ext::kChrisTitusUrl));
    if (utils::runInKonsole(cmd)) {
        return; // it is a TUI: no "press enter" prompt, Konsole closes when the tool exits
    }
    qWarning() << "konsole not found, using the default terminal helper";
    run(cmd, false);
}

bool toggleTweak(tweak::Name t, bool enable, const RunCmdCallback &run)
{
    const tweak::Details d = tweak::details(t);
    const bool userService = d.type == QLatin1String("user_service");
    const systemd::Scope scope = userService ? systemd::Scope::User : systemd::Scope::System;
    const QStringList units = d.units.split(QLatin1Char(' '), Qt::SkipEmptyParts);

    if (enable) {
        if (!d.pkg.isEmpty() && !utils::isPkgInstalled(d.pkg) &&
            !run(QStringLiteral("pacman -S %1").arg(d.pkg), true)) {
            return false;
        }
        systemd::enable(units, scope, true);
        if (!tweak::autostartFiles(t).isEmpty() && !utils::findExecutable(QStringLiteral("arch-update")).isEmpty()) {
            utils::runCmd(QStringLiteral("arch-update --tray"), false, false);
        }
    } else {
        if (!tweak::autostartFiles(t).isEmpty()) {
            utils::runCmd(QStringLiteral("killall arch-update-tray"), false, false);
        }
        systemd::disable(units, scope);
        if (userService && tweak::isGloballyEnabled(d.units)) {
            // try to disable explicitly for all users
            QStringList args = {QStringLiteral("systemctl"), QStringLiteral("--global"), QStringLiteral("disable")};
            args << units;
            utils::pkexecCmd(args);
        }
        if (userService) {
            tweak::removeAutostartFiles(t);
        }
        if (!d.pkg.isEmpty() && tweak::shouldUninstallOnDisable(t) && utils::isPkgInstalled(d.pkg)) {
            run(QStringLiteral("pacman -Rsn %1").arg(d.pkg), true);
        }
    }

    if (userService) {
        systemd::refreshUserCache();
    } else {
        systemd::refreshSystemCache();
    }
    return true;
}

void changeDnsServer(const QString &conn, const QString &ipv4, const QString &ipv6, bool enableDot,
                     const QString &dotHostname, const MessageSink &sink)
{
    // With DoT, NetworkManager expects "address#servername" for SNI
    auto withSni = [&](const QString &addr) {
        return (enableDot && !dotHostname.isEmpty()) ? dns::appendDotHostname(addr, dotHostname) : addr;
    };

    nm::DnsMods mods;
    mods.ipv4Dns = splitAddrs(withSni(ipv4));
    mods.ipv6Dns = splitAddrs(withSni(ipv6));
    mods.ipv4DnsPriority = -1;
    mods.ipv6DnsPriority = -1;
    mods.dnsOverTls = enableDot ? nm::DnsOverTls::Yes : nm::DnsOverTls::No;

    QString err;
    const auto status = nm::modifyConnectionDns(conn, mods, &err);
    sendDnsResult(sink, conn, status, err, false);
}

void resetDnsServer(const QString &conn, const MessageSink &sink)
{
    stopBlocky(); // blocky may have been running (DoH/DoQ mode)

    nm::DnsMods mods;
    mods.ipv4Dns = QStringList();
    mods.ipv6Dns = QStringList();
    mods.ipv4DnsPriority = 0;
    mods.ipv6DnsPriority = 0;
    mods.ipv4IgnoreAutoDns = false;
    mods.ipv6IgnoreAutoDns = false;
    mods.dnsOverTls = nm::DnsOverTls::Default;

    QString err;
    const auto status = nm::modifyConnectionDns(conn, mods, &err);
    sendDnsResult(sink, conn, status, err, true);
}

void changeDnsServerBlocky(const RunCmdCallback &run, const QString &conn, dns::BlockyMode mode,
                           const QString &upstream, const QString &bootstrapIpv4, const QString &bootstrapIpv6,
                           const QString &dotHostname, const MessageSink &sink)
{
    const QString modeName = mode == dns::BlockyMode::Doh ? QStringLiteral("DoH") : QStringLiteral("DoQ");

    // 1. install blocky if missing
    if (!utils::isPkgInstalled(QStringLiteral("blocky"))) {
        installNeededPackages(run, {QStringLiteral("blocky")},
                              fl(QStringLiteral("blocky-install-failed"), {{QStringLiteral("mode"), modeName}}), sink);
        if (!utils::isPkgInstalled(QStringLiteral("blocky"))) {
            return;
        }
    }

    // 2. write the blocky config (needs root, so go through `install` under pkexec)
    const QString config = dns::generateBlockyConfig(upstream, bootstrapIpv4, bootstrapIpv6, dotHostname);
    QTemporaryFile tmp;
    bool written = tmp.open() && tmp.write(config.toUtf8()) >= 0 && tmp.flush();
    if (written) {
        written = utils::pkexecCmd({QStringLiteral("install"), QStringLiteral("-Dm644"), tmp.fileName(), dns::kBlockyConfigPath});
    }
    if (!written) {
        qWarning() << "Failed to write blocky config";
        sink(MessageType::Error, fl(QStringLiteral("dns-server-failed")));
        return;
    }

    // 3. point NetworkManager to blocky, then (re)start it. ignore-auto-dns makes sure DHCP DNS
    // cannot bypass the encrypted proxy.
    QString error;
    if (!systemd::enable({dns::kBlockyService}, systemd::Scope::System, false, &error)) {
        sendDnsResult(sink, conn, nm::ApplyStatus::Failed, error, false);
        return;
    }
    nm::DnsMods mods;
    mods.ipv4Dns = QStringList{QStringLiteral("127.0.0.1")};
    mods.ipv6Dns = QStringList{QStringLiteral("::1")};
    mods.ipv4IgnoreAutoDns = true;
    mods.ipv6IgnoreAutoDns = true;
    mods.dnsOverTls = nm::DnsOverTls::No;
    const auto status = nm::modifyConnectionDns(conn, mods, &error);
    QThread::sleep(1);
    if (status != nm::ApplyStatus::Failed && !systemd::restart(dns::kBlockyService, systemd::Scope::System, &error)) {
        sendDnsResult(sink, conn, nm::ApplyStatus::Failed, error, false);
        return;
    }
    sendDnsResult(sink, conn, status, error, false);
}

void stopBlocky()
{
    systemd::stop(dns::kBlockyService, systemd::Scope::System);
    systemd::disable({dns::kBlockyService}, systemd::Scope::System);
}

bool isBlockyActive()
{
    return systemd::isActive(dns::kBlockyService, systemd::Scope::System);
}

} // namespace actions

#include "cli.h"

#include "actions.h"
#include "dnsdata.h"
#include "externals.h"
#include "networkmanager.h"
#include "systemd.h"
#include "tweak.h"
#include "utils.h"

#include <QCoreApplication>
#include <QTextStream>

#include <optional>

namespace cli {

namespace {

QTextStream out(stdout);
QTextStream err(stderr);

QString bold(const QString &s) { return QStringLiteral("\x1b[1m") + s + QStringLiteral("\x1b[0m"); }
QString cyan(const QString &s) { return QStringLiteral("\x1b[36m") + s + QStringLiteral("\x1b[0m"); }
QString green(const QString &s) { return QStringLiteral("\x1b[32m") + s + QStringLiteral("\x1b[0m"); }
QString red(const QString &s) { return QStringLiteral("\x1b[31m") + s + QStringLiteral("\x1b[0m"); }
QString yellow(const QString &s) { return QStringLiteral("\x1b[33m") + s + QStringLiteral("\x1b[0m"); }

const utils::RunCmdCallback kRun = [](const QString &command, bool escalate) {
    return utils::runCmd(command, escalate, true);
};

const actions::MessageSink kSink = [](actions::MessageType type, const QString &message) {
    const QString name = actions::messageTypeName(type);
    out << QStringLiteral("[%1] %2: %3\n").arg(name, name, message);
    out.flush();
};

const QList<std::pair<QString, QString>> kServerNames = {
    {QStringLiteral("ad-guard"), QStringLiteral("AdGuard")},
    {QStringLiteral("ad-guard-family"), QStringLiteral("AdGuard Family Protection")},
    {QStringLiteral("cloudflare"), QStringLiteral("Cloudflare")},
    {QStringLiteral("cloudflare-malware"), QStringLiteral("Cloudflare Malware blocking")},
    {QStringLiteral("cloudflare-malware-adult"), QStringLiteral("Cloudflare Malware and adult content blocking")},
    {QStringLiteral("open-dns"), QStringLiteral("Cisco Umbrella(OpenDNS)")},
    {QStringLiteral("dns-watch"), QStringLiteral("DNS.Watch")},
    {QStringLiteral("f-fmuc"), QStringLiteral("FFMUC DNS / Freie Netze Muenchen e.V.")},
    {QStringLiteral("g-core"), QStringLiteral("GCore")},
    {QStringLiteral("google"), QStringLiteral("Google")},
    {QStringLiteral("quad9"), QStringLiteral("Quad9")},
    {QStringLiteral("yandex"), QStringLiteral("Yandex")},
    {QStringLiteral("yandex-malware"), QStringLiteral("Yandex Malware blocking")},
    {QStringLiteral("yandex-malware-adult"), QStringLiteral("Yandex Malware and adult content blocking")},
    {QStringLiteral("ali-dns"), QStringLiteral("阿里云公共DNS (AliDNS)")},
    {QStringLiteral("tencent"), QStringLiteral("腾讯云 DNSPod (Tencent)")},
};

void usage()
{
    const QString bin = QString::fromLatin1(ext::kBinaryName);
    out << "ClaudeMods Hello " << APP_VERSION << "\n\n"
        << "Usage: " << bin << " [COMMAND]\n"
        << "       (no command starts the graphical welcome screen)\n\n"
        << "Commands:\n"
        << "  fix <ACTION>      update-system | reinstall-packages | reset-keyrings | remove-lock |\n"
        << "                    clear-cache | remove-orphans | rank-mirrors | install-gaming |\n"
        << "                    show-kwin-debug | install-winboat | install-vram-management | christitus\n"
        << "  tweak <ACTION>    enable <NAME> | disable <NAME> | list\n"
        << "                    NAME: psd | oomd | bpftune | bluetooth | ananicy | claudemods-update\n"
        << "  dns <ACTION>      set -c <CONNECTION> -s <SERVER> [--dot|--doh|--doq]\n"
        << "                    set-custom -c <CONNECTION> [--ipv4 A,B] [--ipv6 A,B] [--dot --dot-hostname H]\n"
        << "                               [--doh --doh-url URL] [--doq --doq-endpoint quic:HOST]\n"
        << "                    reset -c <CONNECTION> | list-connections | list-servers | test-latency\n"
        << "  launch <APP>      package-installer | kernel-manager\n"
        << "  -h, --help        show this help\n"
        << "  -V, --version     show the version\n";
    out.flush();
}

/// Removes `--name VALUE` / `-n VALUE` / `--name=VALUE` from args and returns the value.
std::optional<QString> takeValue(QStringList &args, const QString &longName, const QString &shortName = QString())
{
    for (int i = 0; i < args.size(); ++i) {
        const QString &a = args.at(i);
        if (a == longName || (!shortName.isEmpty() && a == shortName)) {
            if (i + 1 >= args.size()) {
                return QString();
            }
            const QString v = args.at(i + 1);
            args.removeAt(i + 1);
            args.removeAt(i);
            return v;
        }
        if (a.startsWith(longName + QLatin1Char('='))) {
            const QString v = a.mid(longName.size() + 1);
            args.removeAt(i);
            return v;
        }
    }
    return std::nullopt;
}

bool takeFlag(QStringList &args, const QString &name)
{
    return args.removeAll(name) > 0;
}

int handleFix(const QString &action)
{
    actions::MessageSink sink = kSink;
    if (action == QLatin1String("update-system")) {
        out << bold(QStringLiteral("Updating system...")) << "\n";
        out.flush();
        actions::updateSystem(kRun);
    } else if (action == QLatin1String("reinstall-packages")) {
        out << bold(QStringLiteral("Reinstalling all native packages...")) << "\n";
        out.flush();
        actions::reinstallPackages(kRun);
    } else if (action == QLatin1String("reset-keyrings")) {
        out << bold(QStringLiteral("Resetting pacman keyrings...")) << "\n";
        out.flush();
        actions::resetKeyring(kRun);
    } else if (action == QLatin1String("remove-lock")) {
        out << bold(QStringLiteral("Removing pacman database lock...")) << "\n";
        out.flush();
        actions::removeDbLock(sink);
    } else if (action == QLatin1String("clear-cache")) {
        out << bold(QStringLiteral("Clearing package cache...")) << "\n";
        out.flush();
        actions::clearPkgCache(kRun);
    } else if (action == QLatin1String("remove-orphans")) {
        out << bold(QStringLiteral("Removing orphan packages...")) << "\n";
        out.flush();
        actions::removeOrphans(kRun, sink);
    } else if (action == QLatin1String("rank-mirrors")) {
        out << bold(QStringLiteral("Ranking mirrors...")) << "\n";
        out.flush();
        actions::rankMirrors(kRun);
    } else if (action == QLatin1String("install-gaming")) {
        out << bold(QStringLiteral("Installing ClaudeMods gaming packages...")) << "\n";
        out.flush();
        actions::installGaming(kRun, sink);
    } else if (action == QLatin1String("show-kwin-debug")) {
        out << bold(QStringLiteral("Attempting to launch KWin debug console...")) << "\n";
        out.flush();
        actions::launchKwinDebugWindow();
    } else if (action == QLatin1String("christitus")) {
        out << bold(QStringLiteral("Launching Chris Titus Tech's Linux tool...")) << "\n";
        out.flush();
        actions::launchChrisTitusTool(kRun);
    } else if (action == QLatin1String("install-winboat")) {
        out << bold(QStringLiteral("Installing Winboat...")) << "\n";
        out.flush();
        actions::installWinboat(kRun, sink);
    } else if (action == QLatin1String("install-vram-management")) {
        out << bold(QStringLiteral("Installing VRAM management packages...")) << "\n";
        out.flush();
        actions::installVramManagement(kRun, sink);
    } else {
        err << "Error: unknown fix action '" << action << "'\n";
        err.flush();
        usage();
        return 2;
    }
    return 0;
}

int toggleTweakCli(const QString &name, bool enable)
{
    tweak::Name t;
    if (!tweak::fromId(name, &t)) {
        err << "Error: unknown tweak '" << name << "'\n";
        err.flush();
        return 2;
    }
    out << (enable ? "Enabling" : "Disabling") << " tweak '" << name << "'...\n";
    out.flush();
    if (!actions::toggleTweak(t, enable, kRun)) {
        err << "Error: Failed to toggle tweak '" << name << "'\n";
        err.flush();
        return 1;
    }
    out << "Tweak '" << name << "' successfully " << (enable ? green(QStringLiteral("enabled")) : red(QStringLiteral("disabled")))
        << ".\n";
    return 0;
}

int listTweaks()
{
    out << bold(QStringLiteral("Available Tweaks Status:")) << "\n";
    systemd::refreshCache();
    for (tweak::Name t : tweak::all()) {
        const bool enabled = systemd::checkAnyUnits(tweak::details(t).units);
        out << "- " << tweak::id(t).leftJustified(18) << " "
            << (enabled ? green(QStringLiteral("[enabled]")) : red(QStringLiteral("[disabled]"))) << "\n";
    }
    return 0;
}

int handleTweak(QStringList args)
{
    if (args.isEmpty()) {
        usage();
        return 2;
    }
    const QString action = args.takeFirst();
    if (action == QLatin1String("list")) {
        return listTweaks();
    }
    if ((action == QLatin1String("enable") || action == QLatin1String("disable")) && !args.isEmpty()) {
        return toggleTweakCli(args.first(), action == QLatin1String("enable"));
    }
    usage();
    return 2;
}

QString blockyLabel(dns::BlockyMode mode)
{
    return mode == dns::BlockyMode::Doh ? QStringLiteral("DoH") : QStringLiteral("DoQ");
}

int handleDns(QStringList args)
{
    if (args.isEmpty()) {
        usage();
        return 2;
    }
    const QString action = args.takeFirst();
    const auto &servers = dns::servers();

    if (action == QLatin1String("set")) {
        const QString connection = takeValue(args, QStringLiteral("--connection"), QStringLiteral("-c")).value_or(QString());
        const QString serverArg = takeValue(args, QStringLiteral("--server"), QStringLiteral("-s")).value_or(QString());
        const bool dot = takeFlag(args, QStringLiteral("--dot"));
        const bool doh = takeFlag(args, QStringLiteral("--doh"));
        const bool doq = takeFlag(args, QStringLiteral("--doq"));
        if ((int(dot) + int(doh) + int(doq)) > 1) {
            err << "Error: --dot, --doh and --doq cannot be combined.\n";
            err.flush();
            return 2;
        }
        QString serverName;
        for (const auto &p : kServerNames) {
            if (p.first == serverArg) {
                serverName = p.second;
            }
        }
        const int idx = dns::serverIndex(serverName);
        if (connection.isEmpty() || idx < 0) {
            err << "Error: --connection and a valid --server are required (see 'dns list-servers').\n";
            err.flush();
            return 2;
        }
        const dns::Server &server = servers.at(idx);

        if (doh || doq) {
            const dns::BlockyMode mode = doh ? dns::BlockyMode::Doh : dns::BlockyMode::Doq;
            const QString upstream = dns::blockyUpstream(server, mode);
            if (!upstream.isEmpty()) {
                out << "Setting DNS for '" << cyan(connection) << "' to '" << cyan(server.name) << "' ("
                    << blockyLabel(mode) << " enabled via blocky)...\n";
                out.flush();
                actions::changeDnsServerBlocky(kRun, connection, mode, upstream, server.ipv4, server.ipv6, server.dotHost, kSink);
            } else {
                out << yellow(QStringLiteral("Warning")) << ": DNS over " << blockyLabel(mode) << " is not supported by '"
                    << server.name << "'.\nSetting DNS without " << blockyLabel(mode) << "...\n";
                out.flush();
                actions::changeDnsServer(connection, server.ipv4, server.ipv6, false, server.dotHost, kSink);
            }
        } else {
            actions::stopBlocky();
            const bool dotSupported = !server.dotHost.isEmpty();
            if (dot && !dotSupported) {
                out << yellow(QStringLiteral("Warning")) << ": DNS over TLS is not supported by '" << server.name
                    << "'.\nSetting DNS without DoT...\n";
            }
            const bool enableDot = dot && dotSupported;
            out << "Setting DNS for '" << cyan(connection) << "' to '" << cyan(server.name) << "'"
                << (enableDot ? " (DoT enabled)" : "") << "...\n";
            out.flush();
            actions::changeDnsServer(connection, server.ipv4, server.ipv6, enableDot, server.dotHost, kSink);
        }
        return 0;
    }

    if (action == QLatin1String("set-custom")) {
        const QString connection = takeValue(args, QStringLiteral("--connection"), QStringLiteral("-c")).value_or(QString());
        const QString ipv4 = takeValue(args, QStringLiteral("--ipv4")).value_or(QString());
        const QString ipv6 = takeValue(args, QStringLiteral("--ipv6")).value_or(QString());
        const QString dotHostname = takeValue(args, QStringLiteral("--dot-hostname")).value_or(QString());
        const QString dohUrl = takeValue(args, QStringLiteral("--doh-url")).value_or(QString());
        const QString doqEndpoint = takeValue(args, QStringLiteral("--doq-endpoint")).value_or(QString());
        const bool dot = takeFlag(args, QStringLiteral("--dot"));
        const bool doh = takeFlag(args, QStringLiteral("--doh"));
        const bool doq = takeFlag(args, QStringLiteral("--doq"));
        if ((int(dot) + int(doh) + int(doq)) > 1) {
            err << "Error: --dot, --doh and --doq cannot be combined.\n";
            err.flush();
            return 2;
        }
        if (connection.isEmpty()) {
            err << red(QStringLiteral("Error")) << ": --connection is required.\n";
            err.flush();
            return 2;
        }
        if (ipv4.isEmpty() && ipv6.isEmpty()) {
            err << red(QStringLiteral("Error")) << ": At least one of --ipv4 or --ipv6 must be provided.\n";
            err.flush();
            return 1;
        }
        if (!dotHostname.isEmpty() && !dns::isValidDotHostname(dotHostname)) {
            err << red(QStringLiteral("Error")) << ": Invalid DoT hostname '" << dotHostname << "'.\n";
            err.flush();
            return 1;
        }
        if (doh) {
            if (!dns::isValidDohUrl(dohUrl)) {
                err << red(QStringLiteral("Error")) << ": --doh-url must be a valid https:// URL.\n";
                err.flush();
                return 1;
            }
            out << "Setting custom DoH DNS for '" << cyan(connection) << "': upstream='" << cyan(dohUrl) << "'...\n";
            out.flush();
            actions::changeDnsServerBlocky(kRun, connection, dns::BlockyMode::Doh, dohUrl, ipv4, ipv6, dotHostname, kSink);
        } else if (doq) {
            if (!dns::isValidDoqEndpoint(doqEndpoint)) {
                err << red(QStringLiteral("Error")) << ": --doq-endpoint must start with quic: or quic://.\n";
                err.flush();
                return 1;
            }
            out << "Setting custom DoQ DNS for '" << cyan(connection) << "': upstream='" << cyan(doqEndpoint) << "'...\n";
            out.flush();
            actions::changeDnsServerBlocky(kRun, connection, dns::BlockyMode::Doq, doqEndpoint, ipv4, ipv6, dotHostname, kSink);
        } else {
            actions::stopBlocky();
            out << "Setting custom DNS for '" << cyan(connection) << "': IPv4='" << (ipv4.isEmpty() ? QStringLiteral("(none)") : ipv4)
                << "' IPv6='" << (ipv6.isEmpty() ? QStringLiteral("(none)") : ipv6) << "'"
                << (dotHostname.isEmpty() ? QString() : QStringLiteral(" hostname=") + dotHostname)
                << (dot ? " (DoT enabled)" : "") << "\n";
            out.flush();
            actions::changeDnsServer(connection, ipv4, ipv6, dot, dotHostname, kSink);
        }
        return 0;
    }

    if (action == QLatin1String("reset")) {
        const QString connection = takeValue(args, QStringLiteral("--connection"), QStringLiteral("-c")).value_or(QString());
        if (connection.isEmpty()) {
            err << "Error: --connection is required.\n";
            err.flush();
            return 2;
        }
        out << "Resetting DNS for '" << cyan(connection) << "' to automatic...\n";
        out.flush();
        actions::resetDnsServer(connection, kSink);
        return 0;
    }

    if (action == QLatin1String("list-connections")) {
        out << bold(QStringLiteral("Available Network Connections:")) << "\n";
        const QStringList conns = nm::connections();
        if (conns.isEmpty()) {
            out << "No connections found.\n";
        }
        for (const QString &c : conns) {
            out << "- " << c << "\n";
        }
        return 0;
    }

    if (action == QLatin1String("list-servers")) {
        out << bold(QStringLiteral("Available DNS Servers:")) << "\n";
        for (const dns::Server &s : servers) {
            out << "- " << s.name;
            if (!s.dotHost.isEmpty()) out << " [DoT: " << s.dotHost << "]";
            if (!s.dohUrl.isEmpty()) out << " [DoH: " << s.dohUrl << "]";
            if (!s.doqEndpoint.isEmpty()) out << " [DoQ: " << s.doqEndpoint << "]";
            out << " (" << s.region << " - " << s.homepage << ")\n";
        }
        return 0;
    }

    if (action == QLatin1String("test-latency")) {
        out << bold(QStringLiteral("Testing latency to all DNS servers...")) << "\n";
        out.flush();
        for (const auto &[idx, ms] : dns::measureAllLatencies()) {
            if (ms) {
                out << QStringLiteral("  %1 ms  %2\n").arg(double(*ms), 8, 'f', 2).arg(servers.at(idx).name);
            } else {
                out << "        --  " << servers.at(idx).name << " (timeout)\n";
            }
        }
        return 0;
    }

    usage();
    return 2;
}

int handleLaunch(const QString &app)
{
    QString appName;
    QString bin;
    if (app == QLatin1String("package-installer")) {
        appName = QStringLiteral("ClaudeMods Package Installer");
        bin = QString::fromLatin1(ext::kPackageInstallerBin);
    } else if (app == QLatin1String("kernel-manager")) {
        appName = QStringLiteral("ClaudeMods Kernel Manager");
        bin = QString::fromLatin1(ext::kKernelManagerBin);
    } else {
        usage();
        return 2;
    }
    out << "Launching " << bold(appName) << "...\n";
    out.flush();
    const QString path = utils::findExecutable(bin);
    if (path.isEmpty()) {
        err << "Error: '" << bin << "' executable not found in your PATH.\n";
        err.flush();
        return 1;
    }
    utils::spawnDetached(path);
    out << appName << " launched successfully.\n";
    return 0;
}

} // namespace

int run(const QStringList &argsIn)
{
    QStringList args = argsIn;
    args.removeFirst(); // program name
    if (args.isEmpty() || args.first() == QLatin1String("-h") || args.first() == QLatin1String("--help")) {
        usage();
        return 0;
    }
    if (args.first() == QLatin1String("-V") || args.first() == QLatin1String("--version")) {
        out << ext::kBinaryName << " " << APP_VERSION << "\n";
        return 0;
    }

    const QString command = args.takeFirst();
    int code = 2;
    if (command == QLatin1String("fix")) {
        code = args.isEmpty() ? (usage(), 2) : handleFix(args.first());
    } else if (command == QLatin1String("tweak")) {
        code = handleTweak(args);
    } else if (command == QLatin1String("dns")) {
        code = handleDns(args);
    } else if (command == QLatin1String("launch")) {
        code = args.isEmpty() ? (usage(), 2) : handleLaunch(args.first());
    } else {
        err << "Error: unknown command '" << command << "'\n";
        err.flush();
        usage();
    }
    out.flush();
    return code;
}

} // namespace cli

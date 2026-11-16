#include "networkmanager.h"

#include "utils.h"

#include <QDebug>
#include <QProcess>

namespace nm {

namespace {

const QString kNmcli = QStringLiteral("nmcli");

/// Splits an nmcli `-t` line on unescaped ':' and unescapes `\:` / `\\`.
QStringList splitTerse(const QString &line)
{
    QStringList parts;
    QString cur;
    for (int i = 0; i < line.size(); ++i) {
        const QChar c = line[i];
        if (c == QLatin1Char('\\') && i + 1 < line.size()) {
            cur += line[++i];
        } else if (c == QLatin1Char(':')) {
            parts << cur;
            cur.clear();
        } else {
            cur += c;
        }
    }
    parts << cur;
    return parts;
}

struct Result {
    int code = -1;
    QString out;
    QString err;
};

Result run(const QStringList &args, bool pkexec = false)
{
    QProcess p;
    if (pkexec) {
        p.start(QStringLiteral("pkexec"), QStringList{kNmcli} + args);
    } else {
        p.start(kNmcli, args);
    }
    Result r;
    if (!p.waitForFinished(120000)) {
        p.kill();
        return r;
    }
    r.code = p.exitStatus() == QProcess::NormalExit ? p.exitCode() : -1;
    r.out = QString::fromUtf8(p.readAllStandardOutput());
    r.err = QString::fromUtf8(p.readAllStandardError());
    return r;
}

QString joinDns(const QStringList &list)
{
    return list.join(QLatin1Char(','));
}

bool dnsAllowed(const QString &conn, const QString &family)
{
    const QString method = run({QStringLiteral("-g"), family + QStringLiteral(".method"), QStringLiteral("connection"),
                                QStringLiteral("show"), conn})
                               .out.trimmed();
    static const QStringList blocked = {QStringLiteral("disabled"), QStringLiteral("ignore"),
                                        QStringLiteral("link-local"), QStringLiteral("shared")};
    return !blocked.contains(method);
}

QString yesNo(bool v)
{
    return v ? QStringLiteral("yes") : QStringLiteral("no");
}

QString dotValue(DnsOverTls v)
{
    switch (v) {
    case DnsOverTls::Default: return QStringLiteral("default");
    case DnsOverTls::No: return QStringLiteral("no");
    case DnsOverTls::Opportunistic: return QStringLiteral("opportunistic");
    case DnsOverTls::Yes: return QStringLiteral("yes");
    }
    return QStringLiteral("default");
}

} // namespace

bool isAvailable()
{
    return !utils::findExecutable(kNmcli).isEmpty();
}

QStringList connections()
{
    const QString out = run({QStringLiteral("-t"), QStringLiteral("-f"), QStringLiteral("NAME"),
                             QStringLiteral("connection"), QStringLiteral("show")})
                            .out;
    QStringList names;
    for (const QString &line : out.split(QLatin1Char('\n'), Qt::SkipEmptyParts)) {
        names << splitTerse(line).value(0);
    }
    return names;
}

QString activeConnectionName()
{
    const QString out = run({QStringLiteral("-g"), QStringLiteral("NAME"), QStringLiteral("connection"),
                             QStringLiteral("show"), QStringLiteral("--active")})
                            .out;
    const QStringList lines = out.split(QLatin1Char('\n'), Qt::SkipEmptyParts);
    return lines.isEmpty() ? QString() : lines.first();
}

std::optional<DnsInfo> dnsForConnection(const QString &name)
{
    const QString out = run({QStringLiteral("-g"), QStringLiteral("ipv4.dns,ipv6.dns"), QStringLiteral("con"),
                             QStringLiteral("show"), name})
                            .out;
    const QStringList lines = out.split(QLatin1Char('\n'));
    const QString rawV4 = lines.value(0);
    QString rawV6 = lines.value(1);
    rawV6.replace(QStringLiteral("\\:"), QStringLiteral(":"));
    if (rawV4.isEmpty() && rawV6.isEmpty()) {
        return std::nullopt;
    }

    // "addr#hostname": every address of a connection shares the same hostname
    DnsInfo info;
    auto strip = [&info](const QString &raw) {
        QStringList addrs;
        for (const QString &addr : raw.split(QLatin1Char(','))) {
            const int pos = addr.indexOf(QLatin1Char('#'));
            if (pos >= 0) {
                if (info.dotHostname.isEmpty()) {
                    info.dotHostname = addr.mid(pos + 1);
                }
                addrs << addr.left(pos);
            } else {
                addrs << addr;
            }
        }
        return addrs.join(QLatin1Char(','));
    };
    info.ipv4 = strip(rawV4);
    info.ipv6 = strip(rawV6);
    return info;
}

bool dotForConnection(const QString &name)
{
    const QString v = run({QStringLiteral("-g"), QStringLiteral("connection.dns-over-tls"), QStringLiteral("con"),
                           QStringLiteral("show"), name})
                          .out.trimmed();
    // nmcli prints either the number or "yes"/"2 (yes)" depending on version
    return v == QLatin1String("2") || v == QLatin1String("yes") || v.startsWith(QLatin1String("2 "));
}

ApplyStatus modifyConnectionDns(const QString &name, const DnsMods &mods, QString *error)
{
    QStringList args = {QStringLiteral("connection"), QStringLiteral("modify"), name};

    auto patchFamily = [&](const QString &family, const std::optional<QStringList> &dns,
                           const std::optional<int> &priority, const std::optional<bool> &ignoreAuto) {
        if (!dnsAllowed(name, family)) {
            qDebug() << "skipping" << family << "DNS: NetworkManager disallows it for this method";
            return;
        }
        if (dns) {
            args << family + QStringLiteral(".dns") << joinDns(*dns);
        }
        if (priority) {
            args << family + QStringLiteral(".dns-priority") << QString::number(*priority);
        }
        if (ignoreAuto) {
            args << family + QStringLiteral(".ignore-auto-dns") << yesNo(*ignoreAuto);
        }
    };
    patchFamily(QStringLiteral("ipv4"), mods.ipv4Dns, mods.ipv4DnsPriority, mods.ipv4IgnoreAutoDns);
    patchFamily(QStringLiteral("ipv6"), mods.ipv6Dns, mods.ipv6DnsPriority, mods.ipv6IgnoreAutoDns);
    if (mods.dnsOverTls) {
        args << QStringLiteral("connection.dns-over-tls") << dotValue(*mods.dnsOverTls);
    }

    Result r = run(args);
    if (r.code != 0) {
        // Not authorised for the unprivileged call: retry elevated.
        r = run(args, true);
    }
    if (r.code != 0) {
        if (error) {
            *error = r.err.trimmed();
        }
        return ApplyStatus::Failed;
    }

    // Reapply so the change is live now: find the device of the active connection.
    const QString active = run({QStringLiteral("-t"), QStringLiteral("-f"), QStringLiteral("NAME,DEVICE"),
                                QStringLiteral("connection"), QStringLiteral("show"), QStringLiteral("--active")})
                               .out;
    bool applied = false;
    for (const QString &line : active.split(QLatin1Char('\n'), Qt::SkipEmptyParts)) {
        const QStringList parts = splitTerse(line);
        if (parts.size() >= 2 && parts.at(0) == name && !parts.at(1).isEmpty()) {
            const Result re = run({QStringLiteral("device"), QStringLiteral("reapply"), parts.at(1)});
            if (re.code == 0) {
                applied = true;
            } else {
                qWarning() << "Failed to reapply connection (applies on next connect):" << re.err.trimmed();
            }
        }
    }
    return applied ? ApplyStatus::Applied : ApplyStatus::PendingReconnect;
}

} // namespace nm

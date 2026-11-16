#include "dnsdata.h"

#include "utils.h"

#include <QElapsedTimer>
#include <QFile>
#include <QHostAddress>
#include <QSslSocket>
#include <QUdpSocket>
#include <QtConcurrent>

#include <algorithm>
#include <limits>

namespace dns {

namespace {

QList<Server> buildServers()
{
    auto S = [](const char *name, const char *v4, const char *v6, const char *dot, const char *doh, const char *doq,
                const char *region, const char *home, bool filtering) {
        Server s;
        s.name = QString::fromUtf8(name);
        s.ipv4 = QString::fromUtf8(v4);
        s.ipv6 = QString::fromUtf8(v6);
        s.dotHost = QString::fromUtf8(dot);
        s.dohUrl = QString::fromUtf8(doh);
        s.doqEndpoint = QString::fromUtf8(doq);
        s.region = QString::fromUtf8(region);
        s.homepage = QString::fromUtf8(home);
        s.filtering = filtering;
        return s;
    };
    return {
        S("AdGuard", "94.140.14.14,94.140.15.15", "2a10:50c0::ad1:ff,2a10:50c0::ad2:ff", "dns.adguard-dns.com",
          "https://dns.adguard-dns.com/dns-query", "quic:dns.adguard-dns.com", "EU", "https://adguard-dns.io", false),
        S("AdGuard Family Protection", "94.140.14.15,94.140.15.16", "2a10:50c0::bad1:ff,2a10:50c0::bad2:ff",
          "family.adguard-dns.com", "https://family.adguard-dns.com/dns-query", "quic:family.adguard-dns.com", "EU",
          "https://adguard-dns.io", true),
        S("Cisco Umbrella(OpenDNS)", "208.67.222.222,208.67.220.220", "2620:119:35::35,2620:119:53::53",
          "dns.opendns.com", "https://doh.opendns.com/dns-query", "", "US", "https://www.opendns.com", false),
        S("Cloudflare", "1.1.1.1,1.0.0.1", "2606:4700:4700::1111,2606:4700:4700::1001", "cloudflare-dns.com",
          "https://cloudflare-dns.com/dns-query", "", "US", "https://1.1.1.1", false),
        S("Cloudflare Malware and adult content blocking", "1.1.1.3,1.0.0.3", "2606:4700:4700::1113,2606:4700:4700::1003",
          "family.cloudflare-dns.com", "https://family.cloudflare-dns.com/dns-query", "", "US", "https://1.1.1.1/family",
          true),
        S("Cloudflare Malware blocking", "1.1.1.2,1.0.0.2", "2606:4700:4700::1112,2606:4700:4700::1002",
          "security.cloudflare-dns.com", "https://security.cloudflare-dns.com/dns-query", "", "US",
          "https://1.1.1.1/family", true),
        S("DNS.Watch", "84.200.69.80,84.200.70.40", "2001:1608:10:25::1c04:b12f,2001:1608:10:25::9249:d69b", "", "", "",
          "EU", "https://dns.watch", false),
        S("FFMUC DNS / Freie Netze Muenchen e.V.", "185.150.99.255,5.1.66.255", "2001:678:e68:f000::,2001:678:ed0:f000::",
          "dot.ffmuc.net", "https://doh.ffmuc.net/dns-query", "quic:doq.ffmuc.net", "EU", "https://dns-setup.ffmuc.net",
          false),
        S("GCore", "95.85.95.85,2.56.220.2", "2a03:90c0:999d::1,2a03:90c0:9992::1", "", "", "", "EU",
          "https://gcore.com/public-dns", false),
        S("Google", "8.8.8.8,8.8.4.4", "2001:4860:4860::8888,2001:4860:4860::8844", "dns.google",
          "https://dns.google/dns-query", "", "US", "https://dns.google", false),
        S("Quad9", "9.9.9.9,149.112.112.112", "2620:fe::fe,2620:fe::9", "dns.quad9.net",
          "https://dns.quad9.net/dns-query", "quic:dns.quad9.net", "EU", "https://www.quad9.net", false),
        S("Yandex", "77.88.8.8,77.88.8.1", "2a02:6b8::feed:0ff,2a02:6b8:0:1::feed:0ff", "common.dot.dns.yandex.net",
          "https://common.dot.dns.yandex.net/dns-query", "", "RU", "https://dns.yandex.com", false),
        S("Yandex Malware and adult content blocking", "77.88.8.7,77.88.8.3", "2a02:6b8::feed:a11,2a02:6b8:0:1::feed:a11",
          "family.dot.dns.yandex.net", "https://family.dot.dns.yandex.net/dns-query", "", "RU", "https://dns.yandex.com",
          true),
        S("Yandex Malware blocking", "77.88.8.88,77.88.8.2", "2a02:6b8::feed:bad,2a02:6b8:0:1::feed:bad",
          "safe.dot.dns.yandex.net", "https://safe.dot.dns.yandex.net/dns-query", "", "RU", "https://dns.yandex.com", true),
        S("阿里云公共DNS (AliDNS)", "223.5.5.5,223.6.6.6", "2400:3200::1,2400:3200:baba::1", "dns.alidns.com",
          "https://dns.alidns.com/dns-query", "", "CN", "https://alidns.com", false),
        S("腾讯云 DNSPod (Tencent)", "119.29.29.29,119.28.28.28", "2402:4e00::,2402:4e00:1::", "dot.pub",
          "https://doh.pub/dns-query", "", "CN", "https://www.dnspod.cn", false),
    };
}

/// DNS wire-format query for an A record.
QByteArray buildQuery(const QString &domain)
{
    QByteArray q;
    q.append(char(0x12)).append(char(0x34)); // id
    q.append(char(0x01)).append(char(0x00)); // recursion desired
    q.append(char(0x00)).append(char(0x01)); // QDCOUNT
    q.append(char(0x00)).append(char(0x00)); // ANCOUNT
    q.append(char(0x00)).append(char(0x00)); // NSCOUNT
    q.append(char(0x00)).append(char(0x00)); // ARCOUNT
    for (const QString &label : domain.split(QLatin1Char('.'), Qt::SkipEmptyParts)) {
        const QByteArray l = label.toLatin1();
        q.append(char(l.size())).append(l);
    }
    q.append(char(0x00));
    q.append(char(0x00)).append(char(0x01)); // QTYPE A
    q.append(char(0x00)).append(char(0x01)); // QCLASS IN
    return q;
}

constexpr int kTimeoutMs = 3000;
const QString kProbeName = QStringLiteral("example.com");

bool validResponse(const QByteArray &r)
{
    // id echoed + QR bit set
    return r.size() >= 12 && quint8(r[0]) == 0x12 && quint8(r[1]) == 0x34 && (quint8(r[2]) & 0x80);
}

std::optional<qint64> probePlain(const QHostAddress &ip)
{
    QUdpSocket sock;
    const QByteArray query = buildQuery(kProbeName);
    QElapsedTimer t;
    t.start();
    if (sock.writeDatagram(query, ip, 53) != query.size()) {
        return std::nullopt;
    }
    if (!sock.waitForReadyRead(kTimeoutMs)) {
        return std::nullopt;
    }
    const QByteArray resp = sock.receiveDatagram().data();
    if (!validResponse(resp)) {
        return std::nullopt;
    }
    return t.elapsed();
}

bool connectTls(QSslSocket &sock, const QHostAddress &ip, quint16 port, const QString &hostname)
{
    sock.connectToHostEncrypted(ip.toString(), port, hostname);
    return sock.waitForEncrypted(kTimeoutMs);
}

std::optional<qint64> probeDot(const QHostAddress &ip, const QString &hostname)
{
    QSslSocket sock;
    QElapsedTimer t;
    t.start();
    if (!connectTls(sock, ip, 853, hostname)) {
        return std::nullopt;
    }
    const QByteArray query = buildQuery(kProbeName);
    QByteArray framed;
    framed.append(char(query.size() >> 8)).append(char(query.size() & 0xff)).append(query);
    sock.write(framed);
    QByteArray buf;
    while (buf.size() < 2 || buf.size() < 2 + ((quint8(buf[0]) << 8) | quint8(buf[1]))) {
        if (!sock.waitForReadyRead(kTimeoutMs)) {
            return std::nullopt;
        }
        buf += sock.readAll();
    }
    if (!validResponse(buf.mid(2))) {
        return std::nullopt;
    }
    return t.elapsed();
}

std::optional<qint64> probeDoh(const QHostAddress &ip, const QString &hostname, const QString &path)
{
    QSslSocket sock;
    QElapsedTimer t;
    t.start();
    if (!connectTls(sock, ip, 443, hostname)) {
        return std::nullopt;
    }
    const QByteArray query = buildQuery(kProbeName);
    QByteArray req = "POST " + (path.isEmpty() ? QByteArray("/dns-query") : path.toUtf8()) + " HTTP/1.1\r\n";
    req += "Host: " + hostname.toUtf8() + "\r\n";
    req += "Content-Type: application/dns-message\r\nAccept: application/dns-message\r\n";
    req += "Content-Length: " + QByteArray::number(query.size()) + "\r\nConnection: close\r\n\r\n";
    req += query;
    sock.write(req);

    QByteArray buf;
    for (;;) {
        const int headerEnd = buf.indexOf("\r\n\r\n");
        if (headerEnd >= 0) {
            if (!buf.startsWith("HTTP/1.1 200") && !buf.startsWith("HTTP/2 200") && !buf.startsWith("HTTP/1.0 200")) {
                return std::nullopt;
            }
            if (buf.size() - (headerEnd + 4) >= 12) {
                break;
            }
        }
        if (!sock.waitForReadyRead(kTimeoutMs)) {
            return std::nullopt;
        }
        buf += sock.readAll();
    }
    const QByteArray body = buf.mid(buf.indexOf("\r\n\r\n") + 4);
    if (!validResponse(body)) {
        // chunked bodies start with a size line; treat any 200 with data as an answer
        if (body.size() < 12) {
            return std::nullopt;
        }
    }
    return t.elapsed();
}

QString stripQuic(QString endpoint)
{
    endpoint.remove(QStringLiteral("quic://"));
    endpoint.remove(QStringLiteral("quic:"));
    return endpoint;
}

} // namespace

const QList<Server> &servers()
{
    static const QList<Server> list = buildServers();
    return list;
}

int serverIndex(const QString &name)
{
    const auto &list = servers();
    for (int i = 0; i < list.size(); ++i) {
        if (list.at(i).name == name) {
            return i;
        }
    }
    return -1;
}

QString typeId(Type t)
{
    switch (t) {
    case Type::Plain: return QStringLiteral("plain");
    case Type::Dot: return QStringLiteral("dot");
    case Type::Doh: return QStringLiteral("doh");
    case Type::Doq: return QStringLiteral("doq");
    }
    return QStringLiteral("plain");
}

Type typeFromId(const QString &id)
{
    if (id == QLatin1String("dot")) return Type::Dot;
    if (id == QLatin1String("doh")) return Type::Doh;
    if (id == QLatin1String("doq")) return Type::Doq;
    return Type::Plain;
}

LatencyProbe probeForServer(const Server &s, Type type)
{
    LatencyProbe p;
    switch (type) {
    case Type::Plain:
        break;
    case Type::Dot:
        if (!s.dotHost.isEmpty()) {
            p.type = Type::Dot;
            p.hostname = s.dotHost;
        }
        break;
    case Type::Doh:
        if (!s.dohUrl.isEmpty()) {
            QString url = s.dohUrl;
            url.remove(QStringLiteral("https://"));
            const int slash = url.indexOf(QLatin1Char('/'));
            p.type = Type::Doh;
            p.hostname = slash < 0 ? url : url.left(slash);
            p.path = slash < 0 ? QString() : url.mid(slash);
        }
        break;
    case Type::Doq:
        if (!s.doqEndpoint.isEmpty()) {
            p.type = Type::Doq;
            p.hostname = stripQuic(s.doqEndpoint);
        }
        break;
    }
    return p;
}

std::optional<qint64> measureLatency(const QString &serverIpv4, const LatencyProbe &probe)
{
    QString first = serverIpv4.split(QLatin1Char(',')).value(0);
    first = first.split(QLatin1Char('#')).value(0).trimmed();
    const QHostAddress ip(first);
    if (ip.isNull()) {
        return std::nullopt;
    }
    switch (probe.type) {
    case Type::Plain:
    case Type::Doq: // no public QUIC client in Qt: fall back to a plain probe
        return probePlain(ip);
    case Type::Dot:
        return probeDot(ip, probe.hostname);
    case Type::Doh:
        return probeDoh(ip, probe.hostname, probe.path);
    }
    return std::nullopt;
}

QList<std::pair<int, std::optional<qint64>>> measureAllLatencies()
{
    const auto &list = servers();
    QList<int> indices;
    for (int i = 0; i < list.size(); ++i) {
        indices << i;
    }
    // every lookup runs on its own worker thread
    QList<std::pair<int, std::optional<qint64>>> results =
        QtConcurrent::blockingMapped<QList<std::pair<int, std::optional<qint64>>>>(
            indices, [&list](int i) { return std::make_pair(i, measureLatency(list.at(i).ipv4, LatencyProbe{})); });
    std::stable_sort(results.begin(), results.end(), [](const auto &a, const auto &b) {
        return a.second.value_or(std::numeric_limits<qint64>::max()) < b.second.value_or(std::numeric_limits<qint64>::max());
    });
    return results;
}

QString appendDotHostname(const QString &addrs, const QString &hostname)
{
    if (addrs.isEmpty()) {
        return {};
    }
    QStringList out;
    for (const QString &addr : addrs.split(QLatin1Char(','))) {
        out << addr + QLatin1Char('#') + hostname;
    }
    return out.join(QLatin1Char(','));
}

bool isValidDnsInput(const QString &s)
{
    if (s.isEmpty()) {
        return false;
    }
    for (const QChar c : s) {
        const bool hex = c.isDigit() || (c.toLower() >= QLatin1Char('a') && c.toLower() <= QLatin1Char('f'));
        if (!hex && c != QLatin1Char('.') && c != QLatin1Char(':') && c != QLatin1Char(',')) {
            return false;
        }
    }
    return true;
}

bool isValidDotHostname(const QString &hostname)
{
    if (hostname.isEmpty() || hostname.size() > 253) {
        return false;
    }
    for (const QString &label : hostname.split(QLatin1Char('.'))) {
        if (label.isEmpty() || label.size() > 63 || label.startsWith(QLatin1Char('-')) ||
            label.endsWith(QLatin1Char('-'))) {
            return false;
        }
        for (const QChar c : label) {
            const ushort u = c.unicode();
            const bool alnum = (u >= '0' && u <= '9') || (u >= 'a' && u <= 'z') || (u >= 'A' && u <= 'Z');
            if (!alnum && c != QLatin1Char('-')) {
                return false;
            }
        }
    }
    return true;
}

bool isValidDohUrl(const QString &url)
{
    return url.startsWith(QStringLiteral("https://"));
}

bool isValidDoqEndpoint(const QString &endpoint)
{
    return endpoint.startsWith(QStringLiteral("quic:")) || endpoint.startsWith(QStringLiteral("quic://"));
}

QString blockyUpstream(const Server &server, BlockyMode mode)
{
    return mode == BlockyMode::Doh ? server.dohUrl : server.doqEndpoint;
}

std::optional<BlockyMode> blockyModeFromUpstream(const QString &upstream)
{
    if (isValidDohUrl(upstream)) {
        return BlockyMode::Doh;
    }
    if (isValidDoqEndpoint(upstream)) {
        return BlockyMode::Doq;
    }
    return std::nullopt;
}

static const QString kMarker = QStringLiteral("# Generated by ClaudeMods Hello");
static const QString kLegacyMarker = QStringLiteral("# Generated by CachyOS Hello"); // configs written by the old app

QString generateBlockyConfig(const QString &upstream, const QString &bootstrapIpv4, const QString &bootstrapIpv6,
                             const QString &dotHostname)
{
    QStringList allIps = bootstrapIpv4.split(QLatin1Char(','), Qt::SkipEmptyParts);
    allIps << bootstrapIpv6.split(QLatin1Char(','), Qt::SkipEmptyParts);

    QString bootstrap;
    if (!dotHostname.isEmpty()) {
        QStringList ips;
        for (const QString &ip : allIps) {
            ips << QStringLiteral("      - \"%1\"").arg(ip);
        }
        bootstrap = QStringLiteral("  - upstream: tcp-tls:%1\n    ips:\n%2").arg(dotHostname, ips.join(QLatin1Char('\n')));
    } else {
        QStringList lines;
        for (const QString &ip : allIps) {
            lines << QStringLiteral("  - upstream: \"%1\"").arg(ip);
        }
        bootstrap = lines.join(QLatin1Char('\n'));
    }

    return QStringLiteral(
               "%1 — do not edit manually\n"
               "upstreams:\n"
               "  groups:\n"
               "    default:\n"
               "      - \"%2\"\n"
               "  strategy: strict\n"
               "  timeout: 5s\n"
               "  userAgent: \"ClaudeMods/blocky\"\n"
               "\n"
               "bootstrapDns:\n"
               "%3\n"
               "\n"
               "ports:\n"
               "  dns:\n"
               "    - 127.0.0.1:53\n"
               "    - \"[::1]:53\"\n"
               "\n"
               "caching:\n"
               "  minTime: 5m\n"
               "  maxTime: 30m\n"
               "  prefetching: true\n")
        .arg(kMarker, upstream, bootstrap);
}

std::optional<std::pair<BlockyMode, QString>> readActiveBlocky()
{
    const QString config = utils::readTextFile(kBlockyConfigPath);
    if (!config.startsWith(kMarker) && !config.startsWith(kLegacyMarker)) {
        return std::nullopt;
    }
    for (const QString &line : config.split(QLatin1Char('\n'))) {
        QString t = line.trimmed();
        if (t.startsWith(QStringLiteral("- "))) {
            t = t.mid(2);
        }
        t.remove(QLatin1Char('"'));
        if (t.startsWith(QStringLiteral("https://")) || t.startsWith(QStringLiteral("quic:")) ||
            t.startsWith(QStringLiteral("quic://"))) {
            const auto mode = blockyModeFromUpstream(t);
            if (mode) {
                return std::make_pair(*mode, t);
            }
        }
    }
    return std::nullopt;
}

int findServerByBlockyUpstream(const QString &upstream, BlockyMode mode)
{
    const auto &list = servers();
    for (int i = 0; i < list.size(); ++i) {
        if (blockyUpstream(list.at(i), mode) == upstream && !upstream.isEmpty()) {
            return i;
        }
    }
    return -1;
}

BlockyBootstrap readBlockyBootstrap()
{
    BlockyBootstrap out;
    const QString config = utils::readTextFile(kBlockyConfigPath);
    if (config.isEmpty()) {
        return out;
    }
    QStringList v4;
    QStringList v6;
    bool inBootstrap = false;
    bool inIps = false;

    for (const QString &line : config.split(QLatin1Char('\n'))) {
        const QString t = line.trimmed();
        if (t.startsWith(QStringLiteral("bootstrapDns:"))) {
            inBootstrap = true;
            continue;
        }
        if (inBootstrap && !t.isEmpty() && !t.startsWith(QLatin1Char('-')) && !t.startsWith(QStringLiteral("ips:")) &&
            !line.startsWith(QLatin1Char(' '))) {
            break; // left the section
        }
        if (!inBootstrap) {
            continue;
        }
        if (t.startsWith(QStringLiteral("- upstream: tcp-tls:"))) {
            out.dotHostname = t.mid(QStringLiteral("- upstream: tcp-tls:").size());
            inIps = false;
            continue;
        }
        if (t.startsWith(QStringLiteral("- upstream:")) && !t.contains(QStringLiteral("tcp-tls"))) {
            QString ip = t.mid(QStringLiteral("- upstream:").size()).trimmed();
            ip.remove(QLatin1Char('"'));
            if (!ip.isEmpty()) {
                (ip.contains(QLatin1Char(':')) ? v6 : v4) << ip;
            }
            continue;
        }
        if (t == QLatin1String("ips:")) {
            inIps = true;
            continue;
        }
        if (inIps && t.startsWith(QStringLiteral("- "))) {
            QString ip = t.mid(2);
            ip.remove(QLatin1Char('"'));
            if (ip.contains(QLatin1Char(':'))) {
                v6 << ip;
            } else if (!ip.isEmpty()) {
                v4 << ip;
            }
        }
    }
    out.ipv4 = v4.join(QLatin1Char(','));
    out.ipv6 = v6.join(QLatin1Char(','));
    return out;
}

} // namespace dns

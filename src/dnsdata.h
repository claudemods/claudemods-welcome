#pragma once

#include <QList>
#include <QString>
#include <QStringList>

#include <optional>
#include <utility>

namespace dns {

struct Server {
    QString name;
    QString ipv4;      ///< comma separated
    QString ipv6;      ///< comma separated
    QString dotHost;   ///< empty = no DNS-over-TLS support
    QString dohUrl;    ///< empty = no DNS-over-HTTPS support
    QString doqEndpoint; ///< empty = no DNS-over-QUIC support
    QString region;
    QString homepage;
    bool filtering = false;
};

/// Ordered list of preset providers (same order as the original app).
const QList<Server> &servers();
int serverIndex(const QString &name);

enum class Type { Plain, Dot, Doh, Doq };
QString typeId(Type t);
Type typeFromId(const QString &id);

enum class BlockyMode { Doh, Doq };

struct LatencyProbe {
    Type type = Type::Plain;
    QString hostname;
    QString path; ///< DoH request path
};
LatencyProbe probeForServer(const Server &server, Type type);

/// Times a real DNS lookup in ms; nullopt on timeout/error. DoQ cannot be probed with Qt (no
/// public QUIC client) and is measured as plain UDP.
std::optional<qint64> measureLatency(const QString &serverIpv4, const LatencyProbe &probe);
/// (server index, latency) for every preset server sorted fastest first (timeouts last).
QList<std::pair<int, std::optional<qint64>>> measureAllLatencies();

QString appendDotHostname(const QString &addrs, const QString &hostname);
bool isValidDnsInput(const QString &s);
bool isValidDotHostname(const QString &hostname);
bool isValidDohUrl(const QString &url);
bool isValidDoqEndpoint(const QString &endpoint);

QString blockyUpstream(const Server &server, BlockyMode mode);
std::optional<BlockyMode> blockyModeFromUpstream(const QString &upstream);

inline const QString kBlockyConfigPath = QStringLiteral("/etc/blocky/blocky.yml");
inline const QString kBlockyService = QStringLiteral("blocky.service");

QString generateBlockyConfig(const QString &upstream, const QString &bootstrapIpv4, const QString &bootstrapIpv6,
                             const QString &dotHostname);
std::optional<std::pair<BlockyMode, QString>> readActiveBlocky();
int findServerByBlockyUpstream(const QString &upstream, BlockyMode mode);
struct BlockyBootstrap {
    QString ipv4;
    QString ipv6;
    QString dotHostname;
};
BlockyBootstrap readBlockyBootstrap();

} // namespace dns

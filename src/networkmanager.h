#pragma once

#include <QString>
#include <QStringList>

#include <optional>

/// NetworkManager access through `nmcli` (polkit prompts are handled by the desktop's agent).
namespace nm {

enum class DnsOverTls { Default = -1, No = 0, Opportunistic = 1, Yes = 2 };

struct DnsMods {
    std::optional<QStringList> ipv4Dns; ///< entries may carry a `#hostname` DoT SNI suffix
    std::optional<QStringList> ipv6Dns;
    std::optional<int> ipv4DnsPriority;
    std::optional<int> ipv6DnsPriority;
    std::optional<bool> ipv4IgnoreAutoDns;
    std::optional<bool> ipv6IgnoreAutoDns;
    std::optional<DnsOverTls> dnsOverTls;
};

enum class ApplyStatus { Applied, PendingReconnect, Failed };

struct DnsInfo {
    QString ipv4;
    QString ipv6;
    QString dotHostname; ///< empty when none
};

bool isAvailable();
QStringList connections();
QString activeConnectionName();
/// DNS configured on the connection; nullopt when both families are empty (automatic/DHCP).
std::optional<DnsInfo> dnsForConnection(const QString &name);
/// True when DNS-over-TLS is strict for the connection.
bool dotForConnection(const QString &name);

/// Applies `mods` to the connection and re-applies it on its live device.
ApplyStatus modifyConnectionDns(const QString &name, const DnsMods &mods, QString *error = nullptr);

} // namespace nm

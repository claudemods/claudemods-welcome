#pragma once

#include <QObject>
#include <QQmlEngine>
#include <QString>
#include <QStringList>
#include <QVariantList>

/// claudemods DNS Manager (the DNSManager v1.04 tool): pick a DNS provider for a NetworkManager
/// connection, see the DNS currently in use and reset to automatic (DHCP). Updated for 2026:
/// optional DNS over TLS, changes are re-applied live instead of reconnecting, no sudo password.
class DnsManager : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    Q_PROPERTY(QString currentDns READ currentDns NOTIFY currentDnsChanged)

public:
    explicit DnsManager(QObject *parent = nullptr);
    static DnsManager *instance();
    static DnsManager *create(QQmlEngine *, QJSEngine *);

    QString currentDns() const { return m_currentDns; }

    /// [{name, ipv4, ipv6, dot}] — the original provider list
    Q_INVOKABLE QVariantList providers() const;
    Q_INVOKABLE QStringList connections() const;
    Q_INVOKABLE QString activeConnection() const;
    /// Re-reads the DNS servers in use by `connection` (async, updates currentDns)
    Q_INVOKABLE void refresh(const QString &connection);
    Q_INVOKABLE void apply(const QString &connection, const QString &provider, bool useDot);
    Q_INVOKABLE void reset(const QString &connection);

signals:
    /// type: 0 info, 1 warning, 2 error
    void message(int type, const QString &text, const QString &title);
    void currentDnsChanged();

private:
    void setCurrentDns(const QString &text);

    QString m_currentDns = QStringLiteral("…");
};

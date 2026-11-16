#include "dnsmanager.h"

#include "actions.h"
#include "networkmanager.h"
#include "systemtool.h"
#include "utils.h"

#include <QThread>
#include <QtConcurrent>

namespace {

DnsManager *g_instance = nullptr;

struct Provider {
    const char *name;
    const char *ipv4;
    const char *ipv6;
    const char *dotHost; ///< SNI name for DNS over TLS
};

// The provider list of the original DNSManager (Quad9 uses the ECS-enabled 9.9.9.11 set).
const Provider kProviders[] = {
    {"Google Public DNS", "8.8.8.8,8.8.4.4", "2001:4860:4860::8888,2001:4860:4860::8844", "dns.google"},
    {"Cloudflare DNS", "1.1.1.1,1.0.0.1", "2606:4700:4700::1111,2606:4700:4700::1001", "cloudflare-dns.com"},
    {"OpenDNS", "208.67.222.222,208.67.220.220", "2620:119:35::35,2620:119:53::53", "dns.opendns.com"},
    {"Quad9 DNS", "9.9.9.11,149.112.112.11", "2620:fe::11,2620:fe::fe:11", "dns11.quad9.net"},
};

const Provider *findProvider(const QString &name)
{
    for (const Provider &p : kProviders) {
        if (name == QLatin1String(p.name)) {
            return &p;
        }
    }
    return nullptr;
}

/// Values of `IP4.DNS[n]:value` / `IP6.DNS[n]:value` lines (the value may itself contain ':' for IPv6).
QStringList dnsValues(const QString &nmcliOutput, const QString &prefix)
{
    QStringList out;
    for (const QString &line : nmcliOutput.split(QLatin1Char('\n'), Qt::SkipEmptyParts)) {
        if (!line.startsWith(prefix)) {
            continue;
        }
        QString value = line.mid(line.indexOf(QLatin1Char(':')) + 1);
        value.replace(QStringLiteral("\\:"), QStringLiteral(":"));
        if (!value.trimmed().isEmpty()) {
            out << value.trimmed();
        }
    }
    return out;
}

QString joinOrNotSet(const QStringList &l)
{
    return l.isEmpty() ? QStringLiteral("Not set") : l.join(QStringLiteral(", "));
}

const QString kTitle = QStringLiteral("DNS Manager");

} // namespace

DnsManager *DnsManager::instance()
{
    if (!g_instance) {
        g_instance = new DnsManager;
    }
    return g_instance;
}

DnsManager *DnsManager::create(QQmlEngine *, QJSEngine *)
{
    QQmlEngine::setObjectOwnership(instance(), QQmlEngine::CppOwnership);
    return instance();
}

DnsManager::DnsManager(QObject *parent)
    : QObject(parent)
{
    // dialogs go through the System Tool's message handling in the main window
    connect(this, &DnsManager::message, SystemTool::instance(), &SystemTool::message);
}

QVariantList DnsManager::providers() const
{
    QVariantList out;
    for (const Provider &p : kProviders) {
        out.append(QVariantMap{{QStringLiteral("name"), QString::fromLatin1(p.name)},
                               {QStringLiteral("ipv4"), QString::fromLatin1(p.ipv4).replace(QLatin1Char(','), QStringLiteral(", "))},
                               {QStringLiteral("ipv6"), QString::fromLatin1(p.ipv6).replace(QLatin1Char(','), QStringLiteral(", "))}});
    }
    return out;
}

QStringList DnsManager::connections() const
{
    return nm::connections();
}

QString DnsManager::activeConnection() const
{
    return nm::activeConnectionName();
}

void DnsManager::setCurrentDns(const QString &text)
{
    QMetaObject::invokeMethod(
        this,
        [this, text]() {
            m_currentDns = text;
            emit currentDnsChanged();
        },
        Qt::QueuedConnection);
}

void DnsManager::refresh(const QString &connection)
{
    if (connection.isEmpty()) {
        setCurrentDns(QStringLiteral("No active network connection found."));
        return;
    }
    (void)QtConcurrent::run([this, connection]() {
        // runtime values: what the connection is actually using right now
        const QString v4 = utils::cmdOutput(QStringLiteral("nmcli"), {QStringLiteral("-t"), QStringLiteral("-f"),
                                                                       QStringLiteral("IP4.DNS"), QStringLiteral("connection"),
                                                                       QStringLiteral("show"), connection});
        const QString v6 = utils::cmdOutput(QStringLiteral("nmcli"), {QStringLiteral("-t"), QStringLiteral("-f"),
                                                                       QStringLiteral("IP6.DNS"), QStringLiteral("connection"),
                                                                       QStringLiteral("show"), connection});
        setCurrentDns(QStringLiteral("IPv4: %1 | IPv6: %2")
                          .arg(joinOrNotSet(dnsValues(v4, QStringLiteral("IP4.DNS"))),
                               joinOrNotSet(dnsValues(v6, QStringLiteral("IP6.DNS")))));
    });
}

void DnsManager::apply(const QString &connection, const QString &provider, bool useDot)
{
    const Provider *p = findProvider(provider);
    if (!p || connection.isEmpty()) {
        emit message(2, QStringLiteral("Choose a provider and a network connection first."), kTitle);
        return;
    }
    const QString ipv4 = QString::fromLatin1(p->ipv4);
    const QString ipv6 = QString::fromLatin1(p->ipv6);
    const QString dotHost = QString::fromLatin1(p->dotHost);

    const actions::MessageSink sink = [this](actions::MessageType t, const QString &m) {
        emit message(int(t), m, actions::messageTypeName(t));
    };
    (void)QtConcurrent::run([=, this]() {
        actions::stopBlocky(); // a running blocky proxy would override the provider
        actions::changeDnsServer(connection, ipv4, ipv6, useDot, dotHost, sink);
        QThread::sleep(1);
        refresh(connection);
    });
}

void DnsManager::reset(const QString &connection)
{
    if (connection.isEmpty()) {
        emit message(2, QStringLiteral("No network connection selected."), kTitle);
        return;
    }
    const actions::MessageSink sink = [this](actions::MessageType t, const QString &m) {
        emit message(int(t), m, actions::messageTypeName(t));
    };
    (void)QtConcurrent::run([=, this]() {
        actions::resetDnsServer(connection, sink);
        QThread::sleep(1);
        refresh(connection);
    });
}

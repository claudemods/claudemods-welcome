#include "systemd.h"

#include <QDBusArgument>
#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusMessage>
#include <QDBusObjectPath>
#include <QDBusReply>
#include <QFileInfo>
#include <QMutex>
#include <QMutexLocker>
#include <QDebug>

namespace systemd {

namespace {

constexpr auto kService = "org.freedesktop.systemd1";
constexpr auto kPath = "/org/freedesktop/systemd1";
constexpr auto kManagerIface = "org.freedesktop.systemd1.Manager";
constexpr int kTimeoutMs = 120000; // polkit prompts can take a while

QMutex g_mutex;
QStringList g_systemUnits;
QStringList g_userUnits;

QDBusConnection bus(Scope scope)
{
    return scope == Scope::System ? QDBusConnection::systemBus() : QDBusConnection::sessionBus();
}

QDBusMessage callManager(Scope scope, const QString &method, const QVariantList &args)
{
    QDBusMessage msg = QDBusMessage::createMethodCall(QLatin1String(kService), QLatin1String(kPath),
                                                      QLatin1String(kManagerIface), method);
    msg.setArguments(args);
    msg.setInteractiveAuthorizationAllowed(true);
    return bus(scope).call(msg, QDBus::Block, kTimeoutMs);
}

bool ok(const QDBusMessage &reply, QString *error)
{
    if (reply.type() == QDBusMessage::ErrorMessage) {
        if (error) {
            *error = reply.errorName() + QStringLiteral(": ") + reply.errorMessage();
        }
        qWarning() << "systemd D-Bus call failed:" << reply.errorName() << reply.errorMessage();
        return false;
    }
    return true;
}

QStringList listEnabledUnits(Scope scope)
{
    const QDBusMessage reply = callManager(
        scope, QStringLiteral("ListUnitFilesByPatterns"),
        {QStringList{QStringLiteral("enabled")},
         QStringList{QStringLiteral("*.service"), QStringLiteral("*.socket"), QStringLiteral("*.timer")}});
    QStringList result;
    if (!ok(reply, nullptr) || reply.arguments().isEmpty()) {
        return result;
    }
    const QDBusArgument arg = reply.arguments().first().value<QDBusArgument>();
    arg.beginArray();
    while (!arg.atEnd()) {
        QString path;
        QString state;
        arg.beginStructure();
        arg >> path >> state;
        arg.endStructure();
        result << QFileInfo(path).fileName();
    }
    arg.endArray();
    return result;
}

bool unitCall(const QString &method, const QString &unit, Scope scope, QString *error)
{
    return ok(callManager(scope, method, {unit, QStringLiteral("replace")}), error);
}

bool allContained(const QStringList &haystack, const QString &units)
{
    const QStringList list = units.split(QLatin1Char(' '), Qt::SkipEmptyParts);
    for (const QString &u : list) {
        if (!haystack.contains(u)) {
            return false;
        }
    }
    return true;
}

} // namespace

void refreshSystemCache()
{
    const QStringList units = listEnabledUnits(Scope::System);
    QMutexLocker lock(&g_mutex);
    g_systemUnits = units;
}

void refreshUserCache()
{
    const QStringList units = listEnabledUnits(Scope::User);
    QMutexLocker lock(&g_mutex);
    g_userUnits = units;
}

void refreshCache()
{
    refreshSystemCache();
    refreshUserCache();
}

bool checkSystemUnits(const QString &units)
{
    QMutexLocker lock(&g_mutex);
    return allContained(g_systemUnits, units);
}

bool checkUserUnits(const QString &units)
{
    QMutexLocker lock(&g_mutex);
    return allContained(g_userUnits, units);
}

bool checkAnyUnits(const QString &units)
{
    QMutexLocker lock(&g_mutex);
    const QStringList list = units.split(QLatin1Char(' '), Qt::SkipEmptyParts);
    for (const QString &u : list) {
        if (!g_systemUnits.contains(u) && !g_userUnits.contains(u)) {
            return false;
        }
    }
    return true;
}

bool enable(const QStringList &units, Scope scope, bool force, QString *error)
{
    if (!ok(callManager(scope, QStringLiteral("EnableUnitFiles"), {units, false, force}), error)) {
        return false;
    }
    for (const QString &unit : units) {
        if (!unitCall(QStringLiteral("StartUnit"), unit, scope, error)) {
            return false;
        }
    }
    return true;
}

bool disable(const QStringList &units, Scope scope, QString *error)
{
    for (const QString &unit : units) {
        // NOTE: the unit may already be inactive
        unitCall(QStringLiteral("StopUnit"), unit, scope, nullptr);
    }
    return ok(callManager(scope, QStringLiteral("DisableUnitFiles"), {units, false}), error);
}

bool restart(const QString &unit, Scope scope, QString *error)
{
    return unitCall(QStringLiteral("RestartUnit"), unit, scope, error);
}

bool stop(const QString &unit, Scope scope, QString *error)
{
    return unitCall(QStringLiteral("StopUnit"), unit, scope, error);
}

bool isActive(const QString &unit, Scope scope)
{
    const QDBusMessage reply = callManager(scope, QStringLiteral("GetUnit"), {unit});
    if (!ok(reply, nullptr) || reply.arguments().isEmpty()) {
        return false;
    }
    const QString path = reply.arguments().first().value<QDBusObjectPath>().path();
    QDBusInterface iface(QLatin1String(kService), path, QStringLiteral("org.freedesktop.systemd1.Unit"), bus(scope));
    return iface.property("ActiveState").toString() == QLatin1String("active");
}

} // namespace systemd

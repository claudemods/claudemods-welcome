#pragma once

#include <QString>
#include <QStringList>

/// systemd access over D-Bus (QtDBus). All calls allow polkit interactive authorization.
namespace systemd {

enum class Scope { System, User };

/// Caches of units enabled between reboots (like the original app, refreshed explicitly).
void refreshCache();
void refreshSystemCache();
void refreshUserCache();

bool checkSystemUnits(const QString &units);
bool checkUserUnits(const QString &units);
bool checkAnyUnits(const QString &units);

/// Enable (EnableUnitFiles) and start units.
bool enable(const QStringList &units, Scope scope, bool force, QString *error = nullptr);
/// Stop and disable units.
bool disable(const QStringList &units, Scope scope, QString *error = nullptr);
bool restart(const QString &unit, Scope scope, QString *error = nullptr);
bool stop(const QString &unit, Scope scope, QString *error = nullptr);
bool isActive(const QString &unit, Scope scope);

} // namespace systemd

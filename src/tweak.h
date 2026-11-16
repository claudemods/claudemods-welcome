#pragma once

#include <QString>
#include <QStringList>

namespace tweak {

enum class Name { Psd, Oomd, Bpftune, Bluetooth, Ananicy, ClaudeUpdate };

struct Details {
    QString type;  // "service" or "user_service"
    QString units; // space separated unit list
    QString pkg;   // package that provides it ("" = none)
};

QList<Name> all();
Details details(Name tweak);
/// CLI / QML identifier ("psd", "oomd", ..., "claudemods-update")
QString id(Name tweak);
bool fromId(const QString &id, Name *out);
/// Label shown in the UI
QString label(Name tweak);
/// Fluent id of the tooltip
QString tooltipId(Name tweak);

bool shouldUninstallOnDisable(Name tweak);
QStringList autostartFiles(Name tweak);
QString autostartDir();
bool checkAutostartActive(Name tweak);
void removeAutostartFiles(Name tweak);
bool isGloballyEnabled(const QString &units);

} // namespace tweak

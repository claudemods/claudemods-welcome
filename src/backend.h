#pragma once

#include "dnsdata.h"

#include <QJsonObject>
#include <QObject>
#include <QQmlEngine>
#include <QString>
#include <QVariantList>
#include <QVariantMap>

class QNetworkAccessManager;

/// Everything the QML UI talks to. Registered as the `Backend` singleton of `ClaudeMods.Hello`.
class Backend : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    Q_PROPERTY(QString locale READ locale NOTIFY localeChanged)
    Q_PROPERTY(QVariantList languages READ languages CONSTANT)
    Q_PROPERTY(QString version READ version CONSTANT)
    Q_PROPERTY(QString appId READ appId CONSTANT)
    Q_PROPERTY(bool isIso READ isIso CONSTANT)
    Q_PROPERTY(bool hasNmcli READ hasNmcli CONSTANT)
    Q_PROPERTY(bool hasPackageInstaller READ hasPackageInstaller CONSTANT)
    Q_PROPERTY(bool hasKernelManager READ hasKernelManager CONSTANT)
    Q_PROPERTY(bool isKwinWayland READ isKwinWayland CONSTANT)
    Q_PROPERTY(bool hasVramButton READ hasVramButton CONSTANT)
    Q_PROPERTY(bool autostart READ autostart WRITE setAutostart NOTIFY autostartChanged)
    Q_PROPERTY(bool installerBusy READ installerBusy NOTIFY installerBusyChanged)
    Q_PROPERTY(QString releaseText READ releaseText NOTIFY releaseTextChanged)
    Q_PROPERTY(int tweaksRevision READ tweaksRevision NOTIFY tweaksChanged)

public:
    explicit Backend(QObject *parent = nullptr);
    static Backend *instance();
    static Backend *create(QQmlEngine *, QJSEngine *);

    // --- shared with the CLI -----------------------------------------------------------------
    static QJsonObject loadPreferences();
    static QString savedLocale();
    static QString bestLocale(const QString &saved);
    /// Selects the translation language (saved / system / fallback)
    static void setupLocalization();

    QString locale() const { return m_locale; }
    QVariantList languages() const;
    QString version() const;
    QString appId() const;
    bool isIso() const { return m_isIso; }
    bool hasNmcli() const { return m_hasNmcli; }
    bool hasPackageInstaller() const { return m_hasPackageInstaller; }
    bool hasKernelManager() const { return m_hasKernelManager; }
    bool isKwinWayland() const { return m_isKwinWayland; }
    bool hasVramButton() const { return m_hasVramButton; }
    bool autostart() const { return m_autostart; }
    void setAutostart(bool enabled);
    bool installerBusy() const { return m_installerBusy; }
    QString releaseText() const { return m_releaseText; }
    int tweaksRevision() const { return m_tweaksRevision; }

    // --- general -----------------------------------------------------------------------------
    Q_INVOKABLE QString translate(const QString &id, const QVariantMap &args = {}) const;
    Q_INVOKABLE void setLocale(const QString &code);
    Q_INVOKABLE void openUrl(const QString &keyOrUrl);
    Q_INVOKABLE QString pageText(const QString &name) const;
    Q_INVOKABLE void saveSettings();
    Q_INVOKABLE void launchInstaller();
    Q_INVOKABLE void launchApp(const QString &which);
    Q_INVOKABLE void launchPackageBrowser();

    // --- tweaks / utilities ------------------------------------------------------------------
    Q_INVOKABLE QVariantList tweakList() const;
    Q_INVOKABLE bool tweakEnabled(const QString &id) const;
    Q_INVOKABLE void toggleTweak(const QString &id);
    Q_INVOKABLE void runAction(const QString &id);

    // --- DNS ---------------------------------------------------------------------------------
    Q_INVOKABLE QVariantList dnsServers() const;
    Q_INVOKABLE QStringList nmConnections() const;
    Q_INVOKABLE QString activeConnection() const;
    Q_INVOKABLE QVariantMap dnsState(const QString &connection) const;
    Q_INVOKABLE void applyDns(const QVariantMap &params);
    Q_INVOKABLE void resetDns(const QString &connection);
    Q_INVOKABLE void testLatency(const QVariantMap &params);
    Q_INVOKABLE void bestServer();

signals:
    /// type: 0 info, 1 warning, 2 error
    void message(int type, const QString &text, const QString &title);
    void localeChanged();
    void autostartChanged();
    void installerBusyChanged();
    void releaseTextChanged();
    void tweaksChanged();
    void tweakFailed(const QString &id, const QString &packageName);
    /// milliseconds, or -1 on timeout/error
    void latencyResult(qint64 ms);
    /// serverIndex -1 when nothing answered
    void bestServerResult(int serverIndex, qint64 ms);
    void raiseRequested();

private:
    bool guiRunCommand(const QString &command, bool escalate);
    void emitMessage(int type, const QString &text, const QString &title = QString());
    void setInstallerBusy(bool busy);
    void fetchReleasePage();
    int selectionIndexForConnection(const QString &conn) const;
    dns::Type presetTypeForServer(int index) const;

    QJsonObject m_prefs;
    QString m_locale;
    bool m_isIso = false;
    bool m_hasNmcli = false;
    bool m_hasPackageInstaller = false;
    bool m_hasKernelManager = false;
    bool m_isKwinWayland = false;
    bool m_hasVramButton = false;
    bool m_autostart = false;
    bool m_installerBusy = false;
    QString m_releaseText;
    int m_tweaksRevision = 0;
    QNetworkAccessManager *m_net = nullptr;
};

#pragma once

#include <QJsonObject>
#include <QObject>
#include <QQmlEngine>
#include <QUrl>
#include <QVariantList>

/// The claudemods System Tool: themes (wallpapers + colour schemes), updates, GPU drivers,
/// repositories, backups and a set of desktop tools. The menu is data driven:
///   data/systemtool/actions.json   categories and actions
///   data/systemtool/themes.json    wallpapers and named colour schemes
///   data/systemtool/scripts/*.sh   the shell scripts the actions run
class SystemTool : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    Q_PROPERTY(bool hasPlasmaTools READ hasPlasmaTools CONSTANT)

public:
    explicit SystemTool(QObject *parent = nullptr);
    static SystemTool *instance();
    static SystemTool *create(QQmlEngine *, QJSEngine *);

    bool hasPlasmaTools() const { return m_hasPlasmaTools; }

    Q_INVOKABLE QVariantList categories() const;
    Q_INVOKABLE QVariantList actions(const QString &category) const;
    Q_INVOKABLE void runAction(const QString &id);

    Q_INVOKABLE QVariantList wallpapers() const;
    Q_INVOKABLE void applyWallpaper(const QString &id);
    Q_INVOKABLE QVariantList colorSchemes() const;
    Q_INVOKABLE void applyColorScheme(const QString &id);

    Q_INVOKABLE void integrateAppImage(const QUrl &file);

signals:
    /// type: 0 info, 1 warning, 2 error
    void message(int type, const QString &text, const QString &title);
    void colorSchemesChanged();

private:
    QJsonObject findAction(const QString &id) const;
    QString buildScript(const QJsonObject &action) const;
    void notify(int type, const QString &text);

    QJsonObject m_actions;
    QJsonObject m_themes;
    bool m_hasPlasmaTools = false;
};

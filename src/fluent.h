#pragma once

#include <QHash>
#include <QMutex>
#include <QString>
#include <QStringList>
#include <QVariantMap>

/// Minimal Project-Fluent (.ftl) loader. Supports exactly the subset used by the translation
/// files: `id = text`, multi-line indented values, `{ $var }` placeables, `{ "" }` literals and
/// `#` comments. English is always loaded first as the fallback language.
class Fluent
{
public:
    static Fluent &instance();

    /// Language codes that have a translation file (e.g. "en", "pt-BR").
    QStringList availableLanguages() const;
    bool isLanguageAvailable(const QString &code) const;

    /// Loads `code` on top of the English fallback.
    void select(const QString &code);
    QString current() const;

    QString get(const QString &id, const QVariantMap &args = {}) const;

    static QHash<QString, QString> parse(const QString &text);

private:
    Fluent();
    static QHash<QString, QString> loadFile(const QString &code);

    mutable QMutex m_mutex;
    QHash<QString, QString> m_fallback;
    QHash<QString, QString> m_messages;
    QString m_current;
};

/// Shorthand used throughout the backend.
inline QString fl(const QString &id, const QVariantMap &args = {})
{
    return Fluent::instance().get(id, args);
}

#include "fluent.h"

#include <QDir>
#include <QFile>
#include <QMutexLocker>
#include <QRegularExpression>

static const QString kFtlName = QStringLiteral("claudemods_hello.ftl");

Fluent &Fluent::instance()
{
    static Fluent inst;
    return inst;
}

Fluent::Fluent()
{
    m_fallback = loadFile(QStringLiteral("en"));
    m_messages = m_fallback;
    m_current = QStringLiteral("en");
}

QStringList Fluent::availableLanguages() const
{
    QStringList out;
    const QDir dir(QStringLiteral(":/i18n"));
    for (const QString &name : dir.entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name)) {
        if (QFile::exists(QStringLiteral(":/i18n/%1/%2").arg(name, kFtlName))) {
            out << name;
        }
    }
    return out;
}

bool Fluent::isLanguageAvailable(const QString &code) const
{
    if (code.isEmpty()) {
        return false;
    }
    return availableLanguages().contains(code);
}

QHash<QString, QString> Fluent::loadFile(const QString &code)
{
    QFile f(QStringLiteral(":/i18n/%1/%2").arg(code, kFtlName));
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return {};
    }
    return parse(QString::fromUtf8(f.readAll()));
}

void Fluent::select(const QString &code)
{
    QHash<QString, QString> merged = m_fallback;
    if (code != QLatin1String("en")) {
        const auto localized = loadFile(code);
        for (auto it = localized.cbegin(); it != localized.cend(); ++it) {
            merged.insert(it.key(), it.value());
        }
    }
    QMutexLocker lock(&m_mutex);
    m_messages = merged;
    m_current = code;
}

QString Fluent::current() const
{
    QMutexLocker lock(&m_mutex);
    return m_current;
}

QString Fluent::get(const QString &id, const QVariantMap &args) const
{
    QString text;
    {
        QMutexLocker lock(&m_mutex);
        const auto it = m_messages.constFind(id);
        if (it == m_messages.constEnd()) {
            return id;
        }
        text = it.value();
    }

    static const QRegularExpression varRe(QStringLiteral(R"(\{\s*\$([A-Za-z][A-Za-z0-9_-]*)\s*\})"));
    static const QRegularExpression emptyRe(QStringLiteral(R"(\{\s*""\s*\})"));

    QString out;
    qsizetype last = 0;
    auto it = varRe.globalMatch(text);
    while (it.hasNext()) {
        const auto m = it.next();
        out += text.mid(last, m.capturedStart() - last);
        out += args.value(m.captured(1)).toString();
        last = m.capturedEnd();
    }
    out += text.mid(last);
    out.replace(emptyRe, QString());
    return out;
}

QHash<QString, QString> Fluent::parse(const QString &text)
{
    QHash<QString, QString> out;
    static const QRegularExpression entryRe(QStringLiteral(R"(^([A-Za-z][A-Za-z0-9_-]*)\s*=\s?(.*)$)"));

    QString id;
    QString inlineValue;
    QStringList cont;
    int pendingBlank = 0;

    auto flush = [&]() {
        if (id.isEmpty()) {
            return;
        }
        // dedent continuation lines
        int minIndent = -1;
        for (const QString &l : std::as_const(cont)) {
            if (l.trimmed().isEmpty()) {
                continue;
            }
            int n = 0;
            while (n < l.size() && l[n] == QLatin1Char(' ')) {
                ++n;
            }
            minIndent = minIndent < 0 ? n : qMin(minIndent, n);
        }
        QStringList dedented;
        for (const QString &l : std::as_const(cont)) {
            dedented << (l.trimmed().isEmpty() ? QString() : l.mid(qMax(minIndent, 0)));
        }
        QString value = inlineValue;
        if (!dedented.isEmpty()) {
            const QString block = dedented.join(QLatin1Char('\n'));
            value = value.isEmpty() ? block : value + QLatin1Char('\n') + block;
        }
        while (value.startsWith(QLatin1Char('\n'))) {
            value.remove(0, 1);
        }
        while (value.endsWith(QLatin1Char('\n'))) {
            value.chop(1);
        }
        out.insert(id, value);
        id.clear();
        inlineValue.clear();
        cont.clear();
        pendingBlank = 0;
    };

    const QStringList lines = text.split(QLatin1Char('\n'));
    for (QString line : lines) {
        if (line.endsWith(QLatin1Char('\r'))) {
            line.chop(1);
        }
        if (line.trimmed().isEmpty()) {
            if (!id.isEmpty()) {
                ++pendingBlank;
            }
            continue;
        }
        if (line.startsWith(QLatin1Char(' ')) || line.startsWith(QLatin1Char('\t'))) {
            if (!id.isEmpty()) {
                for (int i = 0; i < pendingBlank; ++i) {
                    cont << QString();
                }
                pendingBlank = 0;
                cont << line;
            }
            continue;
        }
        // non-indented line ends the previous message
        flush();
        if (line.startsWith(QLatin1Char('#'))) {
            continue;
        }
        const auto m = entryRe.match(line);
        if (m.hasMatch()) {
            id = m.captured(1);
            inlineValue = m.captured(2);
        }
    }
    flush();
    return out;
}

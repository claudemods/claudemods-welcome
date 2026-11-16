#include "systemtool.h"

#include "externals.h"
#include "utils.h"

#include <QCoreApplication>
#include <QDebug>
#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImageReader>
#include <QJsonArray>
#include <QJsonDocument>
#include <QProcess>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QtConcurrent>

#include <algorithm>

namespace {

SystemTool *g_instance = nullptr;

const QString kTitle = QStringLiteral("System Tool");

QJsonObject loadJson(const QString &path)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) {
        qCritical() << "missing resource" << path;
        return {};
    }
    return QJsonDocument::fromJson(f.readAll()).object();
}

QString readResource(const QString &path)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return {};
    }
    return QString::fromUtf8(f.readAll());
}

QString userColorDir()
{
    return QDir::homePath() + QStringLiteral("/.local/share/color-schemes");
}

QString systemColorDir()
{
    return QStringLiteral("/usr/share/color-schemes");
}

/// Reads `key` of `[section]` from a KDE .colors file and returns it as "#rrggbb".
QString readColor(const QString &text, const QString &section, const QString &key)
{
    bool inSection = false;
    for (const QString &raw : text.split(QLatin1Char('\n'))) {
        const QString line = raw.trimmed();
        if (line.startsWith(QLatin1Char('['))) {
            inSection = line == QLatin1Char('[') + section + QLatin1Char(']');
            continue;
        }
        if (inSection && line.startsWith(key + QLatin1Char('='))) {
            const QStringList rgb = line.mid(key.size() + 1).split(QLatin1Char(','));
            if (rgb.size() >= 3) {
                return QStringLiteral("#%1%2%3")
                    .arg(rgb[0].trimmed().toInt(), 2, 16, QLatin1Char('0'))
                    .arg(rgb[1].trimmed().toInt(), 2, 16, QLatin1Char('0'))
                    .arg(rgb[2].trimmed().toInt(), 2, 16, QLatin1Char('0'));
            }
        }
    }
    return {};
}

QString readGeneralName(const QString &text)
{
    static const QRegularExpression re(QStringLiteral("^Name=(.+)$"), QRegularExpression::MultilineOption);
    const auto m = re.match(text);
    return m.hasMatch() ? m.captured(1).trimmed() : QString();
}

/// Value of `key` inside the [Desktop Entry] group of a .desktop file's text.
QString desktopValue(const QString &text, const QString &key)
{
    bool inEntry = false;
    for (const QString &raw : text.split(QLatin1Char('\n'))) {
        const QString line = raw.trimmed();
        if (line.startsWith(QLatin1Char('['))) {
            inEntry = line == QLatin1String("[Desktop Entry]");
            continue;
        }
        if (inEntry && line.startsWith(key + QLatin1Char('='))) {
            return line.mid(key.size() + 1).trimmed();
        }
    }
    return {};
}

QString resolveWallpaper(const QString &file)
{
    QStringList dirs = {
        QStringLiteral("/usr/share/claudemods-hello/wallpapers"),
        QCoreApplication::applicationDirPath() + QStringLiteral("/wallpapers"),
        QCoreApplication::applicationDirPath() + QStringLiteral("/../share/claudemods-hello/wallpapers"),
    };
#ifdef CLAUDEMODS_SOURCE_DIR
    dirs << QStringLiteral(CLAUDEMODS_SOURCE_DIR "/data/wallpapers");
#endif
    for (const QString &d : std::as_const(dirs)) {
        const QString p = QDir::cleanPath(d + QLatin1Char('/') + file);
        if (QFile::exists(p)) {
            return p;
        }
    }
    return {};
}

QString sanitizedName(const QString &name)
{
    QString s = name;
    s.replace(QRegularExpression(QStringLiteral("[^A-Za-z0-9._-]+")), QStringLiteral("-"));
    return s.isEmpty() ? QStringLiteral("appimage") : s;
}

QString pickIconSize(int px)
{
    for (int size : {512, 256, 128, 64}) {
        if (px >= size) {
            return QStringLiteral("%1x%1").arg(size);
        }
    }
    return QStringLiteral("48x48");
}

/// Finds the best icon for an extracted AppImage; returns {path, hicolor size dir or "scalable"}.
QPair<QString, QString> findAppImageIcon(const QDir &root, const QString &iconName)
{
    QStringList candidates;
    if (!iconName.isEmpty()) {
        if (QFileInfo(iconName).isAbsolute()) {
            candidates << root.filePath(iconName.mid(1));
        }
        for (const QString &ext : {QStringLiteral("svg"), QStringLiteral("png"), QStringLiteral("xpm")}) {
            candidates << root.filePath(iconName + QLatin1Char('.') + ext);
        }
        // hicolor theme inside the image: prefer scalable, then the largest raster size
        const QDir hicolor(root.filePath(QStringLiteral("usr/share/icons/hicolor")));
        QStringList sizes = hicolor.entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
        std::sort(sizes.begin(), sizes.end(), [](const QString &a, const QString &b) {
            if (a == QLatin1String("scalable")) return true;
            if (b == QLatin1String("scalable")) return false;
            return a.section(QLatin1Char('x'), 0, 0).toInt() > b.section(QLatin1Char('x'), 0, 0).toInt();
        });
        for (const QString &sz : std::as_const(sizes)) {
            for (const QString &ext : {QStringLiteral("svg"), QStringLiteral("png")}) {
                candidates << hicolor.filePath(sz + QStringLiteral("/apps/") + iconName + QLatin1Char('.') + ext);
            }
        }
    }
    candidates << root.filePath(QStringLiteral(".DirIcon"));
    for (const QString &c : std::as_const(candidates)) {
        const QFileInfo info(c);
        if (!info.exists()) {
            continue;
        }
        const QString real = info.canonicalFilePath();
        if (real.endsWith(QLatin1String(".svg"), Qt::CaseInsensitive)) {
            return {real, QStringLiteral("scalable")};
        }
        const QSize size = QImageReader(real).size();
        return {real, pickIconSize(qMax(size.width(), size.height()))};
    }
    // last resort: any image in the root of the image
    const QStringList any = root.entryList({QStringLiteral("*.png"), QStringLiteral("*.svg")}, QDir::Files);
    if (!any.isEmpty()) {
        const QString real = QFileInfo(root.filePath(any.first())).canonicalFilePath();
        if (real.endsWith(QLatin1String(".svg"), Qt::CaseInsensitive)) {
            return {real, QStringLiteral("scalable")};
        }
        const QSize size = QImageReader(real).size();
        return {real, pickIconSize(qMax(size.width(), size.height()))};
    }
    return {};
}

} // namespace

SystemTool *SystemTool::instance()
{
    if (!g_instance) {
        g_instance = new SystemTool;
    }
    return g_instance;
}

SystemTool *SystemTool::create(QQmlEngine *, QJSEngine *)
{
    QQmlEngine::setObjectOwnership(instance(), QQmlEngine::CppOwnership);
    return instance();
}

SystemTool::SystemTool(QObject *parent)
    : QObject(parent)
{
    m_actions = loadJson(QStringLiteral(":/data/systemtool/actions.json"));
    m_themes = loadJson(QStringLiteral(":/data/systemtool/themes.json"));
    m_hasPlasmaTools = !utils::findExecutable(QStringLiteral("plasma-apply-wallpaperimage")).isEmpty();
}

void SystemTool::notify(int type, const QString &text)
{
    emit message(type, text, kTitle);
}

// ---------------------------------------------------------------------------------------------
// categories and actions
// ---------------------------------------------------------------------------------------------

QVariantList SystemTool::categories() const
{
    QVariantList out;
    for (const QJsonValue &v : m_actions.value(QStringLiteral("categories")).toArray()) {
        const QJsonObject c = v.toObject();
        const QString needed = c.value(QStringLiteral("requires")).toString();
        if (!needed.isEmpty() && utils::findExecutable(needed).isEmpty()) {
            continue; // hide sections whose tool is missing (e.g. the DNS Manager without NetworkManager)
        }
        out.append(QVariantMap{{QStringLiteral("id"), c.value(QStringLiteral("id")).toString()},
                               {QStringLiteral("title"), c.value(QStringLiteral("title")).toString()},
                               {QStringLiteral("description"), c.value(QStringLiteral("description")).toString()},
                               {QStringLiteral("icon"), c.value(QStringLiteral("icon")).toString()},
                               {QStringLiteral("view"), c.value(QStringLiteral("view")).toString(QStringLiteral("actions"))}});
    }
    return out;
}

QVariantList SystemTool::actions(const QString &category) const
{
    QVariantList out;
    for (const QJsonValue &cv : m_actions.value(QStringLiteral("categories")).toArray()) {
        const QJsonObject c = cv.toObject();
        if (c.value(QStringLiteral("id")).toString() != category) {
            continue;
        }
        for (const QJsonValue &av : c.value(QStringLiteral("actions")).toArray()) {
            const QJsonObject a = av.toObject();
            const QString requires_ = a.value(QStringLiteral("requires")).toString();
            if (!requires_.isEmpty() && utils::findExecutable(requires_).isEmpty()) {
                continue; // hide launchers for apps that are not installed
            }
            out.append(QVariantMap{{QStringLiteral("id"), a.value(QStringLiteral("id")).toString()},
                                   {QStringLiteral("title"), a.value(QStringLiteral("title")).toString()},
                                   {QStringLiteral("description"), a.value(QStringLiteral("description")).toString()},
                                   {QStringLiteral("kind"), a.value(QStringLiteral("kind")).toString(QStringLiteral("terminal"))},
                                   {QStringLiteral("confirm"), a.value(QStringLiteral("confirm")).toString()},
                                   {QStringLiteral("page"), a.value(QStringLiteral("page")).toString()}});
        }
    }
    return out;
}

QJsonObject SystemTool::findAction(const QString &id) const
{
    for (const QJsonValue &cv : m_actions.value(QStringLiteral("categories")).toArray()) {
        for (const QJsonValue &av : cv.toObject().value(QStringLiteral("actions")).toArray()) {
            if (av.toObject().value(QStringLiteral("id")).toString() == id) {
                return av.toObject();
            }
        }
    }
    return {};
}

QString SystemTool::buildScript(const QJsonObject &a) const
{
    const QString lib = readResource(QStringLiteral(":/data/systemtool/scripts/_lib.sh"));
    const QString body = readResource(QStringLiteral(":/data/systemtool/scripts/") + a.value(QStringLiteral("script")).toString());
    const QString args = a.value(QStringLiteral("args")).toString();
    // The subshell keeps an `exit` inside a script from closing the terminal before the user reads it.
    return QStringLiteral("(\n%1\nset -- %2\n%3\n)").arg(lib, args, body);
}

void SystemTool::runAction(const QString &id)
{
    const QJsonObject a = findAction(id);
    if (a.isEmpty()) {
        qWarning() << "unknown system tool action" << id;
        return;
    }
    const QString kind = a.value(QStringLiteral("kind")).toString(QStringLiteral("terminal"));

    if (kind == QLatin1String("url")) {
        QDesktopServices::openUrl(QUrl(a.value(QStringLiteral("url")).toString()));
    } else if (kind == QLatin1String("launch")) {
        const QString program = a.value(QStringLiteral("program")).toString();
        const QString path = utils::findExecutable(program);
        if (path.isEmpty()) {
            notify(2, QStringLiteral("'%1' is not installed.").arg(program));
            return;
        }
        QStringList args;
        for (const QJsonValue &v : a.value(QStringLiteral("arguments")).toArray()) {
            args << v.toString();
        }
        if (!QProcess::startDetached(path, args)) {
            notify(2, QStringLiteral("Could not start '%1'.").arg(program));
        }
    } else if (kind == QLatin1String("tui")) {
        // a terminal UI program (for example claude-codex): open it in Konsole
        const QString program = a.value(QStringLiteral("program")).toString();
        const QString path = utils::findExecutable(program);
        if (path.isEmpty()) {
            notify(2, QStringLiteral("'%1' is not installed. Install it first, then try again.").arg(program));
            return;
        }
        if (!utils::runInKonsole(QStringLiteral("\"%1\"").arg(path))) {
            (void)QtConcurrent::run([path]() { utils::runInTerminal(path, false); });
        }
    } else if (kind == QLatin1String("terminal")) {
        const QString script = buildScript(a);
        const bool escalate = a.value(QStringLiteral("escalate")).toBool();
        if (script.trimmed().isEmpty()) {
            notify(2, QStringLiteral("The script for this action is missing."));
            return;
        }
        (void)QtConcurrent::run([script, escalate]() { utils::runInTerminal(script, escalate); });
    }
    // "appimage" and "page" are handled by the UI (file picker / page switch)
}

// ---------------------------------------------------------------------------------------------
// themes: wallpapers
// ---------------------------------------------------------------------------------------------

QVariantList SystemTool::wallpapers() const
{
    QVariantList out;
    for (const QJsonValue &v : m_themes.value(QStringLiteral("wallpapers")).toArray()) {
        const QJsonObject w = v.toObject();
        const QString file = w.value(QStringLiteral("file")).toString();
        out.append(QVariantMap{
            {QStringLiteral("id"), w.value(QStringLiteral("id")).toString()},
            {QStringLiteral("name"), w.value(QStringLiteral("name")).toString()},
            {QStringLiteral("thumb"),
             QStringLiteral("qrc:/data/img/wallpapers/%1.jpg").arg(QFileInfo(file).completeBaseName())},
            {QStringLiteral("available"), !resolveWallpaper(file).isEmpty()}});
    }
    return out;
}

void SystemTool::applyWallpaper(const QString &id)
{
    QString file;
    for (const QJsonValue &v : m_themes.value(QStringLiteral("wallpapers")).toArray()) {
        if (v.toObject().value(QStringLiteral("id")).toString() == id) {
            file = v.toObject().value(QStringLiteral("file")).toString();
        }
    }
    const QString path = resolveWallpaper(file);
    if (path.isEmpty()) {
        notify(2, QStringLiteral("The wallpaper file '%1' was not found. Install the app (cmake --install) or keep "
                                 "the data/wallpapers folder next to the sources.")
                      .arg(file));
        return;
    }
    const QString tool = utils::findExecutable(QStringLiteral("plasma-apply-wallpaperimage"));
    if (tool.isEmpty()) {
        notify(2, QStringLiteral("plasma-apply-wallpaperimage was not found (KDE Plasma is required)."));
        return;
    }
    (void)QtConcurrent::run([this, tool, path]() {
        if (utils::runProcess(tool, {path}) != 0) {
            notify(2, QStringLiteral("Could not apply the wallpaper."));
        }
    });
}

// ---------------------------------------------------------------------------------------------
// themes: colour schemes
// ---------------------------------------------------------------------------------------------

QVariantList SystemTool::colorSchemes() const
{
    struct Entry {
        QString id;
        QString name;
        bool system = false;
        QString fallback;
        QString fallbackAccent;
    };
    QList<Entry> entries;
    QStringList known;
    for (const QJsonValue &v : m_themes.value(QStringLiteral("colors")).toArray()) {
        const QJsonObject c = v.toObject();
        entries.append({c.value(QStringLiteral("id")).toString(), c.value(QStringLiteral("name")).toString(),
                        c.value(QStringLiteral("system")).toBool(), c.value(QStringLiteral("fallback")).toString(),
                        c.value(QStringLiteral("fallbackAccent")).toString()});
        known << entries.last().id;
    }
    // schemes you added yourself to ~/.local/share/color-schemes show up automatically
    QStringList userFiles = QDir(userColorDir()).entryList({QStringLiteral("*.colors")}, QDir::Files, QDir::Name);
    for (const QString &f : std::as_const(userFiles)) {
        const QString id = QFileInfo(f).completeBaseName();
        if (!known.contains(id) && !id.contains(QLatin1Char('('))) {
            entries.append({id, QString(), false, QString(), QString()});
        }
    }

    QVariantList out;
    for (const Entry &e : std::as_const(entries)) {
        QString text;
        bool installed = false;
        for (const QString &p : {userColorDir() + QLatin1Char('/') + e.id + QStringLiteral(".colors"),
                                 systemColorDir() + QLatin1Char('/') + e.id + QStringLiteral(".colors")}) {
            if (QFile::exists(p)) {
                text = readResource(p);
                installed = true;
                break;
            }
        }
        if (text.isEmpty()) {
            text = readResource(QStringLiteral(":/data/color-schemes/") + e.id + QStringLiteral(".colors"));
        }
        if (text.isEmpty() && e.fallback.isEmpty()) {
            continue; // listed but not available anywhere
        }
        QString bg = readColor(text, QStringLiteral("Colors:Window"), QStringLiteral("BackgroundNormal"));
        QString accent = readColor(text, QStringLiteral("Colors:Selection"), QStringLiteral("BackgroundNormal"));
        if (bg.isEmpty()) bg = e.fallback;
        if (accent.isEmpty()) accent = e.fallbackAccent.isEmpty() ? bg : e.fallbackAccent;
        QString name = e.name;
        if (name.isEmpty()) name = readGeneralName(text);
        if (name.isEmpty()) name = e.id;
        out.append(QVariantMap{{QStringLiteral("id"), e.id},
                               {QStringLiteral("name"), name},
                               {QStringLiteral("bg"), bg},
                               {QStringLiteral("accent"), accent},
                               {QStringLiteral("installed"), installed || e.system}});
    }
    return out;
}

void SystemTool::applyColorScheme(const QString &id)
{
    const QString tool = utils::findExecutable(QStringLiteral("plasma-apply-colorscheme"));
    if (tool.isEmpty()) {
        notify(2, QStringLiteral("plasma-apply-colorscheme was not found (KDE Plasma is required)."));
        return;
    }
    // Install the bundled scheme on demand, so the buttons work without a manual setup step
    const QString user = userColorDir() + QLatin1Char('/') + id + QStringLiteral(".colors");
    const QString system = systemColorDir() + QLatin1Char('/') + id + QStringLiteral(".colors");
    const QString bundled = QStringLiteral(":/data/color-schemes/") + id + QStringLiteral(".colors");
    if (!QFile::exists(user) && !QFile::exists(system) && QFile::exists(bundled)) {
        QDir().mkpath(userColorDir());
        if (QFile::copy(bundled, user)) {
            QFile::setPermissions(user, QFile::ReadOwner | QFile::WriteOwner | QFile::ReadGroup | QFile::ReadOther);
            emit colorSchemesChanged();
        }
    }
    (void)QtConcurrent::run([this, tool, id]() {
        if (utils::runProcess(tool, {id}) != 0) {
            notify(2, QStringLiteral("Could not apply the colour scheme '%1'.").arg(id));
        }
    });
}

// ---------------------------------------------------------------------------------------------
// AppImage integration
// ---------------------------------------------------------------------------------------------

void SystemTool::integrateAppImage(const QUrl &file)
{
    const QString path = file.isLocalFile() ? file.toLocalFile() : file.toString();
    (void)QtConcurrent::run([this, path]() {
        const QFileInfo info(path);
        if (!info.exists() || !info.isFile()) {
            notify(2, QStringLiteral("The selected file does not exist."));
            return;
        }
        QFile::setPermissions(path, QFile(path).permissions() | QFile::ExeOwner | QFile::ExeGroup | QFile::ExeOther);

        QTemporaryDir tmp;
        if (!tmp.isValid()) {
            notify(2, QStringLiteral("Could not create a temporary folder."));
            return;
        }
        QProcess extract;
        extract.setWorkingDirectory(tmp.path());
        extract.start(info.absoluteFilePath(), {QStringLiteral("--appimage-extract")});
        if (!extract.waitForFinished(180000) || extract.exitCode() != 0) {
            notify(2, QStringLiteral("Could not read the AppImage: %1")
                          .arg(QString::fromUtf8(extract.readAllStandardError()).trimmed()));
            return;
        }
        const QDir root(tmp.filePath(QStringLiteral("squashfs-root")));

        // Use the metadata shipped inside the image
        QString desktopText;
        const QStringList desktops = root.entryList({QStringLiteral("*.desktop")}, QDir::Files);
        if (!desktops.isEmpty()) {
            desktopText = readResource(root.filePath(desktops.first()));
        }
        const QString base = sanitizedName(info.completeBaseName());
        QString name = desktopValue(desktopText, QStringLiteral("Name"));
        if (name.isEmpty()) name = info.completeBaseName();

        const auto icon = findAppImageIcon(root, desktopValue(desktopText, QStringLiteral("Icon")));
        QString installedIcon;
        if (!icon.first.isEmpty()) {
            const QString ext = QFileInfo(icon.first).suffix().isEmpty() ? QStringLiteral("png") : QFileInfo(icon.first).suffix();
            const QString dir = QDir::homePath() + QStringLiteral("/.local/share/icons/hicolor/") + icon.second + QStringLiteral("/apps");
            QDir().mkpath(dir);
            installedIcon = dir + QLatin1Char('/') + base + QLatin1Char('.') + ext;
            QFile::remove(installedIcon);
            if (!QFile::copy(icon.first, installedIcon)) {
                installedIcon.clear();
            }
        }

        const QString appsDir = QDir::homePath() + QStringLiteral("/.local/share/applications");
        QDir().mkpath(appsDir);
        const QString desktopPath = appsDir + QLatin1Char('/') + base + QStringLiteral(".desktop");
        QFile out(desktopPath);
        if (!out.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
            notify(2, QStringLiteral("Could not write %1").arg(desktopPath));
            return;
        }
        QString categories = desktopValue(desktopText, QStringLiteral("Categories"));
        if (categories.isEmpty()) categories = QStringLiteral("Utility;");
        QString entry = QStringLiteral("[Desktop Entry]\nType=Application\nName=%1\nExec=\"%2\" %U\nTerminal=false\nCategories=%3\n")
                            .arg(name, info.absoluteFilePath(), categories);
        if (!installedIcon.isEmpty()) entry += QStringLiteral("Icon=%1\n").arg(installedIcon);
        const QString comment = desktopValue(desktopText, QStringLiteral("Comment"));
        if (!comment.isEmpty()) entry += QStringLiteral("Comment=%1\n").arg(comment);
        const QString mime = desktopValue(desktopText, QStringLiteral("MimeType"));
        if (!mime.isEmpty()) entry += QStringLiteral("MimeType=%1\n").arg(mime);
        const QString wmClass = desktopValue(desktopText, QStringLiteral("StartupWMClass"));
        if (!wmClass.isEmpty()) entry += QStringLiteral("StartupWMClass=%1\n").arg(wmClass);
        out.write(entry.toUtf8());
        out.close();

        utils::runProcess(QStringLiteral("update-desktop-database"), {appsDir}); // best effort
        notify(0, QStringLiteral("'%1' was added to the application menu.").arg(name));
    });
}

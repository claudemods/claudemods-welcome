#include "backend.h"

#include "actions.h"
#include "externals.h"
#include "fluent.h"
#include "networkmanager.h"
#include "systemd.h"
#include "tweak.h"
#include "utils.h"

#include <QDebug>
#include <QDesktopServices>
#include <QDir>
#include <QEventLoop>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QProcess>
#include <QRegularExpression>
#include <QUrl>
#include <QXmlStreamReader>
#include <QtConcurrent>

namespace {

Backend *g_instance = nullptr;

struct Lang {
    const char *id;
    const char *name;
};
// Same list/order as the original language dropdown (English first)
const Lang kLanguages[] = {
    {"en", "English"},          {"af", "Afrikaans"},       {"ar", "العربية"},      {"az", "Azərbaycanca"},
    {"bg", "Български"},        {"by", "Беларуская"},      {"ca", "Català"},       {"cs", "Čeština"},
    {"de", "Deutsch"},          {"el", "Ελληνικά"},        {"es", "Español"},      {"et", "Eesti"},
    {"eu", "Euskara"},          {"fr", "Français"},        {"he", "עברית"},        {"hu", "Magyar"},
    {"hy", "Հայերեն"},          {"it", "Italiano"},        {"ja", "日本語"},       {"ka", "ქართული"},
    {"ko-KR", "한국어"},        {"lt", "Lietuvių"},        {"nb", "Norsk Bokmål"}, {"nl", "Nederlands"},
    {"pl", "Polski"},           {"pt-BR", "Português Brasileiro"}, {"pt-PT", "Português"}, {"ru", "Русский"},
    {"sk", "Slovenčina"},       {"sq", "Shqip"},           {"sv", "Svenska"},      {"tr", "Türkçe"},
    {"uk", "Українська"},       {"vi", "tiếng Việt"},      {"zh-CN", "简体中文"},  {"zh-TW", "繁體中文"},
};

QString toRichText(QString s)
{
    s.replace(QStringLiteral("\r\n"), QStringLiteral("\n"));
    s.replace(QLatin1Char('\n'), QStringLiteral("<br>"));
    return s;
}

/// Blocking HTTP GET with a bounded timeout; usable from worker threads.
bool httpGet(const QUrl &url, int timeoutMs, int *status, QByteArray *body)
{
    QNetworkAccessManager mgr;
    QNetworkRequest req(url);
    req.setTransferTimeout(timeoutMs);
    QNetworkReply *reply = mgr.get(req);
    QEventLoop loop;
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    loop.exec();
    const int code = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    if (status) {
        *status = code;
    }
    if (body) {
        *body = reply->readAll();
    }
    const bool reached = code != 0; // any HTTP answer (even 5xx) proves connectivity
    reply->deleteLater();
    return reached;
}

enum class CheckKind { Pass, Warn, Fail };
struct CheckOutcome {
    CheckKind kind = CheckKind::Pass;
    actions::MessageType type = actions::MessageType::Warning;
    QString text;
};

CheckOutcome pass() { return {}; }
CheckOutcome warn(const QString &t) { return {CheckKind::Warn, actions::MessageType::Warning, t}; }
CheckOutcome fail(actions::MessageType type, const QString &t) { return {CheckKind::Fail, type, t}; }

CheckOutcome connectivityCheck()
{
    int status = 0;
    if (httpGet(QUrl(QString::fromLatin1(ext::kConnectUrl)), 10000, &status, nullptr) &&
        ((status >= 200 && status < 300) || status >= 500)) {
        return pass();
    }
    const QStringList targets = {QStringLiteral("8.8.8.8"),         QStringLiteral("1.1.1.1"),
                                 QStringLiteral("9.9.9.9"),         QStringLiteral("2001:4860:4860::8888"),
                                 QStringLiteral("2606:4700:4700::1111"), QStringLiteral("2620:fe::fe")};
    for (const QString &target : targets) {
        if (utils::runProcess(QStringLiteral("ping"), {QStringLiteral("-c"), QStringLiteral("1"), QStringLiteral("-W"),
                                                         QStringLiteral("3"), target}) == 0) {
            qInfo() << "Connectivity confirmed via ping to" << target;
            return pass();
        }
    }
    return fail(actions::MessageType::Error, fl(QStringLiteral("offline-error")));
}

QStringList handheldProfileNames()
{
    QStringList names;
    const QString toml = utils::readTextFile(QString::fromLatin1(ext::kChwdPciConfigDir) + QStringLiteral("/handhelds/profiles.toml"));
    static const QRegularExpression tableRe(QStringLiteral(R"(^\s*\[\[?\s*([^\]\s]+)\s*\]\]?\s*$)"));
    for (const QString &line : toml.split(QLatin1Char('\n'))) {
        const auto m = tableRe.match(line);
        if (m.hasMatch()) {
            names << m.captured(1);
        }
    }
    return names;
}

CheckOutcome editionCompatCheck()
{
    const QString edition = utils::readTextFile(QStringLiteral("/etc/edition-tag")).trimmed();
    const QString editionTag = edition.isEmpty() ? QStringLiteral("desktop") : edition;

    // Best effort: needs the chwd tool and its handheld profile database. If either is missing the
    // check is skipped instead of blocking the installation.
    const QStringList handheld = handheldProfileNames();
    if (handheld.isEmpty() || utils::findExecutable(QStringLiteral("chwd")).isEmpty()) {
        qWarning() << "chwd data not available, skipping edition compatibility check";
        return pass();
    }
    const QString listing = utils::cmdOutput(QStringLiteral("chwd"), {QStringLiteral("--list-available")});
    bool supportedHandheld = false;
    static const QRegularExpression splitRe(QStringLiteral(R"([\s,>]+)"));
    for (const QString &token : listing.split(splitRe, Qt::SkipEmptyParts)) {
        if (handheld.contains(token)) {
            supportedHandheld = true;
            break;
        }
    }
    if (editionTag == QLatin1String("handheld") && !supportedHandheld) {
        return fail(actions::MessageType::Warning, fl(QStringLiteral("unsupported-hw-warning")));
    }
    if (editionTag == QLatin1String("desktop") && supportedHandheld) {
        return fail(actions::MessageType::Error, fl(QStringLiteral("desktop-on-handheld-error")));
    }
    return pass();
}

CheckOutcome outdatedVersionCheck()
{
    QString edition = utils::readTextFile(QStringLiteral("/etc/edition-tag")).trimmed();
    if (edition.isEmpty()) {
        edition = QStringLiteral("desktop");
    }
    QString version = utils::readTextFile(QStringLiteral("/etc/version-tag")).trimmed();
    if (version.isEmpty()) {
        version = QStringLiteral("testing");
    }
    if (version.contains(QStringLiteral("testing"))) {
        return warn(fl(QStringLiteral("testing-iso-warning")));
    }

    int status = 0;
    QByteArray body;
    if (!httpGet(QUrl(QString::fromLatin1(ext::kVersionsUrl)), 10000, &status, &body)) {
        return fail(actions::MessageType::Warning, fl(QStringLiteral("offline-error")));
    }
    const QJsonObject json = QJsonDocument::fromJson(body).object();
    if (json.isEmpty()) {
        qWarning() << "Failed to fetch versions.json";
        return pass(); // silently continue on server errors
    }
    const QString latest = (edition.contains(QStringLiteral("desktop")) ? json.value(QStringLiteral("desktopISOVersion"))
                                                                          : json.value(QStringLiteral("handheldISOVersion")))
                               .toString()
                               .trimmed();

    bool okCur = false;
    bool okLatest = false;
    const uint cur = version.toUInt(&okCur);
    const uint lat = latest.toUInt(&okLatest);
    if (okCur && okLatest && cur > lat) {
        return warn(fl(QStringLiteral("testing-iso-warning")));
    }
    if (version != latest) {
        return warn(fl(QStringLiteral("outdated-version-warning")));
    }
    return pass();
}

} // namespace

// ---------------------------------------------------------------------------------------------
// construction / shared helpers
// ---------------------------------------------------------------------------------------------

Backend *Backend::instance()
{
    if (!g_instance) {
        g_instance = new Backend;
    }
    return g_instance;
}

Backend *Backend::create(QQmlEngine *, QJSEngine *)
{
    QQmlEngine::setObjectOwnership(instance(), QQmlEngine::CppOwnership);
    return instance();
}

QJsonObject Backend::loadPreferences()
{
    QFile f(QStringLiteral(":/data/preferences.json"));
    if (!f.open(QIODevice::ReadOnly)) {
        qCritical() << "preferences.json missing from resources";
        return {};
    }
    return QJsonDocument::fromJson(f.readAll()).object();
}

QString Backend::savedLocale()
{
    const QJsonObject prefs = loadPreferences();
    const QString path = utils::fixPath(prefs.value(QStringLiteral("save_path")).toString());
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) {
        return {};
    }
    return QJsonDocument::fromJson(f.readAll()).object().value(QStringLiteral("locale")).toString();
}

QString Backend::bestLocale(const QString &saved)
{
    Fluent &fluent = Fluent::instance();
    const QString def = loadPreferences().value(QStringLiteral("default_locale")).toString(QStringLiteral("en"));

    if (fluent.isLanguageAvailable(saved)) {
        return saved;
    }
    if (saved == def) {
        return def;
    }

    QString sys = qEnvironmentVariable("LC_ALL");
    if (sys.isEmpty()) sys = qEnvironmentVariable("LC_MESSAGES");
    if (sys.isEmpty()) sys = qEnvironmentVariable("LANG");
    if (sys.isEmpty()) sys = QStringLiteral("en_US.UTF-8");
    sys = sys.section(QLatin1Char('.'), 0, 0).section(QLatin1Char('@'), 0, 0);

    QString normalized = sys;
    normalized.replace(QLatin1Char('_'), QLatin1Char('-'));
    if (fluent.isLanguageAvailable(normalized)) {
        return normalized;
    }
    const QString twoLetters = sys.left(2);
    if (fluent.isLanguageAvailable(twoLetters)) {
        return twoLetters;
    }
    return def;
}

void Backend::setupLocalization()
{
    Fluent::instance().select(bestLocale(savedLocale()));
}

Backend::Backend(QObject *parent)
    : QObject(parent)
{
    m_prefs = loadPreferences();
    m_locale = bestLocale(savedLocale());
    Fluent::instance().select(m_locale);

    m_isIso = QFileInfo::exists(m_prefs.value(QStringLiteral("live_path")).toString()) &&
              utils::checkRegularFile(m_prefs.value(QStringLiteral("installer_path")).toString());

    m_autostart = QFileInfo::exists(utils::fixPath(m_prefs.value(QStringLiteral("autostart_path")).toString()));

    if (!m_isIso) {
        m_hasNmcli = nm::isAvailable();
        m_hasPackageInstaller = !utils::findExecutable(QString::fromLatin1(ext::kPackageInstallerBin)).isEmpty();
        m_hasKernelManager = !utils::findExecutable(QString::fromLatin1(ext::kKernelManagerBin)).isEmpty();
        m_isKwinWayland = utils::isKwinWayland();
        m_hasVramButton = utils::hasIntelOrAmdGpu();
        systemd::refreshCache(); // warm the enabled-units cache
    }

    m_net = new QNetworkAccessManager(this);
    m_releaseText = QStringLiteral(
        "<big>Latest Release</big>\n\nRelease information is currently unavailable.\n\n"
        "<a href=\"%1\">Visit the blog for the latest release notes</a>")
                        .arg(QString::fromLatin1(ext::kBlogUrl));
    fetchReleasePage();
}

QVariantList Backend::languages() const
{
    QVariantList out;
    const Fluent &f = Fluent::instance();
    for (const Lang &l : kLanguages) {
        if (f.isLanguageAvailable(QString::fromLatin1(l.id))) {
            out.append(QVariantMap{{QStringLiteral("id"), QString::fromLatin1(l.id)},
                                   {QStringLiteral("name"), QString::fromUtf8(l.name)}});
        }
    }
    return out;
}

QString Backend::version() const
{
    return QStringLiteral(APP_VERSION);
}

QString Backend::appId() const
{
    return QString::fromLatin1(ext::kAppId);
}

QString Backend::translate(const QString &id, const QVariantMap &args) const
{
    return fl(id, args);
}

void Backend::setLocale(const QString &code)
{
    if (code == m_locale || !Fluent::instance().isLanguageAvailable(code)) {
        return;
    }
    qDebug() << "Locale changed to" << code;
    Fluent::instance().select(code);
    m_locale = code;
    emit localeChanged();
}

void Backend::saveSettings()
{
    const QString path = utils::fixPath(m_prefs.value(QStringLiteral("save_path")).toString());
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        qWarning() << "Could not save settings to" << path;
        return;
    }
    f.write(QJsonDocument(QJsonObject{{QStringLiteral("locale"), m_locale}}).toJson(QJsonDocument::Compact));
}

void Backend::openUrl(const QString &keyOrUrl)
{
    const QJsonObject urls = m_prefs.value(QStringLiteral("urls")).toObject();
    const QString url = urls.contains(keyOrUrl) ? urls.value(keyOrUrl).toString() : keyOrUrl;
    if (!QDesktopServices::openUrl(QUrl(url))) {
        qWarning() << "Failed to open uri:" << url;
    }
}

QString Backend::pageText(const QString &name) const
{
    if (name == QLatin1String("release")) {
        return toRichText(m_releaseText);
    }
    auto read = [&](const QString &loc) -> QString {
        QFile f(QStringLiteral(":/data/pages/%1/%2").arg(loc, name));
        if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
            return {};
        }
        return QString::fromUtf8(f.readAll());
    };
    QString text = read(m_locale);
    if (text.isEmpty()) {
        text = read(QStringLiteral("en"));
    }
    return toRichText(text);
}

void Backend::setAutostart(bool enabled)
{
    const QString autostartPath = utils::fixPath(m_prefs.value(QStringLiteral("autostart_path")).toString());
    const QString desktopPath = m_prefs.value(QStringLiteral("desktop_path")).toString();
    QDir().mkpath(QFileInfo(autostartPath).absolutePath());
    if (enabled && !utils::checkRegularFile(autostartPath)) {
        QFile::remove(autostartPath); // drop a dangling link
        if (!QFile::link(desktopPath, autostartPath)) {
            qWarning() << "Failed to create autostart link" << autostartPath;
        }
    } else if (!enabled && QFileInfo(autostartPath).isSymLink()) {
        QFile::remove(autostartPath);
    } else if (!enabled && utils::checkRegularFile(autostartPath)) {
        QFile::remove(autostartPath);
    }
    m_autostart = QFileInfo::exists(autostartPath);
    emit autostartChanged();
}

void Backend::emitMessage(int type, const QString &text, const QString &title)
{
    emit message(type, text, title.isEmpty() ? actions::messageTypeName(static_cast<actions::MessageType>(type)) : title);
}

bool Backend::guiRunCommand(const QString &command, bool escalate)
{
    const QString scripts = QString::fromLatin1(ext::kScriptsDir);
    QStringList args;
    if (escalate) {
        args << QStringLiteral("-s") << QStringLiteral("pkexec %1/rootshell.sh").arg(scripts);
    }
    args << command + QStringLiteral("; read -p 'Press enter to exit'");
    return utils::runProcess(scripts + QStringLiteral("/terminal-helper"), args) == 0;
}

void Backend::setInstallerBusy(bool busy)
{
    QMetaObject::invokeMethod(
        this,
        [this, busy]() {
            if (m_installerBusy != busy) {
                m_installerBusy = busy;
                emit installerBusyChanged();
            }
        },
        Qt::QueuedConnection);
}

// ---------------------------------------------------------------------------------------------
// release page (latest blog post)
// ---------------------------------------------------------------------------------------------

void Backend::fetchReleasePage()
{
    QNetworkRequest req{QUrl(QString::fromLatin1(ext::kRssUrl))};
    req.setTransferTimeout(10000);
    QNetworkReply *reply = m_net->get(req);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            qWarning() << "RSS fetch failed:" << reply->errorString();
            return;
        }
        QXmlStreamReader xml(reply->readAll());
        bool inItem = false;
        bool found = false;
        QString cur, title, link, desc, date;
        while (!xml.atEnd()) {
            const auto token = xml.readNext();
            if (token == QXmlStreamReader::StartElement) {
                const QString n = xml.name().toString();
                if (n == QLatin1String("item")) {
                    inItem = true;
                    title.clear();
                    link.clear();
                    desc.clear();
                    date.clear();
                } else if (inItem) {
                    cur = n;
                }
            } else if ((token == QXmlStreamReader::Characters) && inItem && !xml.isWhitespace()) {
                if (cur == QLatin1String("title")) title += xml.text().toString();
                else if (cur == QLatin1String("link")) link += xml.text().toString();
                else if (cur == QLatin1String("description")) desc += xml.text().toString();
                else if (cur == QLatin1String("pubDate")) date += xml.text().toString();
            } else if (token == QXmlStreamReader::EndElement) {
                if (xml.name() == QLatin1String("item")) {
                    if (title.toLower().contains(QStringLiteral("release"))) {
                        found = true;
                        break;
                    }
                    inItem = false;
                }
                cur.clear();
            }
        }
        if (!found) {
            qWarning() << "No release item found in RSS feed";
            return;
        }
        m_releaseText = QStringLiteral("<big>%1</big>\n\n%2\n\n<i>%3</i>\n\n<a href=\"%4\">For more details</a>")
                            .arg(title.toHtmlEscaped(), desc.toHtmlEscaped(), date.toHtmlEscaped(), link);
        emit releaseTextChanged();
    });
}

// ---------------------------------------------------------------------------------------------
// installer / apps
// ---------------------------------------------------------------------------------------------

void Backend::launchInstaller()
{
    if (m_installerBusy) {
        return;
    }
    m_installerBusy = true;
    emit installerBusyChanged();

    (void)QtConcurrent::run([this]() {
        const QString title = fl(QStringLiteral("calamares-install-type"));
        const std::function<CheckOutcome()> checks[] = {connectivityCheck, editionCompatCheck, outdatedVersionCheck};
        for (const auto &check : checks) {
            const CheckOutcome r = check();
            if (r.kind == CheckKind::Warn) {
                emitMessage(int(actions::MessageType::Warning), r.text, title);
            } else if (r.kind == CheckKind::Fail) {
                emitMessage(int(r.type), r.text, title);
                qInfo() << "Some ISO check failed!";
                setInstallerBusy(false);
                return;
            }
        }
        qInfo() << "ISO checks passed! Starting installer..";

        QProcess proc;
        proc.setProcessChannelMode(QProcess::MergedChannels);
        proc.start(QString::fromLatin1(ext::kInstallerScript), {});
        if (!proc.waitForStarted()) {
            qCritical() << "Failed to spawn installer";
            setInstallerBusy(false);
            return;
        }
        while (proc.state() != QProcess::NotRunning) {
            proc.waitForReadyRead(500);
            while (proc.canReadLine()) {
                qInfo().noquote() << QString::fromUtf8(proc.readLine()).trimmed();
            }
        }
        qInfo() << "Installer finished with exit code" << proc.exitCode();
        setInstallerBusy(false);
    });
}

void Backend::launchApp(const QString &which)
{
    const QString bin = which == QLatin1String("kernel-manager") ? QString::fromLatin1(ext::kKernelManagerBin)
                                                                 : QString::fromLatin1(ext::kPackageInstallerBin);
    const QString path = utils::findExecutable(bin);
    if (path.isEmpty()) {
        return;
    }
    (void)QtConcurrent::run([path]() { qDebug() << "launched" << path << utils::spawnDetached(path); });
}

void Backend::launchPackageBrowser()
{
    launchApp(QStringLiteral("package-installer"));
}

// ---------------------------------------------------------------------------------------------
// tweaks / utilities
// ---------------------------------------------------------------------------------------------

QVariantList Backend::tweakList() const
{
    QVariantList out;
    for (tweak::Name t : tweak::all()) {
        out.append(QVariantMap{{QStringLiteral("id"), tweak::id(t)},
                               {QStringLiteral("label"), tweak::label(t)},
                               {QStringLiteral("tooltipId"), tweak::tooltipId(t)}});
    }
    return out;
}

bool Backend::tweakEnabled(const QString &id) const
{
    tweak::Name t;
    if (!tweak::fromId(id, &t)) {
        return false;
    }
    return systemd::checkAnyUnits(tweak::details(t).units) || tweak::checkAutostartActive(t);
}

void Backend::toggleTweak(const QString &id)
{
    tweak::Name t;
    if (!tweak::fromId(id, &t)) {
        return;
    }
    (void)QtConcurrent::run([this, t, id]() {
        const tweak::Details d = tweak::details(t);
        const bool enabled = d.type == QLatin1String("user_service") ? systemd::checkUserUnits(d.units)
                                                                       : systemd::checkSystemUnits(d.units);
        const bool ok = actions::toggleTweak(t, !enabled, [this](const QString &c, bool e) { return guiRunCommand(c, e); });
        if (!ok) {
            emitMessage(int(actions::MessageType::Error),
                        fl(QStringLiteral("package-not-installed"), {{QStringLiteral("package_name"), d.pkg}}),
                        QStringLiteral("Error"));
        }
        QMetaObject::invokeMethod(
            this,
            [this, id, d, ok]() {
                ++m_tweaksRevision;
                emit tweaksChanged();
                if (!ok) {
                    emit tweakFailed(id, d.pkg);
                }
            },
            Qt::QueuedConnection);
    });
}

void Backend::runAction(const QString &id)
{
    (void)QtConcurrent::run([this, id]() {
        const actions::RunCmdCallback run = [this](const QString &c, bool e) { return guiRunCommand(c, e); };
        const actions::MessageSink sink = [this](actions::MessageType t, const QString &m) { emitMessage(int(t), m); };

        if (id == QLatin1String("update-system")) actions::updateSystem(run);
        else if (id == QLatin1String("remove-orphans")) actions::removeOrphans(run, sink);
        else if (id == QLatin1String("rankmirrors")) actions::rankMirrors(run);
        else if (id == QLatin1String("install-gaming")) actions::installGaming(run, sink);
        else if (id == QLatin1String("install-winboat")) actions::installWinboat(run, sink);
        else if (id == QLatin1String("install-vram-management")) actions::installVramManagement(run, sink);
        else if (id == QLatin1String("remove-lock")) actions::removeDbLock(sink);
        else if (id == QLatin1String("reinstall")) actions::reinstallPackages(run);
        else if (id == QLatin1String("reset-keyrings")) actions::resetKeyring(run);
        else if (id == QLatin1String("clear-pkgcache")) actions::clearPkgCache(run);
        else if (id == QLatin1String("kwin-debug")) actions::launchKwinDebugWindow();
        else if (id == QLatin1String("christitus")) actions::launchChrisTitusTool(run);
        else qWarning() << "unknown action" << id;
    });
}

// ---------------------------------------------------------------------------------------------
// DNS
// ---------------------------------------------------------------------------------------------

QVariantList Backend::dnsServers() const
{
    QVariantList out;
    for (const dns::Server &s : dns::servers()) {
        out.append(QVariantMap{{QStringLiteral("name"), s.name},
                               {QStringLiteral("region"), s.region},
                               {QStringLiteral("homepage"), s.homepage},
                               {QStringLiteral("dot"), !s.dotHost.isEmpty()},
                               {QStringLiteral("doh"), !s.dohUrl.isEmpty()},
                               {QStringLiteral("doq"), !s.doqEndpoint.isEmpty()},
                               {QStringLiteral("filtering"), s.filtering}});
    }
    return out;
}

QStringList Backend::nmConnections() const
{
    return nm::connections();
}

QString Backend::activeConnection() const
{
    return nm::activeConnectionName();
}

int Backend::selectionIndexForConnection(const QString &conn) const
{
    const auto &servers = dns::servers();
    const int n = servers.size();

    // blocky (DoH/DoQ mode) pointing at 127.0.0.1: read the preset from its config
    if (actions::isBlockyActive()) {
        const auto info = nm::dnsForConnection(conn);
        const bool pointsToBlocky =
            info && (info->ipv4.contains(QStringLiteral("127.0.0.1")) || info->ipv6.contains(QStringLiteral("::1")));
        if (pointsToBlocky) {
            if (const auto active = dns::readActiveBlocky()) {
                const int i = dns::findServerByBlockyUpstream(active->second, active->first);
                return i >= 0 ? i : n;
            }
        }
    }

    if (const auto info = nm::dnsForConnection(conn)) {
        for (int i = 0; i < n; ++i) {
            if ((!info->ipv4.isEmpty() && info->ipv4 == servers.at(i).ipv4) ||
                (!info->ipv6.isEmpty() && info->ipv6 == servers.at(i).ipv6)) {
                return i;
            }
        }
        return n; // set, but not a preset: custom
    }
    return -1; // automatic (DHCP)
}

dns::Type Backend::presetTypeForServer(int index) const
{
    const auto &servers = dns::servers();
    if (index < 0 || index >= servers.size()) {
        return dns::Type::Plain;
    }
    const dns::Server &s = servers.at(index);
    if (actions::isBlockyActive()) {
        if (const auto active = dns::readActiveBlocky()) {
            if (!dns::blockyUpstream(s, active->first).isEmpty()) {
                return active->first == dns::BlockyMode::Doh ? dns::Type::Doh : dns::Type::Doq;
            }
        }
        return dns::Type::Plain;
    }
    return s.dotHost.isEmpty() ? dns::Type::Plain : dns::Type::Dot;
}

QVariantMap Backend::dnsState(const QString &connection) const
{
    const int n = dns::servers().size();
    const int sel = selectionIndexForConnection(connection);

    QVariantMap out{{QStringLiteral("serverIndex"), sel < 0 ? n + 1 : sel},
                    {QStringLiteral("type"), QStringLiteral("plain")},
                    {QStringLiteral("ipv4"), QString()},
                    {QStringLiteral("ipv6"), QString()},
                    {QStringLiteral("dotHost"), QString()},
                    {QStringLiteral("dohUrl"), QString()},
                    {QStringLiteral("doqEndpoint"), QString()}};

    if (sel < 0) {
        return out; // DHCP: nothing to choose
    }
    if (sel < n) {
        out[QStringLiteral("type")] = dns::typeId(presetTypeForServer(sel));
        return out;
    }

    // custom: prefill the fields with the current values
    dns::Type type = dns::Type::Plain;
    if (actions::isBlockyActive()) {
        if (const auto active = dns::readActiveBlocky()) {
            if (active->first == dns::BlockyMode::Doh) {
                out[QStringLiteral("dohUrl")] = active->second;
                type = dns::Type::Doh;
            } else {
                out[QStringLiteral("doqEndpoint")] = active->second;
                type = dns::Type::Doq;
            }
        }
        const dns::BlockyBootstrap b = dns::readBlockyBootstrap();
        out[QStringLiteral("ipv4")] = b.ipv4;
        out[QStringLiteral("ipv6")] = b.ipv6;
        out[QStringLiteral("dotHost")] = b.dotHostname;
    } else {
        if (const auto info = nm::dnsForConnection(connection)) {
            out[QStringLiteral("ipv4")] = info->ipv4;
            out[QStringLiteral("ipv6")] = info->ipv6;
            out[QStringLiteral("dotHost")] = info->dotHostname;
        }
        type = nm::dotForConnection(connection) ? dns::Type::Dot : dns::Type::Plain;
    }
    out[QStringLiteral("type")] = dns::typeId(type);
    return out;
}

void Backend::applyDns(const QVariantMap &p)
{
    const auto &servers = dns::servers();
    const int n = servers.size();
    const QString conn = p.value(QStringLiteral("connection")).toString();
    const int index = p.value(QStringLiteral("serverIndex")).toInt();
    const bool isCustom = index == n;
    const bool isDhcp = index == n + 1;

    const actions::MessageSink sink = [this](actions::MessageType t, const QString &m) { emitMessage(int(t), m); };
    const actions::RunCmdCallback run = [this](const QString &c, bool e) { return guiRunCommand(c, e); };

    if (isDhcp) {
        (void)QtConcurrent::run([=]() { actions::resetDnsServer(conn, sink); });
        return;
    }

    const dns::Type type = dns::typeFromId(p.value(QStringLiteral("type")).toString());
    const bool enableDot = type == dns::Type::Dot;

    QString ipv4;
    QString ipv6;
    QString dotHostname;
    if (isCustom) {
        ipv4 = p.value(QStringLiteral("ipv4")).toString().trimmed();
        ipv6 = p.value(QStringLiteral("ipv6")).toString().trimmed();
        dotHostname = p.value(QStringLiteral("dotHost")).toString().trimmed();
        const bool v4ok = ipv4.isEmpty() || dns::isValidDnsInput(ipv4);
        const bool v6ok = ipv6.isEmpty() || dns::isValidDnsInput(ipv6);
        if ((ipv4.isEmpty() && ipv6.isEmpty()) || !v4ok || !v6ok) {
            emitMessage(int(actions::MessageType::Error), fl(QStringLiteral("custom-dns-invalid")));
            return;
        }
        if (!dotHostname.isEmpty() && !dns::isValidDotHostname(dotHostname)) {
            emitMessage(int(actions::MessageType::Error), fl(QStringLiteral("custom-dns-invalid-hostname")));
            return;
        }
    } else {
        if (index < 0 || index >= n) {
            return;
        }
        const dns::Server &s = servers.at(index);
        ipv4 = s.ipv4;
        ipv6 = s.ipv6;
        dotHostname = s.dotHost;
    }

    if (type == dns::Type::Doh || type == dns::Type::Doq) {
        const dns::BlockyMode mode = type == dns::Type::Doh ? dns::BlockyMode::Doh : dns::BlockyMode::Doq;
        QString upstream;
        if (isCustom) {
            upstream = p.value(mode == dns::BlockyMode::Doh ? QStringLiteral("dohUrl") : QStringLiteral("doqEndpoint"))
                           .toString()
                           .trimmed();
            const bool valid = mode == dns::BlockyMode::Doh ? dns::isValidDohUrl(upstream) : dns::isValidDoqEndpoint(upstream);
            if (!valid) {
                emitMessage(int(actions::MessageType::Error),
                            fl(mode == dns::BlockyMode::Doh ? QStringLiteral("custom-dns-doh-url-required")
                                                            : QStringLiteral("custom-dns-doq-endpoint-required")));
                return;
            }
        } else {
            upstream = dns::blockyUpstream(servers.at(index), mode);
        }
        (void)QtConcurrent::run(
            [=]() { actions::changeDnsServerBlocky(run, conn, mode, upstream, ipv4, ipv6, dotHostname, sink); });
    } else {
        (void)QtConcurrent::run([=]() {
            // Stop blocky when switching away from encrypted DNS
            actions::stopBlocky();
            actions::changeDnsServer(conn, ipv4, ipv6, enableDot, dotHostname, sink);
        });
    }
}

void Backend::resetDns(const QString &connection)
{
    const actions::MessageSink sink = [this](actions::MessageType t, const QString &m) { emitMessage(int(t), m); };
    (void)QtConcurrent::run([=]() { actions::resetDnsServer(connection, sink); });
}

void Backend::testLatency(const QVariantMap &p)
{
    const auto &servers = dns::servers();
    QString ipv4;
    dns::LatencyProbe probe;
    if (p.value(QStringLiteral("custom")).toBool()) {
        ipv4 = p.value(QStringLiteral("ipv4")).toString().trimmed();
        if (ipv4.isEmpty()) {
            emit latencyResult(-1);
            return;
        }
    } else {
        const int idx = p.value(QStringLiteral("serverIndex")).toInt();
        if (idx < 0 || idx >= servers.size()) {
            emit latencyResult(-1);
            return;
        }
        ipv4 = servers.at(idx).ipv4;
        // probe the selected transport, otherwise a DoT/DoH figure would be off by the handshake
        probe = dns::probeForServer(servers.at(idx), dns::typeFromId(p.value(QStringLiteral("type")).toString()));
    }
    (void)QtConcurrent::run([this, ipv4, probe]() {
        const auto r = dns::measureLatency(ipv4, probe);
        emit latencyResult(r ? *r : -1);
    });
}

void Backend::bestServer()
{
    (void)QtConcurrent::run([this]() {
        const auto results = dns::measureAllLatencies();
        const auto &servers = dns::servers();
        for (const auto &[idx, ms] : results) {
            if (ms && !servers.at(idx).filtering) {
                emit bestServerResult(idx, *ms);
                return;
            }
        }
        emit bestServerResult(-1, -1);
    });
}

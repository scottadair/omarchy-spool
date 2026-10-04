// Theme watching follows Omacut (MIT, David Heinemeier Hansson).
#include "theme.h"

#include <QColor>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QRegularExpression>
#include <cmath>

Theme::Theme(const QString &directory, QObject *parent)
    : QObject(parent),
      m_directory(directory.isEmpty() ? QDir::homePath() + "/.local/state/omarchy/current" : directory) {
    m_colors = readColors({});
    m_debounce.setSingleShot(true);
    m_debounce.setInterval(80);
    connect(&m_watcher, &QFileSystemWatcher::directoryChanged, this, [this] { m_debounce.start(); });
    connect(&m_watcher, &QFileSystemWatcher::fileChanged, this, [this] { m_debounce.start(); });
    connect(&m_debounce, &QTimer::timeout, this, &Theme::reload);
    reload();
}

QString Theme::readAccent(const QString &path) {
    QFile file(path);
    if (file.open(QIODevice::ReadOnly)) {
        const QRegularExpression expression(R"re(^\s*accent\s*=\s*["'](#[0-9a-fA-F]{6})["'])re",
                                            QRegularExpression::MultilineOption);
        const auto match = expression.match(QString::fromUtf8(file.readAll()));
        if (match.hasMatch())
            return match.captured(1);
    }
    return "#FFD60A";
}

QVariantMap Theme::readColors(const QString &path) {
    // Used when Omarchy isn't there or a key is missing.
    QVariantMap colors = {
        {"mode", "dark"},
        {"accent", "#FFD60A"},
        {"selection", "#26262b"},
        {"muted", "#3a3a42"},
        {"background", "#0e0e10"},
        {"dark_background", "#0a0a0c"},
        {"darker_background", "#070709"},
        {"lighter_background", "#17171a"},
        {"foreground", "#c8c8d0"},
        {"dark_foreground", "#80808a"},
        {"light_foreground", "#d8d8e0"},
        {"bright_foreground", "#ffffff"},
        {"red", "#ff6b6b"},
        {"yellow", "#f0c060"},
        {"orange", "#f0a030"},
        {"green", "#5fd08a"},
        {"cyan", "#5fc8d0"},
        {"blue", "#6aa0ff"},
        {"magenta", "#c08aff"},
    };
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return colors;
    const QRegularExpression line(R"re(^\s*([a-z_0-9]+)\s*=\s*["'](#[0-9a-fA-F]{6}|dark|light)["'])re",
                                  QRegularExpression::MultilineOption);
    auto it = line.globalMatch(QString::fromUtf8(file.readAll()));
    while (it.hasNext()) {
        const auto m = it.next();
        colors.insert(m.captured(1), m.captured(2));
    }
    return colors;
}

// Black or white, whichever reads on the accent (WCAG relative luminance).
QString Theme::accentForeground() const {
    const QColor c(m_accent);
    auto linear = [](double v) { return v <= .04045 ? v / 12.92 : std::pow((v + .055) / 1.055, 2.4); };
    const double luminance = .2126 * linear(c.redF()) + .7152 * linear(c.greenF()) + .0722 * linear(c.blueF());
    return luminance > .179 ? "black" : "white";
}

void Theme::reload() {
    const auto paths = m_watcher.files() + m_watcher.directories();
    if (!paths.isEmpty())
        m_watcher.removePaths(paths);

    // A theme switch replaces symlinks and files, so watch the parents too and
    // re-arm after every change. Watching the nearest existing ancestor means
    // installing Omarchy later works without restarting the app.
    QString ancestor = m_directory;
    while (!QFileInfo::exists(ancestor) && ancestor != "/")
        ancestor = QFileInfo(ancestor).absolutePath();
    QStringList candidates{ancestor, QFileInfo(ancestor).absolutePath(), m_directory,
                           m_directory + "/theme", m_directory + "/theme/colors.toml"};
    candidates.removeDuplicates();
    for (const auto &path : candidates)
        if (QFileInfo::exists(path))
            m_watcher.addPath(path);

    const auto colors = readColors(m_directory + "/theme/colors.toml");
    const auto accent = colors.value("accent").toString();

    // Asked on every reload because a font change lands in the same flurry of
    // file writes as a theme change; the lookup is a few milliseconds.
    QString font = m_fontFamily;
    if (m_directory.endsWith("/.local/state/omarchy/current")) {
        QProcess process;
        process.start("omarchy-font-current", {});
        if (process.waitForFinished(1500) && process.exitCode() == 0) {
            const auto name = QString::fromUtf8(process.readAllStandardOutput()).trimmed();
            if (!name.isEmpty())
                font = name;
        }
    }

    if (accent != m_accent || colors != m_colors || font != m_fontFamily) {
        m_accent = accent;
        m_colors = colors;
        m_fontFamily = font;
        emit changed();
    }
}

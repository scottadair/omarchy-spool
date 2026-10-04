#pragma once

#include <QFileSystemWatcher>
#include <QObject>
#include <QTimer>
#include <QVariantMap>

class Theme : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString accent READ accent NOTIFY changed)
    Q_PROPERTY(QString accentForeground READ accentForeground NOTIFY changed)
    // Every color of the current Omarchy theme by its colors.toml key
    // (background, foreground, selection, muted, red, green, ...), with a
    // built-in dark palette filling any gaps.
    Q_PROPERTY(QVariantMap colors READ colors NOTIFY changed)
    Q_PROPERTY(QString mode READ mode NOTIFY changed)
    // The terminal font, so the app looks like the TUIs next to it.
    Q_PROPERTY(QString fontFamily READ fontFamily NOTIFY changed)

public:
    explicit Theme(const QString &currentDirectory = {}, QObject *parent = nullptr);

    QString accent() const { return m_accent; }
    QString accentForeground() const;
    QVariantMap colors() const { return m_colors; }
    QString mode() const { return m_colors.value("mode").toString(); }
    QString fontFamily() const { return m_fontFamily; }

    static QString readAccent(const QString &path);
    static QVariantMap readColors(const QString &path);

signals:
    void changed();

private:
    void reload();

    QString m_directory;
    QString m_accent = "#FFD60A";
    QVariantMap m_colors;
    QString m_fontFamily = "monospace";
    QFileSystemWatcher m_watcher;
    QTimer m_debounce;
};

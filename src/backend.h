#pragma once

#include <QObject>
#include <QTimer>
#include <QVariantList>
#include <QVariantMap>
#include <functional>
#include <memory>

#include "admin.h"
#include "cupsdata.h"
#include "rowmodel.h"

class QProcess;

class Backend : public QObject {
    Q_OBJECT
    Q_PROPERTY(RowModel *printers READ printers CONSTANT)
    Q_PROPERTY(RowModel *jobs READ jobs CONSTANT)
    Q_PROPERTY(RowModel *discovered READ discovered CONSTANT)
    Q_PROPERTY(QString selectedPrinter READ selectedPrinter NOTIFY selectionChanged)
    Q_PROPERTY(QVariantMap selected READ selected NOTIFY selectionChanged)
    Q_PROPERTY(QVariantList options READ options NOTIFY optionsChanged)
    Q_PROPERTY(QString message READ message NOTIFY messageChanged)
    Q_PROPERTY(bool messageIsError READ messageIsError NOTIFY messageChanged)
    Q_PROPERTY(QString connectionError READ connectionError NOTIFY connectionErrorChanged)
    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)
    Q_PROPERTY(bool discovering READ discovering NOTIFY discoveringChanged)
    Q_PROPERTY(bool loaded READ loaded NOTIFY selectionChanged)

public:
    // `admin` is owned by the backend. With `poll` off nothing talks to cupsd on
    // its own, which is what the tests want.
    explicit Backend(Admin *admin = nullptr, bool poll = true, QObject *parent = nullptr);

    RowModel *printers() { return &m_printers; }
    RowModel *jobs() { return &m_jobs; }
    RowModel *discovered() { return &m_discovered; }
    QString selectedPrinter() const { return m_selected; }
    QVariantMap selected() const { return m_selectedMap; }
    QVariantList options() const { return m_options; }
    QString message() const { return m_message; }
    bool messageIsError() const { return m_messageIsError; }
    QString connectionError() const { return m_connectionError; }
    bool busy() const { return m_busy > 0; }
    bool discovering() const { return m_discovering; }
    bool loaded() const { return m_loaded; }

    // Takes data fetched elsewhere (the worker thread, or a test) and updates every model.
    void applySnapshot(const Snapshot &snapshot);

    Q_INVOKABLE void select(const QString &name);
    Q_INVOKABLE void refresh();
    Q_INVOKABLE void discover();
    Q_INVOKABLE QVariantMap checkAddress(const QString &text) const;
    Q_INVOKABLE QString checkName(const QString &name) const;
    Q_INVOKABLE QString suggestName(const QString &display) const;
    Q_INVOKABLE void addPrinter(const QString &uri, const QString &name, const QString &info, bool makeDefault);
    Q_INVOKABLE void setDefault();
    Q_INVOKABLE void printTestPage();
    Q_INVOKABLE void renamePrinter(const QString &newName);
    Q_INVOKABLE void removePrinter();
    Q_INVOKABLE void togglePaused();
    Q_INVOKABLE void toggleAccepting();
    Q_INVOKABLE void setOption(const QString &key, const QString &value);
    Q_INVOKABLE void cancelJob(int id);
    Q_INVOKABLE void restartJob(int id);
    Q_INVOKABLE void dismissMessage();

signals:
    void selectionChanged();
    void optionsChanged();
    void messageChanged();
    void connectionErrorChanged();
    void busyChanged();
    void discoveringChanged();
    void printerAdded(const QString &name);

private:
    struct AdminCall {
        QString method;
        QVariantList args;
    };
    struct AdminJob {
        QList<AdminCall> steps;
        QString success;
        std::function<void()> onOk;
    };

    void runAdmin(const QString &success, const QList<AdminCall> &steps, std::function<void()> onOk = {});
    void runStep(std::shared_ptr<AdminJob> job, int index);
    void notify(const QString &text, bool error);
    void changeBusy(int delta);
    void rebuildSelection();
    const PrinterInfo *find(const QString &name) const;
    void ownJobAction(int id, bool restart);

    Admin *m_admin;
    RowModel m_printers;
    RowModel m_jobs;
    RowModel m_discovered;
    QList<PrinterInfo> m_data;
    QList<JobInfo> m_allJobs;
    QString m_selected;
    QVariantMap m_selectedMap;
    QVariantList m_options;
    QString m_message;
    bool m_messageIsError = false;
    QString m_connectionError;
    int m_busy = 0;
    bool m_discovering = false;
    bool m_loaded = false;
    bool m_fetching = false;
    bool m_autoRefresh;
    QTimer m_poll;
    QTimer m_messageTimer;
};

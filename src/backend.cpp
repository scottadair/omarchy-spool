#include "backend.h"

#include <QDateTime>
#include <QFutureWatcher>
#include <QProcess>
#include <QUrl>
#include <QtConcurrent>

#include "printeraddress.h"

namespace {

QString when(qint64 secs) {
    return secs > 0 ? QDateTime::fromSecsSinceEpoch(secs).toString("MMM d, HH:mm") : QString();
}

// The IPP "sides" keyword and the PPD Duplex choice have to agree: with a PPD
// in play cupsd takes the PPD default, and a lone "sides" default is ignored
// (observed on an IPP Everywhere queue).
QString duplexChoice(const QString &sides) {
    if (sides == "two-sided-long-edge") return "DuplexNoTumble";
    if (sides == "two-sided-short-edge") return "DuplexTumble";
    return "None";
}

} // namespace

Backend::Backend(Admin *admin, bool poll, QObject *parent)
    : QObject(parent),
      m_admin(admin ? admin : new PkHelperAdmin(this)),
      m_autoRefresh(poll),
      m_printers({"name", "info", "stateText", "state", "reasons", "severity", "isDefault", "enabled", "accepting",
                  "activeJobs"},
                 "name", this),
      m_jobs({"id", "name", "user", "state", "stateText", "active", "reasons", "created", "finished", "kbytes"}, "id",
             this),
      m_discovered({"uri", "name", "added"}, "uri", this) {
    m_admin->setParent(this);
    m_messageTimer.setSingleShot(true);
    m_messageTimer.setInterval(9000);
    connect(&m_messageTimer, &QTimer::timeout, this, &Backend::dismissMessage);

    if (poll) {
        m_poll.setInterval(4000);
        connect(&m_poll, &QTimer::timeout, this, &Backend::refresh);
        m_poll.start();
        refresh();
    }
}

const PrinterInfo *Backend::find(const QString &name) const {
    for (const auto &p : m_data)
        if (p.name == name)
            return &p;
    return nullptr;
}

void Backend::notify(const QString &text, bool error) {
    m_message = text;
    m_messageIsError = error;
    emit messageChanged();
    // Errors stay until dismissed; they are the thing the user must read.
    if (error || text.isEmpty())
        m_messageTimer.stop();
    else
        m_messageTimer.start();
}

void Backend::dismissMessage() {
    if (!m_message.isEmpty())
        notify({}, false);
}

void Backend::changeBusy(int delta) {
    const bool before = busy();
    m_busy += delta;
    if (before != busy())
        emit busyChanged();
}

void Backend::refresh() {
    if (m_fetching || !m_autoRefresh)
        return;
    m_fetching = true;
    auto *watcher = new QFutureWatcher<Snapshot>(this);
    connect(watcher, &QFutureWatcher<Snapshot>::finished, this, [this, watcher] {
        m_fetching = false;
        applySnapshot(watcher->result());
        watcher->deleteLater();
    });
    watcher->setFuture(QtConcurrent::run(&CupsData::fetchSnapshot));
}

void Backend::applySnapshot(const Snapshot &snap) {
    if (snap.error != m_connectionError) {
        m_connectionError = snap.error;
        emit connectionErrorChanged();
    }
    if (!snap.error.isEmpty())
        return;

    m_data = snap.printers;
    m_allJobs = snap.jobs;
    const bool wasLoaded = m_loaded;
    m_loaded = true;

    QHash<QString, int> active;
    for (const auto &j : m_allJobs)
        if (j.active())
            active[j.printer]++;

    QList<QVariantMap> rows;
    for (const auto &p : m_data)
        rows.append({{"name", p.name},
                     {"info", p.info},
                     {"stateText", p.stateText()},
                     {"state", p.state},
                     {"reasons", p.reasons.join(" · ")},
                     {"severity", p.severity},
                     {"isDefault", p.isDefault},
                     {"enabled", p.enabled()},
                     {"accepting", p.accepting},
                     {"activeJobs", active.value(p.name)}});
    m_printers.setRows(rows);

    // Keep the selection when the printer still exists, otherwise fall back to
    // the default printer, then the first one.
    if (!find(m_selected)) {
        m_selected.clear();
        for (const auto &p : m_data)
            if (p.isDefault)
                m_selected = p.name;
        if (m_selected.isEmpty() && !m_data.isEmpty())
            m_selected = m_data.first().name;
    }
    rebuildSelection();
    if (!wasLoaded)
        emit selectionChanged();
}

void Backend::rebuildSelection() {
    QVariantMap selectedMap;
    QVariantList options;
    QList<QVariantMap> jobRows;

    if (const auto *p = find(m_selected)) {
        selectedMap = {{"name", p->name},         {"info", p->info},         {"location", p->location},
                         {"makeModel", p->makeModel}, {"deviceUri", p->deviceUri}, {"stateText", p->stateText()},
                         {"reasons", p->reasons},   {"severity", p->severity}, {"isDefault", p->isDefault},
                         {"enabled", p->enabled()}, {"accepting", p->accepting}};
        for (const auto &o : p->options) {
            QVariantList values;
            for (const auto &v : o.values)
                values.append(QVariantMap{{"value", v.value}, {"label", v.label}});
            options.append(QVariantMap{{"key", o.key}, {"label", o.label}, {"values", values}, {"current", o.current}});
        }

        QList<JobInfo> mine;
        for (const auto &j : m_allJobs)
            if (j.printer == p->name)
                mine.append(j);
        // Waiting work in queue order first, then history newest first.
        std::stable_sort(mine.begin(), mine.end(), [](const JobInfo &a, const JobInfo &b) {
            if (a.active() != b.active())
                return a.active();
            return a.active() ? a.id < b.id : a.id > b.id;
        });
        for (const auto &j : mine)
            jobRows.append({{"id", j.id},
                            {"name", j.name},
                            {"user", j.user},
                            {"state", j.state},
                            {"stateText", j.stateText()},
                            {"active", j.active()},
                            {"reasons", j.reasons},
                            {"created", when(j.created)},
                            {"finished", when(j.finished)},
                            {"kbytes", j.kbytes}});
    }
    m_jobs.setRows(jobRows);

    // Only announce real changes: QML rebuilds the option combo boxes on
    // optionsChanged, which would close an open popup on every poll.
    if (selectedMap != m_selectedMap) {
        m_selectedMap = selectedMap;
        emit selectionChanged();
    }
    if (options != m_options) {
        m_options = options;
        emit optionsChanged();
    }
}

void Backend::select(const QString &name) {
    if (name == m_selected)
        return;
    m_selected = name;
    rebuildSelection();
}

// ---- admin plumbing -------------------------------------------------------

void Backend::runAdmin(const QString &success, const QList<AdminCall> &steps, std::function<void()> onOk) {
    auto job = std::make_shared<AdminJob>(AdminJob{steps, success, std::move(onOk)});
    changeBusy(+1);
    notify("Waiting for authorization…", false);
    runStep(job, 0);
}

void Backend::runStep(std::shared_ptr<AdminJob> job, int index) {
    if (index >= job->steps.size()) {
        changeBusy(-1);
        notify(job->success, false);
        if (job->onOk)
            job->onOk();
        refresh();
        return;
    }
    const auto &step = job->steps.at(index);
    m_admin->call(step.method, step.args, [this, job, index](const AdminResult &r) {
        if (!r.ok()) {
            changeBusy(-1);
            notify(r.message, true);
            refresh(); // an earlier step may already have taken effect
            return;
        }
        runStep(job, index + 1);
    });
}

// ---- actions --------------------------------------------------------------

QVariantMap Backend::checkAddress(const QString &text) const {
    const auto r = PrinterAddress::normalize(text);
    return {{"ok", r.ok}, {"uri", r.uri}, {"error", r.error}};
}

QString Backend::checkName(const QString &name) const {
    const QString problem = PrinterAddress::validateName(name);
    if (!problem.isEmpty())
        return problem;
    return find(name) ? "A printer with that name already exists." : QString();
}

QString Backend::suggestName(const QString &display) const {
    const QString base = PrinterAddress::sanitizeName(display);
    QString name = base;
    for (int i = 2; find(name); ++i)
        name = base + "-" + QString::number(i);
    return name;
}

void Backend::addPrinter(const QString &uri, const QString &name, const QString &info, bool makeDefault) {
    const QString problem = checkName(name);
    if (!problem.isEmpty()) {
        notify(problem, true);
        return;
    }
    // "everywhere" makes cupsd build the queue from the printer's own IPP
    // attributes. cups-pk-helper hands it over as-is (checked on CUPS 2.4).
    QList<AdminCall> steps = {
        {"PrinterAdd", {name, uri, "everywhere", info, QString()}},
        // New queues start out disabled and not accepting.
        {"PrinterSetEnabled", {name, true}},
        {"PrinterSetAcceptJobs", {name, true, QString()}},
    };
    if (makeDefault)
        steps.append({"PrinterSetDefault", {name}});
    runAdmin("Added " + name + ".", steps, [this, name] {
        m_selected = name;
        emit printerAdded(name);
    });
}

void Backend::setDefault() {
    if (m_selected.isEmpty())
        return;
    runAdmin(m_selected + " is now the default printer.", {{"PrinterSetDefault", {m_selected}}});
}

void Backend::printTestPage() {
    if (m_selected.isEmpty())
        return;
    const QString name = m_selected;
    const auto *p = find(name);
    if (p && (!p->accepting)) {
        notify(name + " isn't accepting jobs. Accept jobs first (G).", true);
        return;
    }
    changeBusy(+1);
    auto *watcher = new QFutureWatcher<QString>(this);
    connect(watcher, &QFutureWatcher<QString>::finished, this, [this, watcher, name] {
        changeBusy(-1);
        const QString error = watcher->result();
        if (error.isEmpty())
            notify("Test page sent to " + name + ".", false);
        else
            notify("Couldn't print the test page: " + error, true);
        refresh();
        watcher->deleteLater();
    });
    watcher->setFuture(QtConcurrent::run(&CupsData::printTestPage, name));
}

void Backend::renamePrinter(const QString &newName) {
    if (m_selected.isEmpty() || newName == m_selected)
        return;
    const QString problem = checkName(newName);
    if (!problem.isEmpty()) {
        notify(problem, true);
        return;
    }
    runAdmin("Renamed to " + newName + ".", {{"PrinterRename", {m_selected, newName}}},
             [this, newName] { m_selected = newName; });
}

void Backend::removePrinter() {
    if (m_selected.isEmpty())
        return;
    const QString name = m_selected;
    runAdmin("Removed " + name + ".", {{"PrinterDelete", {name}}});
}

void Backend::togglePaused() {
    const auto *p = find(m_selected);
    if (!p)
        return;
    const bool resume = !p->enabled();
    runAdmin(p->name + (resume ? " resumed." : " paused."), {{"PrinterSetEnabled", {p->name, resume}}});
}

void Backend::toggleAccepting() {
    const auto *p = find(m_selected);
    if (!p)
        return;
    const bool accept = !p->accepting;
    runAdmin(p->name + (accept ? " is accepting jobs." : " is rejecting jobs."),
             {{"PrinterSetAcceptJobs", {p->name, accept, accept ? QString() : QString("Rejecting jobs from Spool")}}});
}

void Backend::setOption(const QString &key, const QString &value) {
    if (m_selected.isEmpty())
        return;
    QList<AdminCall> steps = {{"PrinterAddOptionDefault", {m_selected, key, QStringList{value}}}};
    if (key == "sides")
        steps.append({"PrinterAddOptionDefault", {m_selected, QString("Duplex"), QStringList{duplexChoice(value)}}});
    runAdmin("Updated " + m_selected + ".", steps);
}

void Backend::ownJobAction(int id, bool restart) {
    changeBusy(+1);
    struct Outcome {
        QString error;
        bool forbidden;
    };
    auto *watcher = new QFutureWatcher<Outcome>(this);
    connect(watcher, &QFutureWatcher<Outcome>::finished, this, [this, watcher, id, restart] {
        changeBusy(-1);
        const auto outcome = watcher->result();
        watcher->deleteLater();
        if (outcome.error.isEmpty()) {
            notify(QString(restart ? "Restarted job %1." : "Canceled job %1.").arg(id), false);
            refresh();
        } else if (outcome.forbidden) {
            // Someone else's job: that takes admin rights, so ask for them.
            if (restart)
                runAdmin(QString("Restarted job %1.").arg(id), {{"JobRestart", {id}}});
            else
                runAdmin(QString("Canceled job %1.").arg(id), {{"JobCancelPurge", {id, false}}});
        } else {
            notify(outcome.error, true);
            refresh();
        }
    });
    watcher->setFuture(QtConcurrent::run([id, restart] {
        bool forbidden = false;
        const QString error =
            restart ? CupsData::restartOwnJob(id, &forbidden) : CupsData::cancelOwnJob(id, &forbidden);
        return Outcome{error, forbidden};
    }));
}

void Backend::cancelJob(int id) { ownJobAction(id, false); }
void Backend::restartJob(int id) { ownJobAction(id, true); }

void Backend::discover() {
    if (m_discovering)
        return;
    m_discovering = true;
    emit discoveringChanged();

    auto *proc = new QProcess(this);
    connect(proc, &QProcess::finished, this, [this, proc] {
        QList<QVariantMap> rows;
        for (const auto &d : CupsData::parseDiscovery(QString::fromUtf8(proc->readAllStandardOutput()))) {
            bool added = false;
            for (const auto &p : m_data)
                added = added || QUrl::fromPercentEncoding(p.deviceUri.toUtf8()).contains(d.name);
            rows.append({{"uri", d.uri}, {"name", d.name}, {"added", added}});
        }
        m_discovered.setRows(rows);
        m_discovering = false;
        emit discoveringChanged();
        proc->deleteLater();
    });
    connect(proc, &QProcess::errorOccurred, this, [this, proc](QProcess::ProcessError e) {
        if (e != QProcess::FailedToStart)
            return;
        notify("Network discovery needs the `driverless` tool (cups-filters), which wasn't found.", true);
        m_discovering = false;
        emit discoveringChanged();
        proc->deleteLater();
    });
    proc->start("driverless", {});
}

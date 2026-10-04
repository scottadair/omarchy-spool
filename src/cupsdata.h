#pragma once

#include <QList>
#include <QString>
#include <QStringList>
#include <cups/cups.h>

// Everything that reads from CUPS (no admin rights needed) lives here. The
// parse functions take a raw IPP response so tests can feed them hand-built
// messages; the fetch functions talk to cupsd and are meant for worker threads.

struct OptionValue {
    QString value;
    QString label;
};

struct OptionSpec {
    QString key;   // IPP attribute name, e.g. "sides"
    QString label; // shown in the UI
    QList<OptionValue> values;
    QString current;
};

struct PrinterInfo {
    QString name;
    QString info;
    QString location;
    QString makeModel;
    QString deviceUri;
    int state = 3; // 3 idle, 4 processing, 5 stopped
    bool accepting = true;
    bool isDefault = false;
    QStringList reasons; // human readable, most severe first
    QString severity;    // "error", "warning", "report" or empty
    QList<OptionSpec> options;

    bool enabled() const { return state != 5; }
    QString stateText() const;
};

struct JobInfo {
    int id = 0;
    QString printer;
    QString name;
    QString user;
    int state = 3; // 3 pending, 4 held, 5 processing, 6 stopped, 7 canceled, 8 aborted, 9 completed
    QString reasons;
    qint64 created = 0;
    qint64 finished = 0;
    int kbytes = 0;

    bool active() const { return state >= 3 && state <= 6; }
    QString stateText() const;
};

struct DiscoveredPrinter {
    QString uri;
    QString name; // human name taken from the DNS-SD service name
};

struct Snapshot {
    QList<PrinterInfo> printers;
    QList<JobInfo> jobs;
    QString error;
};

namespace CupsData {

QString humanReason(const QString &keyword, QString *severity = nullptr);
QList<PrinterInfo> parsePrinters(ipp_t *response);
QList<JobInfo> parseJobs(ipp_t *response);
QList<DiscoveredPrinter> parseDiscovery(const QString &output);

Snapshot fetchSnapshot();
QString printTestPage(const QString &printer);   // empty on success, else a message
// No admin needed for your own jobs; forbidden=true when CUPS says the job belongs to someone else.
QString cancelOwnJob(int id, bool *forbidden);
QString restartOwnJob(int id, bool *forbidden);

} // namespace CupsData

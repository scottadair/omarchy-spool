#include "cupsdata.h"

#include <QDir>
#include <QFile>
#include <QHash>
#include <QRegularExpression>
#include <QUrl>
#include <cups/http.h>
#include <cups/ipp.h>
#include <cups/pwg.h>
#include <memory>

namespace {

using AttrMap = QHash<QString, ipp_attribute_t *>;

// The server lists some attributes twice (its own default and the one derived
// from the PPD); the first one is the one cupsd applies, so first wins.
void remember(AttrMap &map, ipp_attribute_t *attr) {
    const char *name = ippGetName(attr);
    if (name && !map.contains(QString::fromUtf8(name)))
        map.insert(QString::fromUtf8(name), attr);
}

QString str(const AttrMap &m, const char *name, int index = 0) {
    auto *a = m.value(name);
    if (!a || index >= ippGetCount(a))
        return {};
    const char *s = ippGetString(a, index, nullptr);
    return s ? QString::fromUtf8(s) : QString();
}

QStringList strs(const AttrMap &m, const char *name) {
    QStringList out;
    if (auto *a = m.value(name))
        for (int i = 0; i < ippGetCount(a); ++i)
            if (const char *s = ippGetString(a, i, nullptr))
                out << QString::fromUtf8(s);
    return out;
}

int integer(const AttrMap &m, const char *name, int fallback = 0) {
    auto *a = m.value(name);
    return a && ippGetCount(a) > 0 ? ippGetInteger(a, 0) : fallback;
}

bool boolean(const AttrMap &m, const char *name, bool fallback) {
    auto *a = m.value(name);
    return a && ippGetCount(a) > 0 ? ippGetBoolean(a, 0) : fallback;
}

// Splits a response into one attribute map per object in the given group.
QList<AttrMap> groups(ipp_t *response, ipp_tag_t wanted) {
    QList<AttrMap> out;
    AttrMap current;
    for (auto *a = ippFirstAttribute(response); a; a = ippNextAttribute(response)) {
        if (!ippGetName(a)) { // group separator
            if (!current.isEmpty())
                out << current;
            current.clear();
            continue;
        }
        if (ippGetGroupTag(a) == wanted)
            remember(current, a);
    }
    if (!current.isEmpty())
        out << current;
    return out;
}

QString mediaLabel(const QString &keyword) {
    const pwg_media_t *media = pwgMediaForPWG(keyword.toUtf8().constData());
    if (!media || !media->width)
        return keyword;
    const QString name = media->ppd      ? QString::fromUtf8(media->ppd)
                       : media->legacy ? QString::fromUtf8(media->legacy)
                                       : QString();
    const bool inches = keyword.endsWith("in");
    auto trim = [](double v) { return QString::number(v, 'f', 2).remove(QRegularExpression("\\.?0+$")); };
    const QString size = inches ? trim(media->width / 2540.0) + "×" + trim(media->length / 2540.0) + " in"
                                : trim(media->width / 100.0) + "×" + trim(media->length / 100.0) + " mm";
    return name.isEmpty() ? size : name + "  ·  " + size;
}

QString sidesLabel(const QString &v) {
    if (v == "one-sided") return "Off (one-sided)";
    if (v == "two-sided-long-edge") return "Long edge (portrait)";
    if (v == "two-sided-short-edge") return "Short edge (landscape)";
    return v;
}

QString colorLabel(const QString &v) {
    if (v == "color") return "Color";
    if (v == "monochrome") return "Grayscale";
    if (v == "auto") return "Automatic";
    if (v == "bi-level") return "Black and white";
    if (v == "process-monochrome") return "Grayscale (processed)";
    return v;
}

QString qualityLabel(int v) {
    if (v == 3) return "Draft";
    if (v == 4) return "Normal";
    if (v == 5) return "High";
    return QString::number(v);
}

void addOption(QList<OptionSpec> &out, const QString &key, const QString &label, const QList<OptionValue> &values,
               const QString &current) {
    // An option with one choice is not a choice.
    if (values.size() < 2)
        return;
    out.append({key, label, values, current});
}

QList<OptionSpec> parseOptions(const AttrMap &m) {
    QList<OptionSpec> out;

    QList<OptionValue> media;
    for (const auto &keyword : strs(m, "media-supported"))
        if (!keyword.startsWith("custom_") && !keyword.startsWith("custom-"))
            media.append({keyword, mediaLabel(keyword)});
    addOption(out, "media", "Paper size", media, str(m, "media-default"));

    QList<OptionValue> sides;
    for (const auto &v : strs(m, "sides-supported"))
        sides.append({v, sidesLabel(v)});
    addOption(out, "sides", "Duplex", sides, str(m, "sides-default"));

    QList<OptionValue> color;
    for (const auto &v : strs(m, "print-color-mode-supported"))
        color.append({v, colorLabel(v)});
    addOption(out, "print-color-mode", "Color mode", color, str(m, "print-color-mode-default"));

    QList<OptionValue> quality;
    if (auto *a = m.value("print-quality-supported"))
        for (int i = 0; i < ippGetCount(a); ++i)
            quality.append({QString::number(ippGetInteger(a, i)), qualityLabel(ippGetInteger(a, i))});
    if (!quality.isEmpty())
        addOption(out, "print-quality", "Quality", quality, QString::number(integer(m, "print-quality-default", 4)));

    return out;
}

int severityRank(const QString &s) { return s == "error" ? 3 : s == "warning" ? 2 : s == "report" ? 1 : 0; }

struct Connection {
    http_t *http = nullptr;
    Connection() {
        http = httpConnect2(cupsServer(), ippPort(), nullptr, AF_UNSPEC, cupsEncryption(), 1, 5000, nullptr);
    }
    ~Connection() {
        if (http)
            httpClose(http);
    }
};

struct Response {
    ipp_t *ipp = nullptr;
    ~Response() {
        if (ipp)
            ippDelete(ipp);
    }
    ipp_status_t status() const { return ipp ? ippGetStatusCode(ipp) : cupsLastError(); }
};

QString statusMessage(ipp_status_t status) {
    const QString last = QString::fromUtf8(cupsLastErrorString());
    return last.isEmpty() ? QString::fromUtf8(ippErrorString(status)) : last;
}

QString printerNameFromUri(const QString &uri) { return QUrl::fromPercentEncoding(uri.section('/', -1).toUtf8()); }

} // namespace

QString PrinterInfo::stateText() const {
    return state == 4 ? "Printing" : state == 5 ? "Stopped" : "Idle";
}

QString JobInfo::stateText() const {
    switch (state) {
    case 3: return "Waiting";
    case 4: return "Held";
    case 5: return "Printing";
    case 6: return "Stopped";
    case 7: return "Canceled";
    case 8: return "Failed";
    case 9: return "Completed";
    }
    return "Unknown";
}

namespace CupsData {

QString humanReason(const QString &keyword, QString *severity) {
    QString base = keyword;
    QString level = "report";
    for (const char *suffix : {"-error", "-warning", "-report"}) {
        if (base.endsWith(suffix)) {
            level = QString(suffix).mid(1);
            base.chop(strlen(suffix));
            break;
        }
    }
    if (severity)
        *severity = level;

    static const QHash<QString, QString> known = {
        {"toner-low", "Toner low"},
        {"toner-empty", "Toner empty"},
        {"marker-supply-low", "Supplies low"},
        {"marker-supply-empty", "Supplies empty"},
        {"marker-waste-almost-full", "Waste container almost full"},
        {"marker-waste-full", "Waste container full"},
        {"media-jam", "Paper jam"},
        {"media-empty", "Out of paper"},
        {"media-needed", "Paper needed"},
        {"media-low", "Paper low"},
        {"cover-open", "Cover open"},
        {"door-open", "Door open"},
        {"offline", "Offline"},
        {"connecting-to-device", "Connecting to printer"},
        {"timed-out", "Printer not responding"},
        {"input-tray-missing", "Paper tray missing"},
        {"output-area-full", "Output tray full"},
        {"output-tray-missing", "Output tray missing"},
        {"opc-near-eol", "Drum near end of life"},
        {"opc-life-over", "Drum worn out"},
        {"developer-low", "Developer low"},
        {"developer-empty", "Developer empty"},
        {"fuser-over-temp", "Fuser too hot"},
        {"fuser-under-temp", "Fuser too cold"},
        {"spool-area-full", "Spool area full"},
        {"shutdown", "Shut down"},
        {"cups-missing-filter", "Missing CUPS filter"},
        {"cups-insecure-filter", "Insecure CUPS filter"},
    };
    if (known.contains(base))
        return known.value(base);
    QString words = base;
    words.replace('-', ' ');
    return words.isEmpty() ? words : words.left(1).toUpper() + words.mid(1);
}

QList<PrinterInfo> parsePrinters(ipp_t *response) {
    QList<PrinterInfo> out;
    for (const auto &m : groups(response, IPP_TAG_PRINTER)) {
        PrinterInfo p;
        p.name = str(m, "printer-name");
        if (p.name.isEmpty())
            continue;
        p.info = str(m, "printer-info");
        p.location = str(m, "printer-location");
        p.makeModel = str(m, "printer-make-and-model");
        p.deviceUri = str(m, "device-uri");
        p.state = integer(m, "printer-state", 3);
        p.accepting = boolean(m, "printer-is-accepting-jobs", true);

        QList<QPair<int, QString>> ranked;
        for (const auto &keyword : strs(m, "printer-state-reasons")) {
            // "paused" just restates "stopped" and "none" says nothing.
            if (keyword == "none" || keyword == "paused")
                continue;
            QString level;
            const QString text = humanReason(keyword, &level);
            ranked.append({severityRank(level), text});
        }
        std::stable_sort(ranked.begin(), ranked.end(), [](auto &a, auto &b) { return a.first > b.first; });
        for (const auto &r : ranked)
            p.reasons << r.second;
        if (!ranked.isEmpty())
            p.severity = ranked.first().first == 3 ? "error" : ranked.first().first == 2 ? "warning" : "report";

        p.options = parseOptions(m);
        out << p;
    }
    return out;
}

QList<JobInfo> parseJobs(ipp_t *response) {
    QList<JobInfo> out;
    for (const auto &m : groups(response, IPP_TAG_JOB)) {
        JobInfo j;
        j.id = integer(m, "job-id");
        if (!j.id)
            continue;
        j.printer = printerNameFromUri(str(m, "job-printer-uri"));
        j.name = str(m, "job-name");
        j.user = str(m, "job-originating-user-name");
        j.state = integer(m, "job-state", 3);
        j.kbytes = integer(m, "job-k-octets");
        j.created = integer(m, "time-at-creation");
        j.finished = integer(m, "time-at-completed");
        QStringList reasons;
        for (const auto &r : strs(m, "job-state-reasons"))
            if (r != "none" && !r.startsWith("job-completed") && r != "processing-to-stop-point")
                reasons << humanReason(r);
        j.reasons = reasons.join(", ");
        out << j;
    }
    return out;
}

QList<DiscoveredPrinter> parseDiscovery(const QString &output) {
    QList<DiscoveredPrinter> out;
    for (const auto &raw : output.split('\n')) {
        const QString line = raw.trimmed();
        if (!line.startsWith("ipp://") && !line.startsWith("ipps://"))
            continue;
        DiscoveredPrinter d;
        d.uri = line.section(' ', 0, 0);
        // ipp://Brother%20MFC-L3750CDW%20series._ipp._tcp.local/ -> the service name.
        QString host = d.uri.section("://", 1).section('/', 0, 0);
        const int dot = host.indexOf("._ipp");
        if (dot > 0)
            host.truncate(dot);
        d.name = QUrl::fromPercentEncoding(host.toUtf8());
        out << d;
    }
    return out;
}

Snapshot fetchSnapshot() {
    Snapshot snap;
    Connection conn;
    if (!conn.http) {
        snap.error = "Can't reach the print service (cupsd). Is cups.service running?";
        return snap;
    }

    {
        ipp_t *request = ippNewRequest(IPP_OP_CUPS_GET_PRINTERS);
        ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_URI, "printer-uri", nullptr, "ipp://localhost/");
        static const char *wanted[] = {
            "printer-name", "printer-info", "printer-location", "printer-make-and-model", "device-uri",
            "printer-state", "printer-state-reasons", "printer-is-accepting-jobs", "media-supported",
            "media-default", "sides-supported", "sides-default", "print-color-mode-supported",
            "print-color-mode-default", "print-quality-supported", "print-quality-default"};
        ippAddStrings(request, IPP_TAG_OPERATION, IPP_TAG_KEYWORD, "requested-attributes",
                      sizeof(wanted) / sizeof(*wanted), nullptr, wanted);
        Response r{cupsDoRequest(conn.http, request, "/")};
        if (!r.ipp || ippGetStatusCode(r.ipp) > IPP_STATUS_OK_EVENTS_COMPLETE) {
            snap.error = "Couldn't list printers: " + statusMessage(r.status());
            return snap;
        }
        snap.printers = parsePrinters(r.ipp);
    }

    {
        ipp_t *request = ippNewRequest(IPP_OP_CUPS_GET_DEFAULT);
        static const char *wanted[] = {"printer-name"};
        ippAddStrings(request, IPP_TAG_OPERATION, IPP_TAG_KEYWORD, "requested-attributes", 1, nullptr, wanted);
        Response r{cupsDoRequest(conn.http, request, "/")};
        if (r.ipp && ippGetStatusCode(r.ipp) <= IPP_STATUS_OK_EVENTS_COMPLETE)
            if (auto *a = ippFindAttribute(r.ipp, "printer-name", IPP_TAG_NAME)) {
                const QString name = QString::fromUtf8(ippGetString(a, 0, nullptr));
                for (auto &p : snap.printers)
                    p.isDefault = p.name == name;
            }
    }

    for (const char *which : {"not-completed", "completed"}) {
        ipp_t *request = ippNewRequest(IPP_OP_GET_JOBS);
        ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_URI, "printer-uri", nullptr, "ipp://localhost/");
        ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_KEYWORD, "which-jobs", nullptr, which);
        // Without a user name cupsd treats us as anonymous and hides job names.
        ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_NAME, "requesting-user-name", nullptr, cupsUser());
        ippAddInteger(request, IPP_TAG_OPERATION, IPP_TAG_INTEGER, "limit", 100);
        static const char *wanted[] = {"job-id", "job-printer-uri", "job-name", "job-originating-user-name",
                                       "job-state", "job-state-reasons", "job-k-octets", "time-at-creation",
                                       "time-at-completed"};
        ippAddStrings(request, IPP_TAG_OPERATION, IPP_TAG_KEYWORD, "requested-attributes",
                      sizeof(wanted) / sizeof(*wanted), nullptr, wanted);
        Response r{cupsDoRequest(conn.http, request, "/")};
        if (r.ipp && ippGetStatusCode(r.ipp) <= IPP_STATUS_OK_EVENTS_COMPLETE)
            snap.jobs << parseJobs(r.ipp);
    }
    return snap;
}

QString printTestPage(const QString &printer) {
    // The CUPS test page is PostScript-ish; the PDF one is what cups-filters
    // ships for driverless queues, so prefer whichever exists.
    QString file;
    for (const char *candidate : {"/usr/share/cups/data/testprint", "/usr/share/cups/data/default-testpage.pdf"})
        if (QFile::exists(candidate)) {
            file = candidate;
            break;
        }
    if (file.isEmpty())
        return "No CUPS test page found in /usr/share/cups/data.";
    const QByteArray name = printer.toUtf8();
    const int id = cupsPrintFile2(CUPS_HTTP_DEFAULT, name.constData(), file.toUtf8().constData(), "Test page", 0, nullptr);
    if (id == 0)
        return QString::fromUtf8(cupsLastErrorString());
    return {};
}

QString cancelOwnJob(int id, bool *forbidden) {
    if (forbidden)
        *forbidden = false;
    // Job ids are unique per server, so the destination argument can be null.
    if (cupsCancelJob2(CUPS_HTTP_DEFAULT, nullptr, id, 0) == IPP_STATUS_OK)
        return {};
    const auto status = cupsLastError();
    if (forbidden)
        *forbidden = status == IPP_STATUS_ERROR_FORBIDDEN || status == IPP_STATUS_ERROR_NOT_AUTHORIZED
                  || status == IPP_STATUS_ERROR_NOT_POSSIBLE;
    return QString::fromUtf8(cupsLastErrorString());
}

QString restartOwnJob(int id, bool *forbidden) {
    if (forbidden)
        *forbidden = false;
    Connection conn;
    if (!conn.http)
        return "Can't reach the print service (cupsd).";
    ipp_t *request = ippNewRequest(IPP_OP_RESTART_JOB);
    const QByteArray uri = "ipp://localhost/jobs/" + QByteArray::number(id);
    ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_URI, "job-uri", nullptr, uri.constData());
    ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_NAME, "requesting-user-name", nullptr, cupsUser());
    Response r{cupsDoRequest(conn.http, request, "/jobs/")};
    if (r.ipp && ippGetStatusCode(r.ipp) <= IPP_STATUS_OK_EVENTS_COMPLETE)
        return {};
    const auto status = r.status();
    if (forbidden)
        *forbidden = status == IPP_STATUS_ERROR_FORBIDDEN || status == IPP_STATUS_ERROR_NOT_AUTHORIZED;
    return statusMessage(status);
}

} // namespace CupsData

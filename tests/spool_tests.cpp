#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QtTest>
#include <cups/ipp.h>

#include "admin.h"
#include "backend.h"
#include "cupsdata.h"
#include "printeraddress.h"
#include "rowmodel.h"
#include "theme.h"

namespace {

// Stands in for cups-pk-helper: records calls and answers from a script.
class FakeAdmin : public Admin {
public:
    struct Call {
        QString method;
        QVariantList args;
    };
    QList<Call> calls;
    QList<AdminResult> script; // consumed in order; empty means success

    void call(const QString &method, const QVariantList &args, Callback done) override {
        calls.append({method, args});
        done(script.isEmpty() ? AdminResult{} : script.takeFirst());
    }
};

ipp_t *response() {
    ipp_t *r = ippNew();
    ippSetOperation(r, IPP_OP_CUPS_GET_PRINTERS);
    ippAddString(r, IPP_TAG_OPERATION, IPP_TAG_CHARSET, "attributes-charset", nullptr, "utf-8");
    return r;
}

void addKeywords(ipp_t *r, const char *name, std::initializer_list<const char *> values) {
    QList<const char *> list(values);
    ippAddStrings(r, IPP_TAG_PRINTER, IPP_TAG_KEYWORD, name, list.size(), nullptr, list.constData());
}

void addPrinter(ipp_t *r, const char *name, int state, bool accepting, std::initializer_list<const char *> reasons) {
    ippAddSeparator(r);
    ippAddString(r, IPP_TAG_PRINTER, IPP_TAG_NAME, "printer-name", nullptr, name);
    ippAddString(r, IPP_TAG_PRINTER, IPP_TAG_TEXT, "printer-location", nullptr, "Office");
    ippAddInteger(r, IPP_TAG_PRINTER, IPP_TAG_ENUM, "printer-state", state);
    ippAddBoolean(r, IPP_TAG_PRINTER, "printer-is-accepting-jobs", accepting);
    addKeywords(r, "printer-state-reasons", reasons);
    addKeywords(r, "media-supported", {"iso_a4_210x297mm", "na_letter_8.5x11in", "custom_min_3x5in"});
    ippAddString(r, IPP_TAG_PRINTER, IPP_TAG_KEYWORD, "media-default", nullptr, "iso_a4_210x297mm");
    addKeywords(r, "sides-supported", {"one-sided", "two-sided-long-edge"});
    ippAddString(r, IPP_TAG_PRINTER, IPP_TAG_KEYWORD, "sides-default", nullptr, "two-sided-long-edge");
    // cupsd lists some defaults twice; the first value is the effective one.
    ippAddString(r, IPP_TAG_PRINTER, IPP_TAG_KEYWORD, "sides-default", nullptr, "one-sided");
    addKeywords(r, "print-color-mode-supported", {"color"}); // single choice: not offered
    int quality[] = {3, 4, 5};
    ippAddIntegers(r, IPP_TAG_PRINTER, IPP_TAG_ENUM, "print-quality-supported", 3, quality);
    ippAddInteger(r, IPP_TAG_PRINTER, IPP_TAG_ENUM, "print-quality-default", 4);
}

Snapshot sampleSnapshot() {
    Snapshot s;
    PrinterInfo a;
    a.name = "Office";
    a.isDefault = true;
    a.options = {{"sides", "Duplex", {{"one-sided", "Off"}, {"two-sided-long-edge", "Long"}}, "one-sided"}};
    PrinterInfo b;
    b.name = "Lab";
    b.state = 5;
    b.accepting = false;
    s.printers = {a, b};
    JobInfo done;
    done.id = 3; done.printer = "Office"; done.state = 9;
    JobInfo waiting;
    waiting.id = 5; waiting.printer = "Office"; waiting.state = 3;
    JobInfo other;
    other.id = 4; other.printer = "Lab"; other.state = 5;
    s.jobs = {done, other, waiting};
    return s;
}

} // namespace

class SpoolTests : public QObject {
    Q_OBJECT

private slots:
    // ---- parsing ------------------------------------------------------

    void parsesPrinterState() {
        ipp_t *r = response();
        addPrinter(r, "Office", 3, true, {"toner-low-warning", "media-jam-error", "paused", "none"});
        addPrinter(r, "Lab", 5, false, {"none"});
        const auto printers = CupsData::parsePrinters(r);
        ippDelete(r);

        QCOMPARE(printers.size(), 2);
        const auto &office = printers.at(0);
        QCOMPARE(office.name, QString("Office"));
        QCOMPARE(office.stateText(), QString("Idle"));
        QVERIFY(office.enabled());
        QVERIFY(office.accepting);
        // Most severe first; "paused" and "none" carry no information of their own.
        QCOMPARE(office.reasons, QStringList({"Paper jam", "Toner low"}));
        QCOMPARE(office.severity, QString("error"));

        const auto &lab = printers.at(1);
        QCOMPARE(lab.stateText(), QString("Stopped"));
        QVERIFY(!lab.enabled());
        QVERIFY(!lab.accepting);
        QVERIFY(lab.reasons.isEmpty());
    }

    void parsesOptionsFromThePrintersOwnValues() {
        ipp_t *r = response();
        addPrinter(r, "Office", 3, true, {"none"});
        const auto p = CupsData::parsePrinters(r).first();
        ippDelete(r);

        QStringList keys;
        for (const auto &o : p.options)
            keys << o.key;
        // print-color-mode has one value so it is not an option.
        QCOMPARE(keys, QStringList({"media", "sides", "print-quality"}));

        const auto &media = p.options.at(0);
        QCOMPARE(media.values.size(), 2); // custom_ sizes are dropped
        QCOMPARE(media.current, QString("iso_a4_210x297mm"));
        QVERIFY2(media.values.at(0).label.contains("A4"), qPrintable(media.values.at(0).label));
        QVERIFY2(media.values.at(0).label.contains("210×297 mm"), qPrintable(media.values.at(0).label));
        QVERIFY2(media.values.at(1).label.contains("8.5×11 in"), qPrintable(media.values.at(1).label));

        QCOMPARE(p.options.at(1).current, QString("two-sided-long-edge")); // first duplicate wins
        QCOMPARE(p.options.at(2).values.at(0).label, QString("Draft"));
        QCOMPARE(p.options.at(2).current, QString("4"));
    }

    void parsesJobs() {
        ipp_t *r = ippNew();
        ippAddSeparator(r);
        ippAddInteger(r, IPP_TAG_JOB, IPP_TAG_INTEGER, "job-id", 12);
        ippAddString(r, IPP_TAG_JOB, IPP_TAG_URI, "job-printer-uri", nullptr, "ipp://localhost/printers/My%20Printer");
        ippAddString(r, IPP_TAG_JOB, IPP_TAG_NAME, "job-name", nullptr, "report.pdf");
        ippAddString(r, IPP_TAG_JOB, IPP_TAG_NAME, "job-originating-user-name", nullptr, "scott");
        ippAddInteger(r, IPP_TAG_JOB, IPP_TAG_ENUM, "job-state", 9);
        ippAddInteger(r, IPP_TAG_JOB, IPP_TAG_INTEGER, "job-k-octets", 40);
        addKeywords(r, "job-state-reasons", {"job-completed-successfully"});
        ippAddSeparator(r);
        ippAddInteger(r, IPP_TAG_JOB, IPP_TAG_INTEGER, "job-id", 13);
        ippAddInteger(r, IPP_TAG_JOB, IPP_TAG_ENUM, "job-state", 5);
        const auto jobs = CupsData::parseJobs(r);
        ippDelete(r);

        QCOMPARE(jobs.size(), 2);
        QCOMPARE(jobs.at(0).id, 12);
        QCOMPARE(jobs.at(0).printer, QString("My Printer"));
        QCOMPARE(jobs.at(0).stateText(), QString("Completed"));
        QVERIFY(!jobs.at(0).active());
        QVERIFY(jobs.at(0).reasons.isEmpty());
        QVERIFY(jobs.at(1).active());
    }

    void parsesDiscovery() {
        const auto found = CupsData::parseDiscovery(
            "ipp://Brother%20MFC-L3750CDW%20series._ipp._tcp.local/\nnoise line\nipps://Lab._ipp._tcp.local/\n");
        QCOMPARE(found.size(), 2);
        QCOMPARE(found.at(0).name, QString("Brother MFC-L3750CDW series"));
        QCOMPARE(found.at(0).uri, QString("ipp://Brother%20MFC-L3750CDW%20series._ipp._tcp.local/"));
        QCOMPARE(found.at(1).name, QString("Lab"));
    }

    // ---- addresses and names -------------------------------------------

    void normalizesAddresses_data() {
        QTest::addColumn<QString>("input");
        QTest::addColumn<bool>("ok");
        QTest::addColumn<QString>("uri");
        QTest::newRow("ip") << "192.168.1.20" << true << "ipp://192.168.1.20/ipp/print";
        QTest::newRow("ip and port") << " 10.0.0.5:631 " << true << "ipp://10.0.0.5:631/ipp/print";
        QTest::newRow("hostname") << "printer.local" << true << "ipp://printer.local/ipp/print";
        QTest::newRow("ipv6") << "[fe80::1]" << true << "ipp://[fe80::1]/ipp/print";
        QTest::newRow("ipp") << "ipp://printer.lan/ipp/print" << true << "ipp://printer.lan/ipp/print";
        QTest::newRow("ipps with port") << "ipps://10.0.0.5:443/ipp/print" << true << "ipps://10.0.0.5:443/ipp/print";
        QTest::newRow("socket") << "socket://10.0.0.5:9100" << true << "socket://10.0.0.5:9100";
        QTest::newRow("empty") << "  " << false << "";
        QTest::newRow("http") << "http://printer.local" << false << "";
        QTest::newRow("lpd") << "lpd://printer.local/queue" << false << "";
        QTest::newRow("spaces") << "my printer" << false << "";
        QTest::newRow("bad ip") << "300.1.1.1.1" << false << "";
        QTest::newRow("short ip") << "1.2.3" << false << "";
        QTest::newRow("bad port") << "10.0.0.5:abc" << false << "";
        QTest::newRow("bad url port") << "ipp://10.0.0.5:99999/" << false << "";
        QTest::newRow("no host") << "ipp:///ipp/print" << false << "";
        QTest::newRow("bad label") << "-bad-.local" << false << "";
    }
    void normalizesAddresses() {
        QFETCH(QString, input);
        QFETCH(bool, ok);
        QFETCH(QString, uri);
        const auto r = PrinterAddress::normalize(input);
        QCOMPARE(r.ok, ok);
        if (ok)
            QCOMPARE(r.uri, uri);
        else
            QVERIFY(!r.error.isEmpty());
    }

    void queueNames() {
        QCOMPARE(PrinterAddress::sanitizeName("Brother MFC-L3750CDW series"), QString("Brother-MFC-L3750CDW-series"));
        QCOMPARE(PrinterAddress::sanitizeName("  a/b #c  "), QString("a-b-c"));
        QVERIFY(PrinterAddress::validateName("Office").isEmpty());
        QVERIFY(!PrinterAddress::validateName("").isEmpty());
        QVERIFY(!PrinterAddress::validateName("two words").isEmpty());
        QVERIFY(!PrinterAddress::validateName("a#b").isEmpty());
        QVERIFY(!PrinterAddress::validateName(QString(128, 'x')).isEmpty());
    }

    // ---- cups-pk-helper replies ------------------------------------------

    void helperRepliesBecomeResults() {
        QVERIFY(AdminErrors::fromHelperReply("").ok());
        QCOMPARE(AdminErrors::fromHelperReply("client-error-forbidden").kind, AdminResult::Denied);
        const auto driver = AdminErrors::fromHelperReply("client-error-bad-request");
        QCOMPARE(driver.kind, AdminResult::Failed);
        QVERIFY(driver.message.contains("driverless"));
        QCOMPARE(AdminErrors::fromHelperReply("something odd").message, QString("something odd"));
    }

    void busErrorsBecomeResults() {
        QCOMPARE(AdminErrors::fromBusError("org.freedesktop.PolicyKit1.Error.Cancelled", "").kind, AdminResult::Cancelled);
        QCOMPARE(AdminErrors::fromBusError("org.freedesktop.DBus.Error.AccessDenied", "").kind, AdminResult::Denied);
        QCOMPARE(AdminErrors::fromBusError("org.opensuse.CupsPkHelper.Mechanism.NotPrivileged", "").kind, AdminResult::Denied);
        QCOMPARE(AdminErrors::fromBusError("org.freedesktop.DBus.Error.ServiceUnknown", "").kind, AdminResult::Unavailable);
        QCOMPARE(AdminErrors::fromBusError("org.freedesktop.DBus.Error.NoReply", "").kind, AdminResult::Failed);
        QCOMPARE(AdminErrors::fromBusError("x.Y", "boom").message, QString("boom"));
    }

    // ---- backend, with the fake admin layer -------------------------------

    void addPrinterRunsTheWholeSequence() {
        auto *fake = new FakeAdmin;
        Backend backend(fake, false);
        backend.applySnapshot(sampleSnapshot());

        backend.addPrinter("ipp://p.local/ipp/print", "New", "New printer", true);

        QStringList methods;
        for (const auto &c : fake->calls)
            methods << c.method;
        QCOMPARE(methods, QStringList({"PrinterAdd", "PrinterSetEnabled", "PrinterSetAcceptJobs", "PrinterSetDefault"}));
        QCOMPARE(fake->calls.at(0).args,
                 QVariantList({"New", "ipp://p.local/ipp/print", "everywhere", "New printer", ""}));
        QVERIFY(!backend.messageIsError());
        QCOMPARE(backend.message(), QString("Added New."));
        QVERIFY(!backend.busy());
    }

    void addPrinterStopsAtTheFirstFailure() {
        auto *fake = new FakeAdmin;
        fake->script = {AdminErrors::fromHelperReply("client-error-bad-request")};
        Backend backend(fake, false);
        backend.applySnapshot(sampleSnapshot());

        backend.addPrinter("socket://10.0.0.5:9100", "Raw", "Raw", false);

        QCOMPARE(fake->calls.size(), 1);
        QVERIFY(backend.messageIsError());
        QVERIFY(backend.message().contains("driverless"));
        QVERIFY(!backend.busy());
    }

    void deniedAndCancelledPromptsAreShown() {
        auto *fake = new FakeAdmin;
        fake->script = {AdminErrors::fromBusError("org.freedesktop.PolicyKit1.Error.Cancelled", ""),
                        AdminErrors::fromBusError("org.freedesktop.DBus.Error.AccessDenied", "")};
        Backend backend(fake, false);
        backend.applySnapshot(sampleSnapshot());

        backend.setDefault();
        QVERIFY(backend.messageIsError());
        QVERIFY(backend.message().contains("cancelled"));
        backend.togglePaused();
        QVERIFY(backend.messageIsError());
        QVERIFY(backend.message().contains("denied"));
    }

    void rejectsDuplicateAndInvalidNamesBeforeCallingTheHelper() {
        auto *fake = new FakeAdmin;
        Backend backend(fake, false);
        backend.applySnapshot(sampleSnapshot());

        backend.addPrinter("ipp://p.local/", "Office", "x", false);
        backend.addPrinter("ipp://p.local/", "has space", "x", false);
        backend.renamePrinter("Lab");
        QVERIFY(fake->calls.isEmpty());
        QVERIFY(backend.messageIsError());
        QCOMPARE(backend.suggestName("Office"), QString("Office-2"));
    }

    void pauseAcceptAndDefaultUseTheSelectedPrinter() {
        auto *fake = new FakeAdmin;
        Backend backend(fake, false);
        backend.applySnapshot(sampleSnapshot());
        QCOMPARE(backend.selectedPrinter(), QString("Office")); // the default printer

        backend.togglePaused();  // idle -> pause
        backend.toggleAccepting();
        backend.select("Lab");
        backend.togglePaused();  // stopped -> resume
        backend.toggleAccepting();
        backend.setDefault();
        backend.renamePrinter("Lab2");
        backend.removePrinter();

        QCOMPARE(fake->calls.at(0).args, QVariantList({"Office", false}));
        QCOMPARE(fake->calls.at(1).method, QString("PrinterSetAcceptJobs"));
        QCOMPARE(fake->calls.at(1).args.at(1).toBool(), false);
        QVERIFY(!fake->calls.at(1).args.at(2).toString().isEmpty()); // a reason when rejecting
        QCOMPARE(fake->calls.at(2).args, QVariantList({"Lab", true}));
        QCOMPARE(fake->calls.at(3).args, QVariantList({"Lab", true, QString()}));
        QCOMPARE(fake->calls.at(4).method, QString("PrinterSetDefault"));
        QCOMPARE(fake->calls.at(5).args, QVariantList({"Lab", "Lab2"}));
        QCOMPARE(fake->calls.at(6).method, QString("PrinterDelete"));
    }

    void duplexIsSetOnTheIppAndPpdOption() {
        auto *fake = new FakeAdmin;
        Backend backend(fake, false);
        backend.applySnapshot(sampleSnapshot());

        backend.setOption("sides", "two-sided-short-edge");
        QCOMPARE(fake->calls.size(), 2);
        QCOMPARE(fake->calls.at(0).args, QVariantList({"Office", "sides", QStringList{"two-sided-short-edge"}}));
        QCOMPARE(fake->calls.at(1).args, QVariantList({"Office", "Duplex", QStringList{"DuplexTumble"}}));

        fake->calls.clear();
        backend.setOption("print-quality", "5");
        QCOMPARE(fake->calls.size(), 1);
        QCOMPARE(fake->calls.at(0).args, QVariantList({"Office", "print-quality", QStringList{"5"}}));
    }

    void jobsAreFilteredAndOrdered() {
        Backend backend(new FakeAdmin, false);
        backend.applySnapshot(sampleSnapshot());
        // Office: waiting job first, then history.
        QCOMPARE(backend.jobs()->rowCount(), 2);
        QCOMPARE(backend.jobs()->get(0).value("id").toInt(), 5);
        QCOMPARE(backend.jobs()->get(1).value("id").toInt(), 3);
        backend.select("Lab");
        QCOMPARE(backend.jobs()->rowCount(), 1);
        QCOMPARE(backend.printers()->get(0).value("activeJobs").toInt(), 1);
    }

    void selectionSurvivesRefreshAndFallsBack() {
        Backend backend(new FakeAdmin, false);
        backend.applySnapshot(sampleSnapshot());
        backend.select("Lab");
        backend.applySnapshot(sampleSnapshot());
        QCOMPARE(backend.selectedPrinter(), QString("Lab"));

        auto gone = sampleSnapshot();
        gone.printers.removeLast();
        backend.applySnapshot(gone);
        QCOMPARE(backend.selectedPrinter(), QString("Office"));
    }

    void cupsdDownIsReportedAndKeepsOldData() {
        Backend backend(new FakeAdmin, false);
        backend.applySnapshot(sampleSnapshot());
        Snapshot down;
        down.error = "Can't reach the print service";
        backend.applySnapshot(down);
        QCOMPARE(backend.connectionError(), down.error);
        QCOMPARE(backend.printers()->rowCount(), 2);
    }

    void unchangedRefreshDoesNotResetTheModel() {
        Backend backend(new FakeAdmin, false);
        backend.applySnapshot(sampleSnapshot());
        QSignalSpy reset(backend.printers(), &QAbstractItemModel::modelReset);
        QSignalSpy options(&backend, &Backend::optionsChanged);
        backend.applySnapshot(sampleSnapshot());
        QCOMPARE(reset.count(), 0);
        QCOMPARE(options.count(), 0);
    }

    // ---- theme --------------------------------------------------------------

    void themeFallsBack() {
        QTemporaryDir dir;
        Theme theme(dir.path());
        QCOMPARE(theme.accent(), QString("#FFD60A"));
        QCOMPARE(theme.accentForeground(), QString("black"));
    }

    void themeReadsAccent() {
        QTemporaryDir dir;
        QDir(dir.path()).mkpath("theme");
        QFile colors(dir.path() + "/theme/colors.toml");
        QVERIFY(colors.open(QIODevice::WriteOnly));
        colors.write("accent = \"#112233\"\n");
        colors.close();

        Theme theme(dir.path());
        QCOMPARE(theme.accent(), QString("#112233"));
        QCOMPARE(theme.accentForeground(), QString("white"));
    }

    void themeReadsFullPaletteAndFillsGaps() {
        QTemporaryDir dir;
        QDir(dir.path()).mkpath("theme");
        QFile colors(dir.path() + "/theme/colors.toml");
        QVERIFY(colors.open(QIODevice::WriteOnly));
        colors.write("mode = \"light\"\naccent = \"#112233\"\nbackground = \"#fafafa\"\nforeground = '#222222'\nbogus = \"nope\"\n");
        colors.close();

        Theme theme(dir.path());
        const auto c = theme.colors();
        QCOMPARE(theme.mode(), QString("light"));
        QCOMPARE(c.value("background").toString(), QString("#fafafa"));
        QCOMPARE(c.value("foreground").toString(), QString("#222222"));
        QVERIFY(!c.contains("bogus"));
        // Keys the theme leaves out still resolve, so no QML color is ever empty.
        QVERIFY(c.value("selection").toString().startsWith('#'));
        QVERIFY(c.value("dark_foreground").toString().startsWith('#'));
    }

    void themeFollowsChanges() {
        QTemporaryDir dir;
        QDir(dir.path()).mkpath("theme");
        auto write = [&](const char *accent) {
            QFile colors(dir.path() + "/theme/colors.toml");
            QVERIFY(colors.open(QIODevice::WriteOnly | QIODevice::Truncate));
            colors.write(QByteArray("accent = \"") + accent + "\"\n");
        };
        write("#112233");
        Theme theme(dir.path());
        QSignalSpy changed(&theme, &Theme::changed);
        write("#aabbcc");
        QTRY_COMPARE_WITH_TIMEOUT(theme.accent(), QString("#aabbcc"), 3000);
        QVERIFY(changed.count() >= 1);
    }
};

QTEST_MAIN(SpoolTests)
#include "spool_tests.moc"

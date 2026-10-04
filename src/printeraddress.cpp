#include "printeraddress.h"

#include <QHostAddress>
#include <QRegularExpression>
#include <QUrl>

namespace PrinterAddress {

namespace {
bool validHostname(const QString &host) {
    static const QRegularExpression label(R"(^[A-Za-z0-9]([A-Za-z0-9-]{0,61}[A-Za-z0-9])?$)");
    if (host.isEmpty() || host.size() > 253)
        return false;
    for (const auto &part : host.split('.'))
        if (!label.match(part).hasMatch())
            return false;
    return true;
}

bool validHost(QString host) {
    if (host.startsWith('[') && host.endsWith(']'))
        host = host.mid(1, host.size() - 2);
    // QHostAddress accepts shorthand like "1.2.3"; a printer address must be spelled out.
    static const QRegularExpression numeric(R"(^[0-9.]+$)");
    if (numeric.match(host).hasMatch())
        return host.count('.') == 3 && QHostAddress().setAddress(host);
    QHostAddress addr;
    return addr.setAddress(host) || validHostname(host);
}
} // namespace

Result normalize(const QString &input) {
    const QString text = input.trimmed();
    if (text.isEmpty())
        return {false, {}, "Enter an address, like 192.168.1.20 or ipp://printer.local/ipp/print."};

    static const QRegularExpression scheme(R"(^([A-Za-z][A-Za-z0-9+.-]*)://(.*)$)");
    const auto m = scheme.match(text);
    if (!m.hasMatch()) {
        if (text.contains(QRegularExpression(R"([\s/])")))
            return {false, {}, "That doesn't look like a hostname or IP address."};
        // host or host:port
        QString host = text;
        QString port;
        if (!text.startsWith('[') && text.count(':') == 1) {
            host = text.section(':', 0, 0);
            port = text.section(':', 1);
        }
        bool portOk = true;
        if (!port.isEmpty())
            port.toInt(&portOk);
        if (!portOk || !validHost(host))
            return {false, {}, "That doesn't look like a hostname or IP address."};
        // 631 is where IPP lives; /ipp/print is the standard IPP Everywhere path.
        return {true, "ipp://" + (port.isEmpty() ? host : host + ":" + port) + "/ipp/print", {}};
    }

    const QString kind = m.captured(1).toLower();
    if (kind != "ipp" && kind != "ipps" && kind != "socket")
        return {false, {}, "Only ipp://, ipps:// and socket:// addresses are supported."};

    const QUrl url(kind + "://" + m.captured(2), QUrl::StrictMode);
    if (!url.isValid() || !validHost(url.host()))
        return {false, {}, "The address has no valid hostname or IP."};
    if (url.port() == 0 || url.port() > 65535)
        return {false, {}, "The port number is invalid."};
    return {true, kind + "://" + m.captured(2), {}};
}

QString sanitizeName(const QString &suggestion) {
    QString name = suggestion.trimmed();
    name.replace(QRegularExpression(R"([\s/#]+)"), "-");
    name.remove(QRegularExpression(R"([\x00-\x1f\x7f])"));
    name.replace(QRegularExpression("-{2,}"), "-");
    name.remove(QRegularExpression("^-+|-+$"));
    return name.left(127);
}

QString validateName(const QString &name) {
    if (name.isEmpty())
        return "Give the printer a name.";
    if (name.size() > 127)
        return "The name is too long (127 characters at most).";
    if (name.contains(QRegularExpression(R"([\s/#\x00-\x1f\x7f])")))
        return "Names can't contain spaces, / or #. Use dashes instead.";
    return {};
}

} // namespace PrinterAddress

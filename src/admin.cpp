#include "admin.h"

#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>

namespace AdminErrors {

AdminResult fromHelperReply(const QString &reply) {
    const QString text = reply.trimmed();
    if (text.isEmpty())
        return {AdminResult::Ok, {}};
    const QString lower = text.toLower();
    if (lower.contains("forbidden") || lower.contains("not-authorized") || lower.contains("not authorized"))
        return {AdminResult::Denied, "CUPS refused the change (" + text + "). Your account isn't allowed to administer printers."};
    if (lower.contains("not-possible") || lower.contains("does not exist") || lower.contains("not-found"))
        return {AdminResult::Failed, "That isn't possible right now: " + text};
    if (lower.contains("bad-request") || lower.contains("not-supported") || lower.contains("filter") || lower.contains("ppd"))
        return {AdminResult::Failed,
                "CUPS couldn't set the printer up (" + text +
                    "). Spool only supports driverless (IPP Everywhere) printers; printers that need a vendor driver can't be added here."};
    return {AdminResult::Failed, text};
}

AdminResult fromBusError(const QString &name, const QString &message) {
    const QString n = name.toLower();
    // polkit reports a dismissed password dialog and a wrong/blocked one differently.
    if (n.contains("cancel") || n.contains("dismiss"))
        return {AdminResult::Cancelled, "Authentication was cancelled. Nothing was changed."};
    if (n.contains("notauthorized") || n.contains("notprivileged") || n.contains("accessdenied") ||
        n.contains("challenge") || n.contains("authorization"))
        return {AdminResult::Denied, "Authentication was denied. Nothing was changed."};
    if (n.contains("serviceunknown") || n.contains("namehasnoowner"))
        return {AdminResult::Unavailable, "cups-pk-helper isn't available on the system bus. Install cups-pk-helper to change printers."};
    if (n.contains("noreply") || n.contains("timeout") || n.contains("timedout"))
        return {AdminResult::Failed, "Timed out waiting for authentication."};
    return {AdminResult::Failed, message.isEmpty() ? name : message};
}

} // namespace AdminErrors

void PkHelperAdmin::call(const QString &method, const QVariantList &args, Callback done) {
    auto message = QDBusMessage::createMethodCall("org.opensuse.CupsPkHelper.Mechanism", "/",
                                                  "org.opensuse.CupsPkHelper.Mechanism", method);
    message.setArguments(args);
    message.setInteractiveAuthorizationAllowed(true);

    // The password prompt is part of the call, so the default 25 s would cut
    // the user off mid-typing. Ten minutes is generous without hanging forever.
    auto pending = QDBusConnection::systemBus().asyncCall(message, 10 * 60 * 1000);
    auto *watcher = new QDBusPendingCallWatcher(pending, this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [done = std::move(done)](QDBusPendingCallWatcher *w) {
        QDBusPendingReply<QString> reply = *w;
        if (reply.isError())
            done(AdminErrors::fromBusError(reply.error().name(), reply.error().message()));
        else
            done(AdminErrors::fromHelperReply(reply.value()));
        w->deleteLater();
    });
}

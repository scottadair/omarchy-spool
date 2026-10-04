#pragma once

#include <QDBusError>
#include <QObject>
#include <QString>
#include <QVariantList>
#include <functional>

// Everything that needs CUPS admin rights goes through cups-pk-helper. This
// interface is the seam: the app uses PkHelperAdmin on the system bus, tests
// use a fake that returns canned answers.

struct AdminResult {
    enum Kind { Ok, Failed, Denied, Cancelled, Unavailable };
    Kind kind = Ok;
    QString message; // user-facing, empty when Ok

    bool ok() const { return kind == Ok; }
};

class Admin : public QObject {
    Q_OBJECT
public:
    using Callback = std::function<void(const AdminResult &)>;
    using QObject::QObject;

    // Calls a cups-pk-helper method. `args` are already typed for D-Bus
    // (QString, bool, int, QStringList). `done` runs on the caller's thread.
    virtual void call(const QString &method, const QVariantList &args, Callback done) = 0;
};

namespace AdminErrors {
// What cups-pk-helper returned in its string reply ("" means success).
AdminResult fromHelperReply(const QString &reply);
// What the bus said when the call itself failed.
AdminResult fromBusError(const QString &name, const QString &message);
} // namespace AdminErrors

class PkHelperAdmin : public Admin {
    Q_OBJECT
public:
    explicit PkHelperAdmin(QObject *parent = nullptr) : Admin(parent) {}
    void call(const QString &method, const QVariantList &args, Callback done) override;
};

#pragma once

#include <QString>

// Checks what the user typed in the "add by address" box and what they want to
// call the queue, before anything is sent to CUPS.
namespace PrinterAddress {

struct Result {
    bool ok = false;
    QString uri;   // normalized device URI
    QString error; // set when !ok
};

Result normalize(const QString &text);

// CUPS queue names cannot contain spaces, '/', '#' or control characters.
QString sanitizeName(const QString &suggestion);
QString validateName(const QString &name);

} // namespace PrinterAddress

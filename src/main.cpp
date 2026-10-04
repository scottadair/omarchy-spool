// spool — printer settings for Omarchy: list, add, configure and watch printers.

#include <QGuiApplication>
#include <QIcon>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QUrl>

#include "backend.h"
#include "theme.h"

int main(int argc, char *argv[]) {
    QGuiApplication app(argc, argv);
    app.setApplicationName("spool");
    app.setApplicationVersion("0.1.0");

    // Matches the window to spool.desktop, so the compositor (Wayland app_id)
    // and the launcher pick up the installed icon.
    app.setDesktopFileName("spool");
    app.setWindowIcon(QIcon::fromTheme("spool"));

    QQuickStyle::setStyle("Material");

    Theme theme;
    Backend backend;

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty("theme", &theme);
    engine.rootContext()->setContextProperty("backend", &backend);
    engine.load(QUrl("qrc:/Main.qml"));
    if (engine.rootObjects().isEmpty())
        return 1;

    return app.exec();
}

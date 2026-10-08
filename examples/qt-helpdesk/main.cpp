// qt-helpdesk: "Acme Desk", a Qt 6 Widgets app with a Get help button.
//
//   GAIADESK_EMBED_TOKEN=gdemb_… ./qt-helpdesk
//
// GAIADESK_SERVER picks the server, GAIADESK_DEMO_GUIDED=1 allows guided
// input, GAIADESK_DEMO_AUTOSTART=1 presses Get help at launch. For automated
// tests only: GAIADESK_DEMO_TEST_CONSENT=1 skips the consent dialog, and
// GAIADESK_DEMO_EXIT_AFTER_MS quits after that long. Every event is printed
// as one line on stdout.
//
// Linux: run under X11 (or XWayland: QT_QPA_PLATFORM=xcb); the library is
// view-only there and shows no consent dialog of its own (this app asks).
//
// SPDX-License-Identifier: MIT
#include "HelpDeskWindow.h"

#include <QApplication>
#include <QTimer>

int main(int argc, char** argv) {
    QApplication app(argc, argv);
    HelpDeskWindow window;
    window.show();
    if (qEnvironmentVariable("GAIADESK_DEMO_AUTOSTART") == QLatin1String("1"))
        QTimer::singleShot(300, &window, &HelpDeskWindow::getHelp);
    bool ok = false;
    const int exitAfter = qEnvironmentVariableIntValue("GAIADESK_DEMO_EXIT_AFTER_MS", &ok);
    if (ok && exitAfter > 0) QTimer::singleShot(exitAfter, &app, &QApplication::quit);
    return app.exec();
}

#include "apptheme.hpp"
#include "mainwindow.hpp"
#include <QApplication>
#include <QLoggingCategory>
#include <QSettings>

int main(int argc, char* argv[]) {
    // Suppress the KDE SNI unavailable warning — non-fatal, fires when the
    // StatusNotifierWatcher D-Bus service isn't ready at launch time.
    QLoggingCategory::setFilterRules("kf.statusnotifieritem=false");

    QApplication app(argc, argv);

    app.setApplicationName("KB2DS");
    app.setOrganizationName("KB2DS");
    app.setApplicationDisplayName("Keyboard to DualSense");
    app.setQuitOnLastWindowClosed(false);

    // ── Apply saved theme before the first window is shown ────────────────────
    {
        QSettings s("KB2DS", "KB2DS");
        Themes::setCurrent(s.value("theme", Themes::darkBlue().name).toString());
    }
    app.setPalette(Themes::buildPalette(Themes::current()));
    app.setStyleSheet(Themes::buildStylesheet(Themes::current()));

    MainWindow window;
    window.show();

    return app.exec();
}

#include <slowmenu.moc>

#include <dbusmenuexporter.h>

#include <QtDBus>
#include <QtGui>

static const char *TEST_SERVICE = "org.kde.dbusmenu-qt-test";
static const char *TEST_OBJECT_PATH = "/TestMenuBar";

SlowMenu::SlowMenu()
: QMenu()
{
    connect(this, SIGNAL(aboutToShow()), SLOT(slotAboutToShow()));
}

void SlowMenu::slotAboutToShow()
{
    qDebug() << __FUNCTION__ << "Entering";
    QTime time;
    time.start();
    while (time.elapsed() < 2000) {
        qApp->processEvents();
    }
    qDebug() << __FUNCTION__ << "Leaving";
}

int main(int argc, char** argv)
{
    QApplication app(argc, argv);
    QDBusConnection::sessionBus().registerService(TEST_SERVICE);
    SlowMenu* inputMenu = new SlowMenu;
    inputMenu->addAction("Test");
    DBusMenuExporter exporter(TEST_OBJECT_PATH, inputMenu);
    qDebug() << "Looping";
    return app.exec();
}

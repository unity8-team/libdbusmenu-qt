/* This file is part of the dbusmenu-qt library
   Copyright 2010 Canonical
   Author: Aurelien Gateau <aurelien.gateau@canonical.com>

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public
   License (LGPL) as published by the Free Software Foundation;
   either version 2 of the License, or (at your option) any later
   version.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public License
   along with this library; see the file COPYING.LIB.  If not, write to
   the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
   Boston, MA 02110-1301, USA.
*/
// Self
#include "dbusmenuimportertest.h"

// Qt
#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusReply>
#include <QIcon>
#include <QMenu>
#include <QtTest>

// DBusMenuQt
#include <dbusmenucustomitemaction.h>
#include <dbusmenucustomitemfactory.h>
#include <dbusmenuexporter.h>
#include <dbusmenuimporter.h>
#include <debug_p.h>

// Local
#include "testutils.h"

QTEST_MAIN(DBusMenuImporterTest)

static const char *TEST_SERVICE = "org.kde.dbusmenu-qt-test";
static const char *TEST_OBJECT_PATH = "/TestMenuBar";

/**
 * Helper class to register TEST_SERVICE.
 * We don't do this in some init()/cleanup() methods because for some tests the
 * service must not be registered.
 */
class RegisterServiceHelper
{
public:
    RegisterServiceHelper()
    {
        QVERIFY(QDBusConnection::sessionBus().registerService(TEST_SERVICE));
    }

    ~RegisterServiceHelper()
    {
        QVERIFY(QDBusConnection::sessionBus().unregisterService(TEST_SERVICE));
    }
};

Q_DECLARE_METATYPE(QAction*)

void DBusMenuImporterTest::initTestCase()
{
    qRegisterMetaType<QAction*>("QAction*");
}

void DBusMenuImporterTest::cleanup()
{
    waitForDeferredDeletes();
}

void DBusMenuImporterTest::testStandardItem()
{
    RegisterServiceHelper helper;

    QMenu inputMenu;
    QAction *action = inputMenu.addAction("Test");
    action->setVisible(false);
    DBusMenuExporter exporter(TEST_OBJECT_PATH, &inputMenu);

    DBusMenuImporter importer(TEST_SERVICE, TEST_OBJECT_PATH);
    QTest::qWait(500);

    QMenu *outputMenu = importer.menu();
    QCOMPARE(outputMenu->actions().count(), 1);
    QAction *outputAction = outputMenu->actions().first();
    QVERIFY(!outputAction->isVisible());
    QCOMPARE(outputAction->text(), QString("Test"));

    // Make the action visible, outputAction should become visible as well
    action->setVisible(true);
    QTest::qWait(500);

    QVERIFY(outputAction->isVisible());
}

void DBusMenuImporterTest::testAddingNewItem()
{
    RegisterServiceHelper helper;

    QMenu inputMenu;
    QAction *action = inputMenu.addAction("Test");
    DBusMenuExporter exporter(TEST_OBJECT_PATH, &inputMenu);

    DBusMenuImporter importer(TEST_SERVICE, TEST_OBJECT_PATH);
    QTest::qWait(500);
    QMenu *outputMenu = importer.menu();
    QCOMPARE(outputMenu->actions().count(), inputMenu.actions().count());

    inputMenu.addAction("Test2");
    QTest::qWait(500);
    QCOMPARE(outputMenu->actions().count(), inputMenu.actions().count());
}

void DBusMenuImporterTest::testShortcut()
{
    RegisterServiceHelper helper;

    QMenu inputMenu;
    QAction *action = inputMenu.addAction("Test");
    action->setShortcut(Qt::CTRL | Qt::Key_S);
    DBusMenuExporter exporter(TEST_OBJECT_PATH, &inputMenu);

    DBusMenuImporter importer(TEST_SERVICE, TEST_OBJECT_PATH);
    QTest::qWait(500);
    QMenu *outputMenu = importer.menu();

    QAction *outputAction = outputMenu->actions().at(0);
    QCOMPARE(outputAction->shortcut(), action->shortcut());
}

void DBusMenuImporterTest::testDeletingImporterWhileWaitingForAboutToShow()
{
    // Start test program and wait for it to be ready
    QProcess slowMenuProcess;
    slowMenuProcess.start("./slowmenu");
    QTest::qWait(500);

    // Create importer and wait for the menu
    DBusMenuImporter *importer = new DBusMenuImporter(TEST_SERVICE, TEST_OBJECT_PATH);
    QTest::qWait(500);

    QMenu *outputMenu = importer->menu();
    QTimer::singleShot(100, importer, SLOT(deleteLater()));
    outputMenu->popup(QPoint(0, 0));

    // If it crashes, it will crash while waiting there
    QTest::qWait(500);

    // Done, stop our test program
    slowMenuProcess.close();
    slowMenuProcess.waitForFinished();
}

void DBusMenuImporterTest::testDynamicMenu()
{
    RegisterServiceHelper helper;

    QMenu rootMenu;
    QAction* a1 = new QAction("a1", &rootMenu);
    QAction* a2 = new QAction("a2", &rootMenu);
    MenuFiller rootMenuFiller(&rootMenu);
    rootMenuFiller.addAction(a1);
    rootMenuFiller.addAction(a2);

    QMenu subMenu;
    MenuFiller subMenuFiller(&subMenu);
    subMenuFiller.addAction(new QAction("a3", &subMenu));

    a1->setMenu(&subMenu);

    DBusMenuExporter exporter(TEST_OBJECT_PATH, &rootMenu);

    // Import this menu
    DBusMenuImporter importer(TEST_SERVICE, TEST_OBJECT_PATH);
    QTest::qWait(500);
    QMenu *outputMenu = importer.menu();

    // There should be no children for now
    QCOMPARE(outputMenu->actions().count(), 0);

    // Update menu, a1 and a2 should get added
    QSignalSpy spy(&importer, SIGNAL(menuUpdated()));
    QSignalSpy spyOld(&importer, SIGNAL(menuReadyToBeShown()));
    importer.updateMenu();
    while (spy.isEmpty()) {
        QTest::qWait(500);
    }

    QCOMPARE(outputMenu->actions().count(), 2);
    QTest::qWait(500);
    QAction* a1Output = outputMenu->actions().first();

    // a1Output should have an empty menu
    QMenu* a1OutputMenu = a1Output->menu();
    QVERIFY(a1OutputMenu);
    QCOMPARE(a1OutputMenu->actions().count(), 0);

    // Show a1OutputMenu, a3 should get added
    QMetaObject::invokeMethod(a1OutputMenu, "aboutToShow");
    QTest::qWait(500);

    QCOMPARE(a1OutputMenu->actions().count(), 1);

    // menuUpdated() and menuReadyToBeShown() should only have been emitted
    // once
    QCOMPARE(spy.count(), 1);
    QCOMPARE(spyOld.count(), 1);
}

void DBusMenuImporterTest::testActionActivationRequested()
{
    RegisterServiceHelper helper;

    // Export a menu
    QMenu inputMenu;
    QAction *inputA1 = inputMenu.addAction("a1");
    QAction *inputA2 = inputMenu.addAction("a2");
    DBusMenuExporter exporter(TEST_OBJECT_PATH, &inputMenu);

    // Import the menu
    DBusMenuImporter importer(TEST_SERVICE, TEST_OBJECT_PATH);
    QSignalSpy spy(&importer, SIGNAL(actionActivationRequested(QAction*)));

    QTest::qWait(500);
    QMenu *outputMenu = importer.menu();

    // Get matching output actions
    QCOMPARE(outputMenu->actions().count(), 2);
    QAction *outputA1 = outputMenu->actions().at(0);
    QAction *outputA2 = outputMenu->actions().at(1);

    // Request activation
    exporter.activateAction(inputA1);
    exporter.activateAction(inputA2);

    // Check we received the signal in the right order
    QTest::qWait(500);
    QCOMPARE(spy.count(), 2);
    QCOMPARE(spy.takeFirst().at(0).value<QAction*>(), outputA1);
    QCOMPARE(spy.takeFirst().at(0).value<QAction*>(), outputA2);
}

void DBusMenuImporterTest::testActionsAreDeletedWhenImporterIs()
{
    RegisterServiceHelper helper;

    // Export a menu
    QMenu inputMenu;
    inputMenu.addAction("a1");
    QMenu *inputSubMenu = inputMenu.addMenu("subMenu");
    inputSubMenu->addAction("a2");
    DBusMenuExporter exporter(TEST_OBJECT_PATH, &inputMenu);

    // Import the menu
    DBusMenuImporter *importer = new DBusMenuImporter(TEST_SERVICE, TEST_OBJECT_PATH);
    QTest::qWait(500);

    // Put all items of the menu in a list of QPointers
    QList< QPointer<QObject> > children;

    QMenu *outputMenu = importer->menu();
    QCOMPARE(outputMenu->actions().count(), 2);
    QMenu *outputSubMenu = outputMenu->actions().at(1)->menu();
    QVERIFY(outputSubMenu);
    QCOMPARE(outputSubMenu->actions().count(), 1);

    children << outputMenu->actions().at(0);
    children << outputMenu->actions().at(1);
    children << outputSubMenu;
    children << outputSubMenu->actions().at(0);

    delete importer;
    waitForDeferredDeletes();

    // There should be only invalid pointers in children
    Q_FOREACH(QPointer<QObject> child, children) {
        //qDebug() << child;
        QVERIFY(child.isNull());
    }
}

class TestCustomItemFactory : public DBusMenuCustomItemFactory
{
public:
    TestCustomItemFactory(const QString &type)
    : DBusMenuCustomItemFactory(type)
    {}

    virtual QAction *createAction(const QVariantMap &properties, QObject *parent)
    {
        QString text = QString("type=%1 int=%2 str=%3")
            .arg(itemType())
            .arg(properties.value("int").toInt())
            .arg(properties.value("str").toString());
        return new QAction(text, parent);
    }
};

void DBusMenuImporterTest::testCustomItems()
{
    // Create a menu containing two custom items
    QMenu inputMenu;
    QVERIFY(QDBusConnection::sessionBus().registerService(TEST_SERVICE));
    DBusMenuExporter *exporter = new DBusMenuExporter(TEST_OBJECT_PATH, &inputMenu);

    QVariantMap props1;
    props1["type"] = "x-a1";
    props1["int"] = 1;
    props1["str"] = "a1";
    QVariantMap props2;
    props2["type"] = "x-a2";
    props2["int"] = 2;
    props2["str"] = "a2";
    inputMenu.addAction(new DBusMenuCustomItemAction(props1));
    inputMenu.addAction(new DBusMenuCustomItemAction(props2));

    // Check exporter is on DBus
    QDBusInterface iface(TEST_SERVICE, TEST_OBJECT_PATH);
    QVERIFY2(iface.isValid(), qPrintable(iface.lastError().message()));

    // Import the menu
    DBusMenuImporter *importer = new DBusMenuImporter(TEST_SERVICE, TEST_OBJECT_PATH);
    importer->addCustomItemFactory(new TestCustomItemFactory("x-a1"));
    importer->addCustomItemFactory(new TestCustomItemFactory("x-a2"));
    QTest::qWait(500);

    QMenu *outputMenu = importer->menu();
    QCOMPARE(outputMenu->actions().count(), 2);
    QCOMPARE(outputMenu->actions().at(0)->text(), QString("type=x-a1 int=1 str=a1"));
    QCOMPARE(outputMenu->actions().at(1)->text(), QString("type=x-a2 int=2 str=a2"));
}

#include "dbusmenuimportertest.moc"

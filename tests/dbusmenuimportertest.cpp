/* This file is part of the KDE libraries
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
#include <dbusmenuexporter.h>
#include <dbusmenuimporter.h>
#include <debug_p.h>

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

#include "dbusmenuimportertest.moc"

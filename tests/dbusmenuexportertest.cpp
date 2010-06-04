/* This file is part of the KDE libraries
   Copyright 2009 Canonical
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
#include "dbusmenuexportertest.h"

// Qt
#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusReply>
#include <QIcon>
#include <QMenu>
#include <QtTest>

// DBusMenuQt
#include <dbusmenuexporter.h>
#include <dbusmenuitem_p.h>
#include <debug_p.h>

QTEST_MAIN(DBusMenuExporterTest)

static const char *TEST_SERVICE = "org.kde.dbusmenu-qt-test";
static const char *TEST_OBJECT_PATH = "/TestMenuBar";

/**
 * An implementation of DBusMenuExporter which uses a property from the action
 * to get the icon name
 */
class TestDBusMenuExporter : public DBusMenuExporter
{
public:
    TestDBusMenuExporter(const QString& path, QMenu *menu)
    : DBusMenuExporter(path, menu)
    {}

protected:
    QString iconNameForAction(QAction *action)
    {
        return action->property("icon-name").toString();
    }
};


void MenuFiller::fillMenu()
{
    m_menu->addAction("a1");
    m_menu->addAction("a2");
}

void DBusMenuExporterTest::init()
{
    QVERIFY(QDBusConnection::sessionBus().registerService(TEST_SERVICE));
}

void DBusMenuExporterTest::cleanup()
{
    QVERIFY(QDBusConnection::sessionBus().unregisterService(TEST_SERVICE));
}

void DBusMenuExporterTest::testGetSomeProperties_data()
{
    QTest::addColumn<QString>("label");
    QTest::addColumn<QString>("iconName");
    QTest::addColumn<bool>("enabled");

    QTest::newRow("label only")           << "label" << QString() << true;
    QTest::newRow("disabled, label only") << "label" << QString() << false;
    QTest::newRow("icon name")            << "label" << "icon"    << true;
}

void DBusMenuExporterTest::testGetSomeProperties()
{
    QFETCH(QString, label);
    QFETCH(QString, iconName);
    QFETCH(bool, enabled);

    // Create an exporter for a menu with one action, defined by the test data
    QMenu inputMenu;
    TestDBusMenuExporter exporter(TEST_OBJECT_PATH, &inputMenu);

    QAction *action = new QAction(label, &inputMenu);
    if (!iconName.isEmpty()) {
        action->setProperty("icon-name", iconName);
    }
    action->setEnabled(enabled);
    inputMenu.addAction(action);

    // Check out exporter is on DBus
    QDBusInterface iface(TEST_SERVICE, TEST_OBJECT_PATH);
    QVERIFY2(iface.isValid(), qPrintable(iface.lastError().message()));

    // Get exported menu info
    QStringList propertyNames = QStringList() << "type" << "enabled" << "label" << "icon-name";
    QDBusReply<DBusMenuItemList> reply = iface.call("GetChildren", 0, propertyNames);
    QVERIFY2(reply.isValid(), qPrintable(reply.error().message()));

    // Check the info we received, in particular, check that any property set to
    // its default value is *not* exported
    DBusMenuItemList list = reply.value();
    QCOMPARE(list.count(), 1);
    DBusMenuItem item = list.first();
    QVERIFY(item.id != 0);
    QVERIFY(!item.properties.contains("type"));
    QCOMPARE(item.properties.value("label").toString(), label);
    if (enabled) {
        QVERIFY(!item.properties.contains("enabled"));
    } else {
        QCOMPARE(item.properties.value("enabled").toBool(), false);
    }
    if (iconName.isEmpty()) {
        QVERIFY(!item.properties.contains("icon-name"));
    } else {
        QCOMPARE(item.properties.value("icon-name").toString(), iconName);
    }
}

void DBusMenuExporterTest::testGetAllProperties()
{
    const QSet<QString> a1Properties = QSet<QString>()
        << "label"
        ;

    const QSet<QString> a2Properties = QSet<QString>()
        << "label"
        << "enabled"
        << "icon-name"
        << "visible"
        ;

    const QSet<QString> separatorProperties = QSet<QString>()
        << "type";

    QMenu inputMenu;
    TestDBusMenuExporter exporter(TEST_OBJECT_PATH, &inputMenu);

    inputMenu.addAction("a1");
    inputMenu.addSeparator();
    QAction *a2 = new QAction("a2", &inputMenu);
    a2->setEnabled(false);
    a2->setProperty("icon-name", "foo");
    a2->setVisible(false);
    inputMenu.addAction(a2);

    QDBusInterface iface(TEST_SERVICE, TEST_OBJECT_PATH);
    QVERIFY2(iface.isValid(), qPrintable(iface.lastError().message()));

    QDBusReply<DBusMenuItemList> reply = iface.call("GetChildren", 0, QStringList());
    QVERIFY2(reply.isValid(), qPrintable(reply.error().message()));

    DBusMenuItemList list = reply.value();
    QCOMPARE(list.count(), 3);

    DBusMenuItem item = list.takeFirst();
    QCOMPARE(QSet<QString>::fromList(item.properties.keys()), a1Properties);

    item = list.takeFirst();
    QCOMPARE(QSet<QString>::fromList(item.properties.keys()), separatorProperties);

    item = list.takeFirst();
    QCOMPARE(QSet<QString>::fromList(item.properties.keys()), a2Properties);
}

void DBusMenuExporterTest::testGetNonExistentProperty()
{
    const char* NON_EXISTENT_KEY = "i-do-not-exist";

    QMenu inputMenu;
    inputMenu.addAction("a1");
    DBusMenuExporter exporter(TEST_OBJECT_PATH, &inputMenu);

    QDBusInterface iface(TEST_SERVICE, TEST_OBJECT_PATH);
    QDBusReply<DBusMenuItemList> reply = iface.call("GetChildren", 0, QStringList() << NON_EXISTENT_KEY);
    QVERIFY2(reply.isValid(), qPrintable(reply.error().message()));

    DBusMenuItemList list = reply.value();
    QCOMPARE(list.count(), 1);

    DBusMenuItem item = list.takeFirst();
    QVERIFY(!item.properties.contains(NON_EXISTENT_KEY));
}

void DBusMenuExporterTest::testClickedEvent()
{
    QMenu inputMenu;
    QAction *action = inputMenu.addAction("a1");
    QSignalSpy spy(action, SIGNAL(triggered()));
    DBusMenuExporter exporter(TEST_OBJECT_PATH, &inputMenu);

    QDBusInterface iface(TEST_SERVICE, TEST_OBJECT_PATH);
    QDBusReply<DBusMenuItemList> reply = iface.call("GetChildren", 0, QStringList());
    QVERIFY2(reply.isValid(), qPrintable(reply.error().message()));

    DBusMenuItemList list = reply.value();
    QCOMPARE(list.count(), 1);
    int id = list.first().id;

    QVariant empty = QVariant::fromValue(QDBusVariant(QString()));
    uint timestamp = QDateTime::currentDateTime().toTime_t();
    iface.call("Event", id, "clicked", empty, timestamp);
    QTest::qWait(500);

    QCOMPARE(spy.count(), 1);
}

void DBusMenuExporterTest::testSubMenu()
{
    QMenu inputMenu;
    QMenu *subMenu = inputMenu.addMenu("menu");
    QAction *a1 = subMenu->addAction("a1");
    QAction *a2 = subMenu->addAction("a2");
    DBusMenuExporter exporter(TEST_OBJECT_PATH, &inputMenu);

    QDBusInterface iface(TEST_SERVICE, TEST_OBJECT_PATH);
    QDBusReply<DBusMenuItemList> reply = iface.call("GetChildren", 0, QStringList());
    QVERIFY2(reply.isValid(), qPrintable(reply.error().message()));

    DBusMenuItemList list = reply.value();
    QCOMPARE(list.count(), 1);
    int id = list.first().id;

    reply = iface.call("GetChildren", id, QStringList());
    QVERIFY2(reply.isValid(), qPrintable(reply.error().message()));
    list = reply.value();
    QCOMPARE(list.count(), 2);

    DBusMenuItem item = list.takeFirst();
    QVERIFY(item.id != 0);
    QCOMPARE(item.properties.value("label").toString(), a1->text());

    item = list.takeFirst();
    QCOMPARE(item.properties.value("label").toString(), a2->text());
}

void DBusMenuExporterTest::testDynamicSubMenu()
{
    QMenu inputMenu;
    DBusMenuExporter exporter(TEST_OBJECT_PATH, &inputMenu);
    QAction *action = inputMenu.addAction("menu");
    QMenu *subMenu = new QMenu;
    action->setMenu(subMenu);
    MenuFiller filler(subMenu);

    QDBusInterface iface(TEST_SERVICE, TEST_OBJECT_PATH);

    // Get id of submenu
    QDBusReply<DBusMenuItemList> reply = iface.call("GetChildren", 0, QStringList());
    QVERIFY2(reply.isValid(), qPrintable(reply.error().message()));
    DBusMenuItemList list = reply.value();
    QCOMPARE(list.count(), 1);
    int id = list.first().id;

    // Nothing for now
    QCOMPARE(subMenu->actions().count(), 0);

    // Pretend we show the menu
    ManualSignalSpy layoutUpdatedSpy;
    QDBusConnection::sessionBus().connect(TEST_SERVICE, TEST_OBJECT_PATH, "org.ayatana.dbusmenu", "LayoutUpdated", "ui", &layoutUpdatedSpy, SLOT(receiveCall(uint, int)));

    QDBusReply<bool> aboutToShowReply = iface.call("AboutToShow", id);
    QVERIFY2(aboutToShowReply.isValid(), qPrintable(reply.error().message()));
    QVERIFY(aboutToShowReply.value());
    QTest::qWait(500);
    QVERIFY(layoutUpdatedSpy.count() >= 1);
    QCOMPARE(layoutUpdatedSpy.takeFirst().at(1).toInt(), id);

    // Get submenu items
    reply = iface.call("GetChildren", id, QStringList());
    QVERIFY2(reply.isValid(), qPrintable(reply.error().message()));
    list = reply.value();
    QVERIFY(subMenu->actions().count() > 0);
    QCOMPARE(list.count(), subMenu->actions().count());

    for (int pos=0; pos< list.count(); ++pos) {
        DBusMenuItem item = list.at(pos);
        QVERIFY(item.id != 0);
        QAction *action = subMenu->actions().at(pos);
        QVERIFY(action);
        QCOMPARE(item.properties.value("label").toString(), action->text());
    }
}

void DBusMenuExporterTest::testRadioItems()
{
    DBusMenuItem item;
    DBusMenuItemList list;
    QMenu inputMenu;
    QVERIFY(QDBusConnection::sessionBus().registerService(TEST_SERVICE));
    DBusMenuExporter exporter(TEST_OBJECT_PATH, &inputMenu);

    // Create 2 radio items, check first one
    QAction *a1 = inputMenu.addAction("a1");
    a1->setCheckable(true);
    QAction *a2 = inputMenu.addAction("a1");
    a2->setCheckable(true);

    QActionGroup group(0);
    group.addAction(a1);
    group.addAction(a2);
    a1->setChecked(true);

    QVERIFY(!a2->isChecked());

    // Get item ids
    QDBusInterface iface(TEST_SERVICE, TEST_OBJECT_PATH);
    QDBusReply<DBusMenuItemList> reply = iface.call("GetChildren", 0, QStringList());
    QVERIFY2(reply.isValid(), qPrintable(reply.error().message()));
    list = reply.value();
    QCOMPARE(list.count(), 2);

    item = list.takeFirst();
    QCOMPARE(item.properties.value("toggle-state").toInt(), 1);
    int a1Id = item.id;
    item = list.takeFirst();
    QCOMPARE(item.properties.value("toggle-state").toInt(), 0);
    int a2Id = item.id;

    // Click a2
    ManualSignalSpy spy;
    QDBusConnection::sessionBus().connect(TEST_SERVICE, TEST_OBJECT_PATH, "org.ayatana.dbusmenu", "ItemUpdated", &spy, SLOT(receiveCall(int)));
    QVariant empty = QVariant::fromValue(QDBusVariant(QString()));
    uint timestamp = QDateTime::currentDateTime().toTime_t();
    iface.call("Event", a2Id, "clicked", empty, timestamp);
    QTest::qWait(500);

    // Check a1 is not checked, but a2 is
    reply = iface.call("GetChildren", 0, QStringList());
    QVERIFY2(reply.isValid(), qPrintable(reply.error().message()));
    list = reply.value();
    QCOMPARE(list.count(), 2);

    item = list.takeFirst();
    QCOMPARE(item.properties.value("toggle-state").toInt(), 0);

    item = list.takeFirst();
    QCOMPARE(item.properties.value("toggle-state").toInt(), 1);

    // Did we get notified?
    QCOMPARE(spy.count(), 2);
    QSet<int> updatedIds;
    updatedIds << spy.takeFirst().at(0).toInt();
    updatedIds << spy.takeFirst().at(0).toInt();

    QSet<int> expectedIds;
    expectedIds << a1Id << a2Id;

    QCOMPARE(updatedIds, expectedIds);
}

void DBusMenuExporterTest::testClickDeletedAction()
{
    QMenu inputMenu;
    QVERIFY(QDBusConnection::sessionBus().registerService(TEST_SERVICE));
    DBusMenuExporter exporter(TEST_OBJECT_PATH, &inputMenu);

    QAction *a1 = inputMenu.addAction("a1");

    // Get id
    QDBusInterface iface(TEST_SERVICE, TEST_OBJECT_PATH);
    QDBusReply<DBusMenuItemList> reply = iface.call("GetChildren", 0, QStringList());
    QVERIFY2(reply.isValid(), qPrintable(reply.error().message()));
    DBusMenuItemList list = reply.value();
    QCOMPARE(list.count(), 1);
    int id = list.takeFirst().id;

    // Delete a1, it should not cause a crash when trying to trigger it
    delete a1;

    // Send a click to deleted a1
    QVariant empty = QVariant::fromValue(QDBusVariant(QString()));
    uint timestamp = QDateTime::currentDateTime().toTime_t();
    iface.call("Event", id, "clicked", empty, timestamp);
    QTest::qWait(500);
}

// Reproduce LP BUG 521011
// https://bugs.launchpad.net/bugs/521011
void DBusMenuExporterTest::testDeleteExporterBeforeMenu()
{
    QMenu inputMenu;
    QVERIFY(QDBusConnection::sessionBus().registerService(TEST_SERVICE));
    DBusMenuExporter *exporter = new DBusMenuExporter(TEST_OBJECT_PATH, &inputMenu);

    QAction *a1 = inputMenu.addAction("a1");
    delete exporter;
    inputMenu.removeAction(a1);
}

#include "dbusmenuexportertest.moc"

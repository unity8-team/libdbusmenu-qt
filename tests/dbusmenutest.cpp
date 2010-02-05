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
#include "dbusmenutest.h"

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

QTEST_MAIN(DBusMenuTest)

static const char *TEST_SERVICE = "org.kde.dbusmenu-qt-test";
static const char *TEST_OBJECT_PATH = "/TestMenuBar";

class TestDBusMenuImporter : public DBusMenuImporter
{
public:
    TestDBusMenuImporter(QDBusAbstractInterface *iface)
    : DBusMenuImporter(iface)
    {}

protected:
    virtual QMenu *createMenu(QWidget *parent) { return new QMenu(parent); }
    virtual QIcon iconForName(const QString &) { return QIcon(); }
};

void DBusMenuTest::init()
{
    QVERIFY(QDBusConnection::sessionBus().registerService(TEST_SERVICE));
}

void DBusMenuTest::cleanup()
{
    QVERIFY(QDBusConnection::sessionBus().unregisterService(TEST_SERVICE));
}

static QString iconForAction(const QAction *action)
{
    return action->property("icon-name").toString();
}

void DBusMenuTest::testExporter_data()
{
    QTest::addColumn<QString>("label");
    QTest::addColumn<QString>("iconName");
    QTest::addColumn<bool>("enabled");

    QTest::newRow("label only")           << "label" << QString() << true;
    QTest::newRow("disabled, label only") << "label" << QString() << false;
    QTest::newRow("icon name")            << "label" << "icon"    << true;
}

void DBusMenuTest::testExporter()
{
    QFETCH(QString, label);
    QFETCH(QString, iconName);
    QFETCH(bool, enabled);

    QMenu inputMenu;
    DBusMenuExporter exporter(QDBusConnection::sessionBus().name(), TEST_OBJECT_PATH, &inputMenu);
    exporter.setIconNameForActionFunction(iconForAction);

    QAction *action = new QAction(label, &inputMenu);
    if (!iconName.isEmpty()) {
        action->setProperty("icon-name", iconName);
    }
    action->setEnabled(enabled);
    inputMenu.addAction(action);

    QDBusInterface iface(TEST_SERVICE, TEST_OBJECT_PATH);
    QVERIFY2(iface.isValid(), qPrintable(iface.lastError().message()));

    QStringList propertyNames = QStringList() << "type" << "enabled" << "label" << "icon-name";
    QDBusReply<DBusMenuItemList> reply = iface.call("GetChildren", 0, propertyNames);
    QVERIFY2(reply.isValid(), qPrintable(reply.error().message()));

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

void DBusMenuTest::testGetAllProperties()
{
    const QSet<QString> a1Properties = QSet<QString>()
        << "label"
        ;

    const QSet<QString> a2Properties = QSet<QString>()
        << "label"
        << "enabled"
        << "icon-name"
        ;

    const QSet<QString> separatorProperties = QSet<QString>()
        << "type";

    QMenu inputMenu;
    DBusMenuExporter exporter(QDBusConnection::sessionBus().name(), TEST_OBJECT_PATH, &inputMenu);
    exporter.setIconNameForActionFunction(iconForAction);

    inputMenu.addAction("a1");
    inputMenu.addSeparator();
    QAction *a2 = new QAction("a2", &inputMenu);
    a2->setEnabled(false);
    a2->setProperty("icon-name", "foo");
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

void DBusMenuTest::testGetNonExistentProperty()
{
    const char* NON_EXISTENT_KEY = "i-do-not-exist";

    QMenu inputMenu;
    inputMenu.addAction("a1");
    DBusMenuExporter exporter(QDBusConnection::sessionBus().name(), TEST_OBJECT_PATH, &inputMenu);

    QDBusInterface iface(TEST_SERVICE, TEST_OBJECT_PATH);
    QDBusReply<DBusMenuItemList> reply = iface.call("GetChildren", 0, QStringList() << NON_EXISTENT_KEY);
    QVERIFY2(reply.isValid(), qPrintable(reply.error().message()));

    DBusMenuItemList list = reply.value();
    QCOMPARE(list.count(), 1);

    DBusMenuItem item = list.takeFirst();
    QVERIFY(!item.properties.contains(NON_EXISTENT_KEY));
}

void DBusMenuTest::testClickedEvent()
{
    QMenu inputMenu;
    QAction *action = inputMenu.addAction("a1");
    QSignalSpy spy(action, SIGNAL(triggered()));
    DBusMenuExporter exporter(QDBusConnection::sessionBus().name(), TEST_OBJECT_PATH, &inputMenu);

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

void DBusMenuTest::testSubMenu()
{
    QMenu inputMenu;
    QMenu *subMenu = inputMenu.addMenu("menu");
    QAction *a1 = subMenu->addAction("a1");
    QAction *a2 = subMenu->addAction("a2");
    DBusMenuExporter exporter(QDBusConnection::sessionBus().name(), TEST_OBJECT_PATH, &inputMenu);

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

void DBusMenuTest::testStandardItem()
{
    QMenu inputMenu;
    inputMenu.addAction("Test");
    QVERIFY(QDBusConnection::sessionBus().registerService(TEST_SERVICE));
    DBusMenuExporter exporter(QDBusConnection::sessionBus().name(), TEST_OBJECT_PATH, &inputMenu);

    QDBusInterface iface(TEST_SERVICE, TEST_OBJECT_PATH);
    TestDBusMenuImporter importer(&iface);
    QEventLoop loop;
    connect(&importer, SIGNAL(menuIsReady()), &loop, SLOT(quit()));
    loop.exec();

    QMenu *outputMenu = importer.menu();
    QCOMPARE(outputMenu->actions().count(), 1);
    QCOMPARE(outputMenu->actions().first()->text(), QString("Test"));
}

#include "dbusmenutest.moc"

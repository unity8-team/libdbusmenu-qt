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
    QAction *action = inputMenu.addAction(label);
    if (!iconName.isEmpty()) {
        action->setProperty("icon-name", iconName);
    }
    action->setEnabled(enabled);

    DBusMenuExporter exporter(QDBusConnection::sessionBus().name(), TEST_OBJECT_PATH, &inputMenu);
    exporter.setIconNameForActionFunction(iconForAction);

    QDBusInterface iface(TEST_SERVICE, TEST_OBJECT_PATH);
    QVERIFY2(iface.isValid(), qPrintable(iface.lastError().message()));

    QStringList propertyNames = QStringList() << "type" << "enabled" << "label" << "icon-name";
    QDBusReply<DBusMenuItemList> reply = iface.call("GetChildren", 0, propertyNames);
    QVERIFY2(reply.isValid(), qPrintable(reply.error().message()));

    DBusMenuItemList list = reply.value();
    QCOMPARE(list.count(), 1);
    DBusMenuItem item = list.first();
    QCOMPARE(item.properties.value("type").toString()      , QString("standard"));
    QCOMPARE(item.properties.value("label").toString()     , label);
    QCOMPARE(item.properties.value("enabled").toBool()     , enabled);
    QCOMPARE(item.properties.value("icon-name").toString() , iconName);
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

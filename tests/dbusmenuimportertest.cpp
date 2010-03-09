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

void DBusMenuImporterTest::init()
{
    QVERIFY(QDBusConnection::sessionBus().registerService(TEST_SERVICE));
}

void DBusMenuImporterTest::cleanup()
{
    QVERIFY(QDBusConnection::sessionBus().unregisterService(TEST_SERVICE));
}

void DBusMenuImporterTest::testStandardItem()
{
    QMenu inputMenu;
    inputMenu.addAction("Test");
    QVERIFY(QDBusConnection::sessionBus().registerService(TEST_SERVICE));
    DBusMenuExporter exporter(TEST_OBJECT_PATH, &inputMenu);

    QDBusInterface* iface = new QDBusInterface(TEST_SERVICE, TEST_OBJECT_PATH);
    TestDBusMenuImporter importer(iface);
    QTest::qWait(500);

    QMenu *outputMenu = importer.menu();
    QCOMPARE(outputMenu->actions().count(), 1);
    QCOMPARE(outputMenu->actions().first()->text(), QString("Test"));
}

#include "dbusmenuimportertest.moc"

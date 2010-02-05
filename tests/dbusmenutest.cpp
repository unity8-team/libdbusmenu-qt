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

void DBusMenuTest::testStandardItem()
{
    QMenu inputMenu;
    inputMenu.addAction("Test");
    QDBusConnection connection = QDBusConnection::connectToBus(QDBusConnection::SessionBus, TEST_SERVICE);
    DBusMenuExporter exporter(TEST_SERVICE, TEST_OBJECT_PATH, &inputMenu);

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

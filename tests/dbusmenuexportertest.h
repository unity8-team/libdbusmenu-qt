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
#ifndef DBUSMENUEXPORTERTEST_H
#define DBUSMENUEXPORTERTEST_H

#define QT_GUI_LIB
#include <QtGui>

// Qt
#include <QObject>

// Local

class QMenu;

class ManualSignalSpy : public QObject, public QList<QVariantList>
{
    Q_OBJECT
public Q_SLOTS:
    void receiveCall(int value)
    {
        append(QVariantList() << value);
    }

    void receiveCall(uint v1, int v2)
    {
        append(QVariantList() << v1 << v2);
    }
};

class MenuFiller : public QObject
{
    Q_OBJECT
public:
    MenuFiller(QMenu *menu)
    : m_menu(menu)
    {
        connect(m_menu, SIGNAL(aboutToShow()), SLOT(fillMenu()));
    }

    void addAction(QAction *action)
    {
        m_actions << action;
    }

public Q_SLOTS:
    void fillMenu()
    {
        while (!m_actions.isEmpty()) {
            m_menu->addAction(m_actions.takeFirst());
        }
    }

private:
    QMenu *m_menu;
    QList<QAction *> m_actions;
};

class DBusMenuExporterTest : public QObject
{
Q_OBJECT
private Q_SLOTS:
    void testGetSomeProperties();
    void testGetSomeProperties_data();
    void testGetAllProperties();
    void testGetNonExistentProperty();
    void testClickedEvent();
    void testSubMenu();
    void testDynamicSubMenu();
    void testRadioItems();
    void testNonExclusiveActionGroup();
    void testClickDeletedAction();
    void testDeleteExporterBeforeMenu();
    void testMenuShortcut();

    void init();
    void cleanup();
};

#endif /* DBUSMENUEXPORTERTEST_H */

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
#include "dbusmenu_p.h"

#include <QAction>
#include <QActionEvent>
#include <QMenu>

#include <kdebug.h>

#include "dbusmenuexporter.h"

DBusMenu::DBusMenu(QMenu *menu, DBusMenuExporter *exporter, uint parentId)
: QObject(menu)
, m_exporter(exporter)
, m_parentId(parentId)
{
    menu->installEventFilter(this);
}

DBusMenu::~DBusMenu()
{
}

bool DBusMenu::eventFilter(QObject *, QEvent *event)
{
    QActionEvent *actionEvent = 0;
    switch (event->type()) {
    case QEvent::ActionAdded:
    case QEvent::ActionChanged:
    case QEvent::ActionRemoved:
        actionEvent = static_cast<QActionEvent *>(event);
        break;
    default:
        return false;
    }
    switch (event->type()) {
    case QEvent::ActionAdded:
        addAction(actionEvent->action());
        break;
    case QEvent::ActionChanged:
        changeAction(actionEvent->action());
        break;
    case QEvent::ActionRemoved:
        removeAction(actionEvent->action());
        break;
    default:
        break;
    }
    return false;
}

void DBusMenu::addAction(QAction *action)
{
    kDebug() << "Added" << action << action->text();
    m_exporter->addAction(action, m_parentId);
}

void DBusMenu::changeAction(QAction *action)
{
    kDebug() << "Changed" << action->text();
    uint id = m_exporter->idForAction(action);
    m_exporter->emitItemUpdated(id);
}

void DBusMenu::removeAction(QAction *action)
{
    kDebug() << "Removed" << action->text();
    m_exporter->removeAction(action, m_parentId);
}

#include "dbusmenu.moc"

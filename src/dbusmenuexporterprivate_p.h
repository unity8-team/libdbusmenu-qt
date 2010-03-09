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
#ifndef DBUSMENUEXPORTERPRIVATE_P_H
#define DBUSMENUEXPORTERPRIVATE_P_H

// Qt
#include <QtCore/QHash>
#include <QtCore/QMap>
#include <QtCore/QSet>
#include <QtCore/QVariant>

// Local
#include "dbusmenuexporter.h"

class QMenu;
class QXmlStreamWriter;

class DBusMenuExporterDBus;

class DBusMenuExporterPrivate
{
public:
    DBusMenuExporter *q;

    DBusMenuExporterDBus *m_dbusObject;

    IconNameForActionFunction m_iconNameForActionFunction;

    QMenu *m_rootMenu;
    QHash<QAction *, QVariantMap> m_actionProperties;
    QMap<int, QAction *> m_actionForId;
    QMap<QAction *, int> m_idForAction;
    int m_nextId;
    int m_revision;

    QSet<int> m_itemUpdatedIds;
    QTimer *m_itemUpdatedTimer;

    int idForAction(QAction *action) const;
    void addMenu(QMenu *menu, int parentId);
    QVariantMap propertiesForAction(QAction *action) const;
    QVariantMap propertiesForKMenuTitleAction(QAction *action_) const;
    QVariantMap propertiesForSeparatorAction(QAction *action) const;
    QVariantMap propertiesForStandardAction(QAction *action) const;
    QMenu *menuForId(int id) const;
    void writeXmlForMenu(QXmlStreamWriter *writer, QMenu *menu, int id);

    void addAction(QAction *action, int parentId);
    void updateAction(QAction *action);
    void removeAction(QAction *action, int parentId);
};


#endif /* DBUSMENUEXPORTERPRIVATE_P_H */

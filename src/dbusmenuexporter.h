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
#ifndef DBUSMENUEXPORTER_H
#define DBUSMENUEXPORTER_H

// Qt
#include <QtCore/QObject>
#include <QtCore/QVariant>
#include <QtDBus/QDBusAbstractAdaptor>

// Local
#include <dbusmenu_export.h>
#include <dbusmenuitem.h>

class QAction;
class QMenu;
class QDBusVariant;

typedef QString (* IconNameForActionFunction)(const QAction *);

class DBusMenuExporterPrivate;

/**
 * Internal class exporting DBus menu changes to DBus
 */
class DBUSMENU_EXPORT DBusMenuExporter : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.ayatana.dbusmenu")
public:
    DBusMenuExporter(const QString &dbusService, QMenu *rootMenu);
    ~DBusMenuExporter();

    /**
     * Make it possible for the application to provide a function to extract an
     * icon name from an action
     */
    void setIconNameForActionFunction(IconNameForActionFunction);

    void emitLayoutUpdated(uint);
    void emitItemUpdated(uint);

    uint idForAction(QAction *) const;
    void addAction(QAction* action, uint parentId);
    void removeAction(QAction* action, uint parentId);

public Q_SLOTS:
    DBusMenuItemList GetChildren(uint parentId, const QStringList &propertyNames);
    Q_NOREPLY void Event(uint id, const QString &eventId, const QDBusVariant &data, uint timestamp);
    QDBusVariant GetProperty(uint id, const QString &property);
    QVariantMap GetProperties(uint id, const QStringList &names);
    uint GetLayout(uint parentId, QString &layout);

Q_SIGNALS:
    void ItemUpdated(uint);
    void ItemPropertyUpdated(uint, QString, QVariant);
    void LayoutUpdated(uint revision, uint parentId);

private Q_SLOTS:
    void doEmitItemUpdated();

private:
    DBusMenuExporterPrivate * const d;
};

#endif /* DBUSMENUEXPORTER_H */

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
#include <QtDBus/QDBusConnection>

// Local
#include <dbusmenu_export.h>

class QAction;
class QMenu;

typedef QString (* IconNameForActionFunction)(const QAction *);

class DBusMenuExporterPrivate;

/**
 * A DBusMenuExporter instance can serialize a menu over DBus
 */
class DBUSMENU_EXPORT DBusMenuExporter : public QObject
{
    Q_OBJECT
public:
    /**
     * Creates a DBusMenuExporter exporting menu at the dbus object path
     * dbusObjectPath, using the given dbusConnection.
     * The instance is added as a child of menu.
     */
    DBusMenuExporter(const QString &dbusObjectPath, QMenu *menu, const QDBusConnection &dbusConnection = QDBusConnection::sessionBus());
    ~DBusMenuExporter();

    /**
     * Make it possible for the application to provide a function to extract an
     * icon name from an action
     */
    void setIconNameForActionFunction(IconNameForActionFunction);

private Q_SLOTS:
    void doUpdateActions();

private:
    DBusMenuExporterPrivate *const d;

    friend class DBusMenuExporterDBus;
    friend class DBusMenu;
};

#endif /* DBUSMENUEXPORTER_H */

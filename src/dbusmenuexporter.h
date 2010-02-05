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
    DBusMenuExporter(const QString &connectionName, const QString& dbusObjectPath, QMenu *rootMenu);
    ~DBusMenuExporter();

    /**
     * Make it possible for the application to provide a function to extract an
     * icon name from an action
     */
    void setIconNameForActionFunction(IconNameForActionFunction);

    void emitLayoutUpdated(int);

    void addAction(QAction* action, int parentId);
    void updateAction(QAction *action);
    void removeAction(QAction* action, int parentId);

public Q_SLOTS:
    DBusMenuItemList GetChildren(int parentId, const QStringList &propertyNames);
    Q_NOREPLY void Event(int id, const QString &eventId, const QDBusVariant &data, int timestamp);
    QDBusVariant GetProperty(int id, const QString &property);
    QVariantMap GetProperties(int id, const QStringList &names);
    int GetLayout(int parentId, QString &layout);
    DBusMenuItemList GetGroupProperties(const QVariantList &ids, const QStringList &propertyNames);

Q_SIGNALS:
    void ItemUpdated(int);
    void ItemPropertyUpdated(int, QString, QVariant);
    void LayoutUpdated(int revision, int parentId);

private Q_SLOTS:
    void doUpdateActions();

private:
    DBusMenuExporterPrivate * const d;
};

#endif /* DBUSMENUEXPORTER_H */

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
#ifndef DBUSMENUIMPORTER_H
#define DBUSMENUIMPORTER_H

// Qt
#include <QtCore/QObject>

// Local
#include <dbusmenu_export.h>

class QDBusAbstractInterface;
class QDBusPendingCallWatcher;
class QIcon;
class QMenu;

class DBusMenuImporterPrivate;
/**
 * A DBusMenuImporter instance can recreate a menu serialized over DBus by
 * DBusMenuExporter
 */
class DBUSMENU_EXPORT DBusMenuImporter : public QObject
{
    Q_OBJECT
public:
    DBusMenuImporter(QDBusAbstractInterface *, QObject *parent = 0);
    ~DBusMenuImporter();

    QMenu *menu() const;

protected:
    /**
     * Must create a menu, may be customized to fit host appearance
     */
    virtual QMenu *createMenu(QWidget *parent) = 0;

    /**
     * Must convert a name into an icon
     */
    virtual QIcon iconForName(const QString &) = 0;

private Q_SLOTS:
    void dispatch(QDBusPendingCallWatcher *);
    void sendClickedEvent(int);
    void slotItemUpdated(int id);
    void slotMenuAboutToShow();
    void slotAboutToShowDBusCallFinished(QDBusPendingCallWatcher *);

private:
    DBusMenuImporterPrivate *const d;
    friend class DBusMenuImporterPrivate;

    void GetChildrenCallback(int id, QDBusPendingCallWatcher *);
    void GetPropertiesCallback(int id, QDBusPendingCallWatcher *);
};

#endif /* DBUSMENUIMPORTER_H */

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

#include <QtCore/QObject>

class QDBusAbstractInterface;
class QDBusPendingCallWatcher;
class QMenu;

class DBusMenuImporterPrivate;
class DBusMenuImporter : public QObject
{
    Q_OBJECT
public:
    DBusMenuImporter(QDBusAbstractInterface *, QObject *parent = 0);
    ~DBusMenuImporter();

    QMenu *menu() const;

Q_SIGNALS:
    void menuIsReady();

private Q_SLOTS:
    void dispatch(QDBusPendingCallWatcher *);
    void sendClickedEvent(int);
    void slotItemUpdated(uint id);
    void slotSubMenuAboutToShow();

private:
    DBusMenuImporterPrivate *const d;
    friend class DBusMenuImporterPrivate;

    void GetChildrenCallback(uint id, QDBusPendingCallWatcher *);
    void GetPropertiesCallback(uint id, QDBusPendingCallWatcher *);
};

#endif /* DBUSMENUIMPORTER_H */

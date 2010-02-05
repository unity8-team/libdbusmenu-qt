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
#ifndef DBUSMENUITEM_H
#define DBUSMENUITEM_H

#include <QtCore/QList>
#include <QtCore/QVariant>

class QDBusArgument;

/**
 * Internal struct used to communicate on DBus
 */
struct DBusMenuItem
{
    uint id;
    QVariantMap properties;
};

Q_DECLARE_METATYPE(DBusMenuItem)

QDBusArgument &operator<<(QDBusArgument &argument, const DBusMenuItem &item);
const QDBusArgument &operator>>(const QDBusArgument &argument, DBusMenuItem &item);

typedef QList<DBusMenuItem> DBusMenuItemList;

Q_DECLARE_METATYPE(DBusMenuItemList)

#endif /* DBUSMENUITEM_H */

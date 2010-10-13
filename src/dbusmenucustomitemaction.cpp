/* This file is part of the dbusmenu-qt library
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
#include "dbusmenucustomitemaction.h"

class DBusMenuCustomItemActionPrivate
{
public:
    QVariantMap m_properties;
};

DBusMenuCustomItemAction::DBusMenuCustomItemAction(const QVariantMap& properties, QObject* parent)
: QAction(parent)
, d(new DBusMenuCustomItemActionPrivate)
{
    d->m_properties = properties;
}

DBusMenuCustomItemAction::DBusMenuCustomItemAction(QObject* parent)
: QAction(parent)
, d(new DBusMenuCustomItemActionPrivate)
{
}

QVariantMap DBusMenuCustomItemAction::properties() const
{
    return d->m_properties;
}

void DBusMenuCustomItemAction::setProperties(const QVariantMap& properties)
{
    d->m_properties = properties;
}

#include "dbusmenucustomitemaction.moc"
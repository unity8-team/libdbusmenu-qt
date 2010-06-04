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
#include "dbusmenuexporterdbus_p.h"

// Qt
#include <QMenu>

// Local
#include "dbusmenuadaptor.h"
#include "dbusmenuexporterprivate_p.h"
#include "dbusmenushortcut_p.h"
#include "debug_p.h"

DBusMenuExporterDBus::DBusMenuExporterDBus(DBusMenuExporter *exporter)
: QObject(exporter)
, m_exporter(exporter)
{
    qDBusRegisterMetaType<DBusMenuItem>();
    qDBusRegisterMetaType<DBusMenuItemList>();
    qDBusRegisterMetaType<DBusMenuShortcut>();
    new DbusmenuAdaptor(this);
}


DBusMenuItemList DBusMenuExporterDBus::GetChildren(int parentId, const QStringList &names)
{
    DBusMenuItemList list;

    QMenu *menu = m_exporter->d->menuForId(parentId);
    if (!menu) {
        return DBusMenuItemList();
    }
    // Process pending actions, we need them *now*
    QMetaObject::invokeMethod(m_exporter, "doUpdateActions");
    Q_FOREACH(QAction *action, menu->actions()) {
        DBusMenuItem item;
        item.id = m_exporter->d->idForAction(action);
        item.properties = GetProperties(item.id, names);
        list << item;
    }
    return list;
}

uint DBusMenuExporterDBus::GetLayout(int parentId, QString &layout)
{
    QMenu *menu = m_exporter->d->menuForId(parentId);
    DMRETURN_VALUE_IF_FAIL(menu, 0);

    QXmlStreamWriter writer(&layout);
    writer.setAutoFormatting(true);
    writer.writeStartDocument();
    m_exporter->d->writeXmlForMenu(&writer, menu, parentId);
    writer.writeEndDocument();

    return m_exporter->d->m_revision;
}

void DBusMenuExporterDBus::Event(int id, const QString &eventType, const QDBusVariant &/*data*/, uint /*timestamp*/)
{
    if (eventType == "clicked") {
        QAction *action = m_exporter->d->m_actionForId.value(id);
        if (!action) {
            return;
        }
        action->trigger();
    } else if (eventType == "hovered") {
        QMenu *menu = m_exporter->d->menuForId(id);
        if (menu) {
            QMetaObject::invokeMethod(menu, "aboutToShow");
        }
    }
}

QDBusVariant DBusMenuExporterDBus::GetProperty(int id, const QString &name)
{
    QAction *action = m_exporter->d->m_actionForId.value(id);
    DMRETURN_VALUE_IF_FAIL(action, QDBusVariant());
    return QDBusVariant(m_exporter->d->m_actionProperties.value(action).value(name));
}

QVariantMap DBusMenuExporterDBus::GetProperties(int id, const QStringList &names)
{
    QAction *action = m_exporter->d->m_actionForId.value(id);
    DMRETURN_VALUE_IF_FAIL(action, QVariantMap());
    QVariantMap all = m_exporter->d->m_actionProperties.value(action);
    if (names.isEmpty()) {
        return all;
    } else {
        QVariantMap map;
        Q_FOREACH(const QString &name, names) {
            QVariant value = all.value(name);
            if (value.isValid()) {
                map.insert(name, value);
            }
        }
        return map;
    }
}

DBusMenuItemList DBusMenuExporterDBus::GetGroupProperties(const QVariantList &ids, const QStringList &names)
{
    DBusMenuItemList list;
    Q_FOREACH(const QVariant &id, ids) {
        DBusMenuItem item;
        item.id = id.toInt();
        item.properties = GetProperties(item.id, names);
        list << item;
    }
    return list;
}

/**
 * An helper class for ::AboutToShow, which sets mChanged to true if a menu
 * changes after its aboutToShow() signal has been emitted.
 */
class ActionEventFilter: public QObject
{
public:
    ActionEventFilter()
    : mChanged(false)
    {}

    bool mChanged;
protected:
    bool eventFilter(QObject *object, QEvent *event)
    {
        switch (event->type()) {
        case QEvent::ActionAdded:
        case QEvent::ActionChanged:
        case QEvent::ActionRemoved:
            mChanged = true;
            // We noticed a change, no need to filter anymore
            object->removeEventFilter(this);
            break;
        default:
            break;
        }
        return false;
    }
};

bool DBusMenuExporterDBus::AboutToShow(int id)
{
    QMenu *menu = m_exporter->d->menuForId(id);
    DMRETURN_VALUE_IF_FAIL(menu, false);

    ActionEventFilter filter;
    menu->installEventFilter(&filter);
    QMetaObject::invokeMethod(menu, "aboutToShow");
    return filter.mChanged;
}


#include "dbusmenuexporterdbus_p.moc"

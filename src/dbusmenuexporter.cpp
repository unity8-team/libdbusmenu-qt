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
#include "dbusmenuexporter.h"

// Qt
#include <QMap>
#include <QMenu>
#include <QSet>
#include <QTimer>
#include <QToolButton>
#include <QWidgetAction>
#include <QXmlStreamWriter>

// Local
#include "dbusmenu_p.h"
#include "dbusmenuitem.h"
#include "dbusmenuadaptor.h"
#include "debug_p.h"
#include "utils_p.h"

static const char *KMENU_TITLE = "kmenu_title";

static QString defaultIconNameForActionFunction(const QAction *)
{
    return QString();
}

class DBusMenuExporterPrivate
{
public:
    DBusMenuExporter *q;

    IconNameForActionFunction m_iconNameForActionFunction;

    QMenu *m_rootMenu;
    QHash<QAction *, QVariantMap> m_actionProperties;
    QMap<int, QAction *> m_actionForId;
    QMap<QAction *, int> m_idForAction;
    int m_nextId;
    int m_revision;

    QSet<int> m_itemUpdatedIds;
    QTimer *m_itemUpdatedTimer;

    int idForAction(QAction *action) const
    {
        if (!action) {
            return 0;
        }
        return m_idForAction.value(action, 0);
    }

    void addMenu(QMenu *menu, int parentId)
    {
        new DBusMenu(menu, q, parentId);
        Q_FOREACH(QAction *action, menu->actions()) {
            q->addAction(action, parentId);
        }
    }

    QVariantMap propertiesForAction(QAction *action) const
    {
        Q_ASSERT(action);

        if (action->objectName() == KMENU_TITLE) {
            // Hack: Support for KDE menu titles in a Qt-only library...
            return propertiesForKMenuTitleAction(action);
        } else if (action->isSeparator()) {
            return propertiesForSeparatorAction(action);
        } else {
            return propertiesForStandardAction(action);
        }
    }

    QVariantMap propertiesForKMenuTitleAction(QAction *action_) const
    {
        QVariantMap map;
        map.insert("type", "text");
        map.insert("enabled", false);

        const QWidgetAction *widgetAction = qobject_cast<const QWidgetAction *>(action_);
        DMRETURN_VALUE_IF_FAIL(widgetAction, map);
        QToolButton *button = qobject_cast<QToolButton *>(widgetAction->defaultWidget());
        DMRETURN_VALUE_IF_FAIL(button, map);
        QAction *action = button->defaultAction();
        DMRETURN_VALUE_IF_FAIL(action, map);

        map.insert("label", swapMnemonicChar(action->text(), '&', '_'));
        QString iconName = m_iconNameForActionFunction(action);
        if (!iconName.isEmpty()) {
            map.insert("icon-name", iconName);
        }
        return map;
    }

    QVariantMap propertiesForSeparatorAction(QAction *action) const
    {
        QVariantMap map;
        map.insert("type", "separator");
        return map;
    }

    QVariantMap propertiesForStandardAction(QAction *action) const
    {
        QVariantMap map;
        map.insert("label", swapMnemonicChar(action->text(), '&', '_'));
        if (!action->isEnabled()) {
            map.insert("enabled", false);
        }
        if (action->menu()) {
            map.insert("children-display", "submenu");
        }
        if (action->isCheckable()) {
            map.insert("toggle-type", action->actionGroup() ? "radio" : "checkmark");
            map.insert("toggle-state", action->isChecked() ? 1 : 0);
        }
        QString iconName = m_iconNameForActionFunction(action);
        if (!iconName.isEmpty()) {
            map.insert("icon-name", iconName);
        }
        return map;
    }

    QMenu *menuForId(int id) const
    {
        if (id == 0) {
            return m_rootMenu;
        }
        QAction *action = m_actionForId.value(id);
        if (!action) {
            DMDEBUG << "No action for id" << id;
            return 0;
        }
        QMenu *menu = action->menu();
        if (!menu) {
            DMDEBUG << "No children for action" << action->text() << id;
            return 0;
        }
        return menu;
    }

    void writeXmlForMenu(QXmlStreamWriter *writer, QMenu *menu, int id)
    {
        Q_ASSERT(menu);
        writer->writeStartElement("menu");
        writer->writeAttribute("id", QString::number(id));
        Q_FOREACH(QAction *action, menu->actions()) {
            int actionId = m_idForAction.value(action, 0);
            if (actionId == 0) {
                DMWARNING << "No id for action";
                continue;
            }
            QMenu *actionMenu = action->menu();
            if (actionMenu) {
                writeXmlForMenu(writer, actionMenu, actionId);
            } else {
                writer->writeEmptyElement("menu");
                writer->writeAttribute("id", QString::number(actionId));
            }
        }
        writer->writeEndElement();
    }

};


DBusMenuExporter::DBusMenuExporter(const QString &connectionName, const QString &objectPath, QMenu *rootMenu)
: QObject(rootMenu)
, d(new DBusMenuExporterPrivate)
{
    d->q = this;
    d->m_rootMenu = rootMenu;
    d->m_nextId = 1;
    d->m_revision = 1;
    d->m_itemUpdatedTimer = new QTimer(this);
    d->m_iconNameForActionFunction = defaultIconNameForActionFunction;

    qDBusRegisterMetaType<DBusMenuItem>();
    qDBusRegisterMetaType<DBusMenuItemList>();
    new DbusmenuAdaptor(this);
    QDBusConnection connection = QDBusConnection::connectToBus(QDBusConnection::SessionBus, connectionName);
    connection.registerObject(objectPath, this, QDBusConnection::ExportAllContents);
    d->addMenu(rootMenu, 0);

    d->m_itemUpdatedTimer->setInterval(0);
    d->m_itemUpdatedTimer->setSingleShot(true);
    connect(d->m_itemUpdatedTimer, SIGNAL(timeout()), SLOT(doUpdateActions()));
}

DBusMenuExporter::~DBusMenuExporter()
{
    delete d;
}

void DBusMenuExporter::setIconNameForActionFunction(IconNameForActionFunction function)
{
    Q_ASSERT(function);
    d->m_iconNameForActionFunction = function;
}

void DBusMenuExporter::emitLayoutUpdated(int id)
{
    LayoutUpdated(d->m_revision, id);
}

void DBusMenuExporter::updateAction(QAction *action)
{
    int id = d->idForAction(action);
    if (d->m_itemUpdatedIds.contains(id)) {
        DMDEBUG << id << "already in";
        return;
    }
    DMDEBUG << id;
    d->m_itemUpdatedIds << id;
    d->m_itemUpdatedTimer->start();
}

void DBusMenuExporter::doUpdateActions()
{
    Q_FOREACH(int id, d->m_itemUpdatedIds) {
        QAction *action = d->m_actionForId.value(id);
        d->m_actionProperties[action] = d->propertiesForAction(action);
        QMenu *menu = action->menu();
        if (menu && !menu->findChild<DBusMenu *>()) {
            d->addMenu(menu, id);
        }
        ItemUpdated(id);
    }
    d->m_itemUpdatedIds.clear();
}

void DBusMenuExporter::addAction(QAction *action, int parentId)
{
    QVariantMap map = d->propertiesForAction(action);
    int id = d->m_nextId++;
    d->m_actionForId.insert(id, action);
    d->m_idForAction.insert(action, id);
    d->m_actionProperties.insert(action, map);
    if (action->menu()) {
        d->addMenu(action->menu(), id);
    }
    ++d->m_revision;
    emitLayoutUpdated(parentId);
}

void DBusMenuExporter::removeAction(QAction *action, int parentId)
{
    d->m_actionProperties.remove(action);
    int id = d->m_idForAction.take(action);
    d->m_actionForId.remove(id);
    ++d->m_revision;
    emitLayoutUpdated(parentId);
}

DBusMenuItemList DBusMenuExporter::GetChildren(int parentId, const QStringList &names)
{
    DBusMenuItemList list;

    QMenu *menu = d->menuForId(parentId);
    if (!menu) {
        return DBusMenuItemList();
    }
    QMetaObject::invokeMethod(menu, "aboutToShow");
    // Process pending actions, we need them *now*
    doUpdateActions();
    Q_FOREACH(QAction *action, menu->actions()) {
        DBusMenuItem item;
        item.id = d->idForAction(action);
        item.properties = GetProperties(item.id, names);
        list << item;
    }
    return list;
}

uint DBusMenuExporter::GetLayout(int parentId, QString &layout)
{
    QMenu *menu = d->menuForId(parentId);
    if (!menu) {
        DMWARNING << "No menu for id" << parentId;
        return 0;
    }

    QXmlStreamWriter writer(&layout);
    writer.setAutoFormatting(true);
    writer.writeStartDocument();
    d->writeXmlForMenu(&writer, menu, parentId);
    writer.writeEndDocument();

    return d->m_revision;
}

void DBusMenuExporter::Event(int id, const QString &eventType, const QDBusVariant &/*data*/, uint /*timestamp*/)
{
    if (eventType == "clicked") {
        QAction *action = d->m_actionForId.value(id);
        if (!action) {
            return;
        }
        action->trigger();
    } else if (eventType == "hovered") {
        QMenu *menu = d->menuForId(id);
        if (menu) {
            QMetaObject::invokeMethod(menu, "aboutToShow");
        }
    }
}

QDBusVariant DBusMenuExporter::GetProperty(int id, const QString &name)
{
    QAction *action = d->m_actionForId.value(id);
    if (!action) {
        DMDEBUG << "No action for id" << id;
        return QDBusVariant();
    }
    return QDBusVariant(d->m_actionProperties.value(action).value(name));
}

QVariantMap DBusMenuExporter::GetProperties(int id, const QStringList &names)
{
    QAction *action = d->m_actionForId.value(id);
    if (!action) {
        DMDEBUG << "No action for id" << id;
        return QVariantMap();
    }
    QVariantMap all = d->m_actionProperties.value(action);
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

DBusMenuItemList DBusMenuExporter::GetGroupProperties(const QVariantList &ids, const QStringList &names)
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

#include "dbusmenuexporter.moc"

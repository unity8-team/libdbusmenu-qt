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


static QAction *internalActionForTitleAction(const QAction *action_)
{
    const QWidgetAction *action = qobject_cast<const QWidgetAction *>(action_);
    DMRETURN_VALUE_IF_FAIL(action, 0);
    QToolButton *button = qobject_cast<QToolButton *>(action->defaultWidget());
    DMRETURN_VALUE_IF_FAIL(button, 0);
    return button->defaultAction();
}

static QString labelForTitleAction(const QAction *action_)
{
    QAction *action = internalActionForTitleAction(action_);
    DMRETURN_VALUE_IF_FAIL(action, QString());
    return action->text();
}

class DBusMenuExporterPrivate
{
public:
    DBusMenuExporter *q;

    IconNameForActionFunction m_iconNameForActionFunction;

    QMenu *m_rootMenu;
    QMap<uint, QAction *> m_actionForId;
    QMap<QAction *, uint> m_idForAction;
    uint m_nextId;
    uint m_revision;

    QSet<uint> m_itemUpdatedIds;
    QTimer *m_itemUpdatedTimer;

    void addMenu(QMenu *menu, uint parentId)
    {
        new DBusMenu(menu, q, parentId);
        Q_FOREACH(QAction *action, menu->actions()) {
            q->addAction(action, parentId);
        }
    }

    QVariantMap propertiesForAction(QAction *action, const QStringList &names) const
    {
        Q_ASSERT(action);
        QVariantMap map;
        Q_FOREACH(const QString &name, names) {
            map.insert(name, propertyForAction(action, name));
        }
        return map;
    }

    QVariant propertyForAction(QAction *action, const QString &name) const
    {
        Q_ASSERT(action);
        // Hack: Support for KDE menu titles in a Qt-only library...
        bool isTitle = action->objectName() == KMENU_TITLE;
        if (name == "label") {
            QString text;
            if (isTitle) {
                text = labelForTitleAction(action);
            } else {
                text = action->text();
            }
            return swapMnemonicChar(text, '&', '_');
        } else if (name == "sensitive") {
            return isTitle ? false : action->isEnabled();
        } else if (name == "type") {
            if (action->menu()) {
                return "menu";
            } else if (action->isSeparator()) {
                return "separator";
            } else if (action->isCheckable()) {
                return action->actionGroup() ? "radio" : "checkbox";
            } else {
                return "action";
            }
        } else if (name == "checked") {
            return action->isChecked();
        } else if (name == "icon") {
            QAction *iconAction = isTitle ? internalActionForTitleAction(action) : action;
            return QVariant(m_iconNameForActionFunction(iconAction));
        } else if (name == "icon-data") {
        }
        DMDEBUG << "Unhandled property" << name;
        return QVariant(QString()); // DBus does not like invalid variants
    }

    QMenu *menuForId(uint id) const
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


DBusMenuExporter::DBusMenuExporter(const QString &service, QMenu *rootMenu)
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
    QDBusConnection connection(service);
    connection.registerObject("/MenuBar", this, QDBusConnection::ExportAllContents);
    d->addMenu(rootMenu, 0);

    d->m_itemUpdatedTimer->setInterval(0);
    d->m_itemUpdatedTimer->setSingleShot(true);
    connect(d->m_itemUpdatedTimer, SIGNAL(timeout()), SLOT(doEmitItemUpdated()));
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

void DBusMenuExporter::emitChildrenUpdated(uint id)
{
    ChildrenUpdated(id);
    LayoutUpdate(d->m_revision, id);
}

void DBusMenuExporter::emitItemUpdated(uint id)
{
    if (d->m_itemUpdatedIds.contains(id)) {
        DMDEBUG << id << "already in";
        return;
    }
    DMDEBUG << id;
    d->m_itemUpdatedIds << id;
    d->m_itemUpdatedTimer->start();
}

void DBusMenuExporter::doEmitItemUpdated()
{
    Q_FOREACH(uint id, d->m_itemUpdatedIds) {
        ItemUpdated(id);
    }
    d->m_itemUpdatedIds.clear();
}

void DBusMenuExporter::addAction(QAction *action, uint parentId)
{
    uint id = d->m_nextId++;
    d->m_actionForId.insert(id, action);
    d->m_idForAction.insert(action, id);
    if (action->menu()) {
        d->addMenu(action->menu(), id);
    }
    ++d->m_revision;
    emitChildrenUpdated(parentId);
}

void DBusMenuExporter::removeAction(QAction *action, uint parentId)
{
    uint id = d->m_idForAction.take(action);
    d->m_actionForId.remove(id);
    ++d->m_revision;
    emitChildrenUpdated(parentId);
}

uint DBusMenuExporter::idForAction(QAction *action) const
{
    if (!action) {
        return 0;
    }
    return d->m_idForAction.value(action, 0);
}

DBusMenuItemList DBusMenuExporter::GetChildren(uint parentId, const QStringList &names)
{
    DBusMenuItemList list;

    QMenu *menu = d->menuForId(parentId);
    if (!menu) {
        return DBusMenuItemList();
    }
    QMetaObject::invokeMethod(menu, "aboutToShow");
    Q_FOREACH(QAction *action, menu->actions()) {
        DBusMenuItem item;
        item.id = idForAction(action);
        item.properties = d->propertiesForAction(action, names);
        list << item;
    }
    return list;
}

uint DBusMenuExporter::GetLayout(uint parentId, QString &layout)
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

void DBusMenuExporter::Event(uint id, const QString &eventType, const QDBusVariant &/*data*/)
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

QDBusVariant DBusMenuExporter::GetProperty(uint id, const QString &name)
{
    QAction *action = d->m_actionForId.value(id);
    if (!action) {
        DMDEBUG << "No action for id" << id;
        return QDBusVariant();
    }
    return QDBusVariant(d->propertyForAction(action, name));
}

QVariantMap DBusMenuExporter::GetProperties(uint id, const QStringList &names)
{
    QAction *action = d->m_actionForId.value(id);
    if (!action) {
        DMDEBUG << "No action for id" << id;
        return QVariantMap();
    }
    return d->propertiesForAction(action, names);
}

#include "dbusmenuexporter.moc"

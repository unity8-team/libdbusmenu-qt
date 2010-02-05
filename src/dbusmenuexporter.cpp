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

// Local
#include "dbusmenu_p.h"
#include "dbusmenuitem.h"
#include "dbusmenuadaptor.h"
#include "debug_p.h"

static QString defaultIconNameForActionFunction(const QAction *)
{
    return QString();
}


/*
 * Swap mnemonic char: Qt uses '&', while dbusmenu uses '_'
 */
template <char src, char dst>
static QString swapMnemonicChar(const QString &in)
{
    QString out;
    bool mnemonicFound = false;

    for (int pos = 0; pos < in.length(); ) {
        QChar ch = in[pos];
        if (ch == src) {
            if (pos == in.length() - 1) {
                // 'src' at the end of string, skip it
                ++pos;
            } else {
                if (in[pos + 1] == src) {
                    // A real 'src'
                    out += src;
                    pos += 2;
                } else if (!mnemonicFound) {
                    // We found the mnemonic
                    mnemonicFound = true;
                    out += dst;
                    ++pos;
                } else {
                    // We already have a mnemonic, just skip the char
                    ++pos;
                }
            }
        } else if (ch == dst) {
            // Escape 'dst'
            out += dst;
            out += dst;
            ++pos;
        } else {
            out += ch;
            ++pos;
        }
    }

    return out;
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
        if (name == "label") {
            return swapMnemonicChar<'&', '_'>(action->text());
        } else if (name == "sensitive") {
            return action->isEnabled();
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
            return QVariant(m_iconNameForActionFunction(action));
            /*
            KIcon icon(action->icon());
            if (icon.isNull()) {
                return QVariant(QString());
            } else {
                return icon.iconName();
            }
            */
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
};


DBusMenuExporter::DBusMenuExporter(const QString &service, QMenu *rootMenu)
: QObject(rootMenu)
, d(new DBusMenuExporterPrivate)
{
    d->q = this;
    d->m_rootMenu = rootMenu;
    d->m_nextId = 1;
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
    emitChildrenUpdated(parentId);
}

void DBusMenuExporter::removeAction(QAction *action, uint parentId)
{
    uint id = d->m_idForAction.take(action);
    d->m_actionForId.remove(id);
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

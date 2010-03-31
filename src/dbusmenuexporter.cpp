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
#include "dbusmenuexporterdbus_p.h"
#include "dbusmenuexporterprivate_p.h"
#include "dbusmenuitem_p.h"
#include "debug_p.h"
#include "utils_p.h"

static const char *KMENU_TITLE = "kmenu_title";

//-------------------------------------------------
//
// DBusMenuExporterPrivate
//
//-------------------------------------------------
int DBusMenuExporterPrivate::idForAction(QAction *action) const
{
    DMRETURN_VALUE_IF_FAIL(action, -1);
    return m_idForAction.value(action, -2);
}

void DBusMenuExporterPrivate::addMenu(QMenu *menu, int parentId)
{
    new DBusMenu(menu, q, parentId);
    Q_FOREACH(QAction *action, menu->actions()) {
        addAction(action, parentId);
    }
}

QVariantMap DBusMenuExporterPrivate::propertiesForAction(QAction *action) const
{
    DMRETURN_VALUE_IF_FAIL(action, QVariantMap());

    if (action->objectName() == KMENU_TITLE) {
        // Hack: Support for KDE menu titles in a Qt-only library...
        return propertiesForKMenuTitleAction(action);
    } else if (action->isSeparator()) {
        return propertiesForSeparatorAction(action);
    } else {
        return propertiesForStandardAction(action);
    }
}

QVariantMap DBusMenuExporterPrivate::propertiesForKMenuTitleAction(QAction *action_) const
{
    QVariantMap map;
    map.insert("enabled", false);

    const QWidgetAction *widgetAction = qobject_cast<const QWidgetAction *>(action_);
    DMRETURN_VALUE_IF_FAIL(widgetAction, map);
    QToolButton *button = qobject_cast<QToolButton *>(widgetAction->defaultWidget());
    DMRETURN_VALUE_IF_FAIL(button, map);
    QAction *action = button->defaultAction();
    DMRETURN_VALUE_IF_FAIL(action, map);

    map.insert("label", swapMnemonicChar(action->text(), '&', '_'));
    QString iconName = q->iconNameForAction(action);
    if (!iconName.isEmpty()) {
        map.insert("icon-name", iconName);
    }
    return map;
}

QVariantMap DBusMenuExporterPrivate::propertiesForSeparatorAction(QAction *action) const
{
    QVariantMap map;
    map.insert("type", "separator");
    return map;
}

QVariantMap DBusMenuExporterPrivate::propertiesForStandardAction(QAction *action) const
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
    QString iconName = q->iconNameForAction(action);
    if (!iconName.isEmpty()) {
        map.insert("icon-name", iconName);
    }
    return map;
}

QMenu *DBusMenuExporterPrivate::menuForId(int id) const
{
    if (id == 0) {
        return m_rootMenu;
    }
    QAction *action = m_actionForId.value(id);
    DMRETURN_VALUE_IF_FAIL(action, 0);
    QMenu *menu = action->menu();
    DMRETURN_VALUE_IF_FAIL(menu, 0);
    return menu;
}

void DBusMenuExporterPrivate::writeXmlForMenu(QXmlStreamWriter *writer, QMenu *menu, int id)
{
    Q_ASSERT(menu);
    writer->writeStartElement("menu");
    writer->writeAttribute("id", QString::number(id));
    Q_FOREACH(QAction *action, menu->actions()) {
        int actionId = m_idForAction.value(action, -1);
        if (actionId == -1) {
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

void DBusMenuExporterPrivate::updateAction(QAction *action)
{
    int id = idForAction(action);
    if (m_itemUpdatedIds.contains(id)) {
        return;
    }
    m_itemUpdatedIds << id;
    m_itemUpdatedTimer->start();
}

void DBusMenuExporterPrivate::addAction(QAction *action, int parentId)
{
    QVariantMap map = propertiesForAction(action);
    int id = m_nextId++;
    m_actionForId.insert(id, action);
    m_idForAction.insert(action, id);
    m_actionProperties.insert(action, map);
    if (action->menu()) {
        addMenu(action->menu(), id);
    }
    ++m_revision;
    emitLayoutUpdated(parentId);
}

void DBusMenuExporterPrivate::removeAction(QAction *action, int parentId)
{
    m_actionProperties.remove(action);
    int id = m_idForAction.take(action);
    m_actionForId.remove(id);
    ++m_revision;
    emitLayoutUpdated(parentId);
}

void DBusMenuExporterPrivate::emitLayoutUpdated(int id)
{
    m_dbusObject->LayoutUpdated(m_revision, id);
}

//-------------------------------------------------
//
// DBusMenuExporter
//
//-------------------------------------------------
DBusMenuExporter::DBusMenuExporter(const QString &objectPath, QMenu *menu, const QDBusConnection &_connection)
: QObject(menu)
, d(new DBusMenuExporterPrivate)
{
    d->q = this;
    d->m_rootMenu = menu;
    d->m_nextId = 1;
    d->m_revision = 1;
    d->m_itemUpdatedTimer = new QTimer(this);
    d->m_dbusObject = new DBusMenuExporterDBus(this);

    d->addMenu(d->m_rootMenu, 0);

    d->m_itemUpdatedTimer->setInterval(0);
    d->m_itemUpdatedTimer->setSingleShot(true);
    connect(d->m_itemUpdatedTimer, SIGNAL(timeout()), SLOT(doUpdateActions()));

    QDBusConnection connection(_connection);
    connection.registerObject(objectPath, d->m_dbusObject, QDBusConnection::ExportAllContents);
}

DBusMenuExporter::~DBusMenuExporter()
{
    delete d;
}

void DBusMenuExporter::doUpdateActions()
{
    Q_FOREACH(int id, d->m_itemUpdatedIds) {
        QAction *action = d->m_actionForId.value(id);
        if (!action) {
            // Action does not exist anymore
            continue;
        }
        d->m_actionProperties[action] = d->propertiesForAction(action);
        QMenu *menu = action->menu();
        if (menu && !menu->findChild<DBusMenu *>()) {
            d->addMenu(menu, id);
        }
        d->m_dbusObject->ItemUpdated(id);
    }
    d->m_itemUpdatedIds.clear();
}

QString DBusMenuExporter::iconNameForAction(QAction * /*action*/)
{
    return QString();
}

#include "dbusmenuexporter.moc"

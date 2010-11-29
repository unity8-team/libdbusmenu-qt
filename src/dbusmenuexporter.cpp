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
#include "dbusmenuexporter.h"

// Qt
#include <QDateTime>
#include <QMap>
#include <QMenu>
#include <QSet>
#include <QTimer>
#include <QToolButton>
#include <QWidgetAction>
#include <QXmlStreamWriter>

// Local
#include "dbusmenu_config.h"
#include "dbusmenu_p.h"
#include "dbusmenuexporterdbus_p.h"
#include "dbusmenuexporterprivate_p.h"
#include "dbusmenuitem_p.h"
#include "dbusmenushortcut_p.h"
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
    if (menu->findChild<DBusMenu *>()) {
        // This can happen if a menu is removed from its parent and added back
        // See KDE bug 254066
        return;
    }
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
    map.insert("x-kde-title", true);

    const QWidgetAction *widgetAction = qobject_cast<const QWidgetAction *>(action_);
    DMRETURN_VALUE_IF_FAIL(widgetAction, map);
    QToolButton *button = qobject_cast<QToolButton *>(widgetAction->defaultWidget());
    DMRETURN_VALUE_IF_FAIL(button, map);
    QAction *action = button->defaultAction();
    DMRETURN_VALUE_IF_FAIL(action, map);

    map.insert("label", swapMnemonicChar(action->text(), '&', '_'));
    insertIconProperty(&map, action);
    if (!action->isVisible()) {
        map.insert("visible", false);
    }
    return map;
}

QVariantMap DBusMenuExporterPrivate::propertiesForSeparatorAction(QAction *action) const
{
    QVariantMap map;
    map.insert("type", "separator");
    if (!action->isVisible()) {
        map.insert("visible", false);
    }
    return map;
}

QVariantMap DBusMenuExporterPrivate::propertiesForStandardAction(QAction *action) const
{
    QVariantMap map;
    map.insert("label", swapMnemonicChar(action->text(), '&', '_'));
    if (!action->isEnabled()) {
        map.insert("enabled", false);
    }
    if (!action->isVisible()) {
        map.insert("visible", false);
    }
    if (action->menu()) {
        map.insert("children-display", "submenu");
    }
    if (action->isCheckable()) {
        bool exclusive = action->actionGroup() && action->actionGroup()->isExclusive();
        map.insert("toggle-type", exclusive ? "radio" : "checkmark");
        map.insert("toggle-state", action->isChecked() ? 1 : 0);
    }
    insertIconProperty(&map, action);
    QKeySequence keySequence = action->shortcut();
    if (!keySequence.isEmpty()) {
        DBusMenuShortcut shortcut = DBusMenuShortcut::fromKeySequence(keySequence);
        map.insert("shortcut", QVariant::fromValue(shortcut));
    }
    return map;
}

QMenu *DBusMenuExporterPrivate::menuForId(int id) const
{
    if (id == 0) {
        return m_rootMenu;
    }
    QAction *action = m_actionForId.value(id);
    // Action may not be in m_actionForId if it has been deleted between the
    // time it was announced by the exporter and the time the importer asks for
    // it.
    return action ? action->menu() : 0;
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
    int id = m_idForAction.value(action, -1);
    if (id != -1) {
        DMWARNING << "Already tracking action" << action->text() << "under id" << id;
        return;
    }
    QVariantMap map = propertiesForAction(action);
    id = m_nextId++;
    QObject::connect(action, SIGNAL(destroyed(QObject*)), q, SLOT(slotActionDestroyed(QObject*)));
    m_actionForId.insert(id, action);
    m_idForAction.insert(action, id);
    m_actionProperties.insert(action, map);
    if (action->menu()) {
        addMenu(action->menu(), id);
    }
    ++m_revision;
    emitLayoutUpdated(parentId);
}

/**
 * IMPORTANT: action might have already been destroyed when this method is
 * called, so don't dereference the pointer (it is a QObject to avoid being
 * tempted to dereference)
 */
void DBusMenuExporterPrivate::removeActionInternal(QObject *object)
{
    QAction* action = static_cast<QAction*>(object);
    m_actionProperties.remove(action);
    int id = m_idForAction.take(action);
    m_actionForId.remove(id);
}

void DBusMenuExporterPrivate::removeAction(QAction *action, int parentId)
{
    removeActionInternal(action);
    QObject::disconnect(action, SIGNAL(destroyed(QObject*)), q, SLOT(slotActionDestroyed(QObject*)));
    ++m_revision;
    emitLayoutUpdated(parentId);
}

void DBusMenuExporterPrivate::emitLayoutUpdated(int id)
{
    if (m_layoutUpdatedIds.contains(id)) {
        return;
    }
    m_layoutUpdatedIds << id;
    m_layoutUpdatedTimer->start();
}

void DBusMenuExporterPrivate::insertIconProperty(QVariantMap *map, QAction *action) const
{
    QString iconName = q->iconNameForAction(action);
    if (!iconName.isEmpty()) {
        map->insert("icon-name", iconName);
        return;
    }
    QIcon icon = action->icon();
    if (icon.isNull()) {
        return;
    }

    // "icon-data";
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
    d->m_layoutUpdatedTimer = new QTimer(this);
    d->m_dbusObject = new DBusMenuExporterDBus(this);

    d->addMenu(d->m_rootMenu, 0);

    d->m_itemUpdatedTimer->setInterval(0);
    d->m_itemUpdatedTimer->setSingleShot(true);
    connect(d->m_itemUpdatedTimer, SIGNAL(timeout()), SLOT(doUpdateActions()));

    d->m_layoutUpdatedTimer->setInterval(0);
    d->m_layoutUpdatedTimer->setSingleShot(true);
    connect(d->m_layoutUpdatedTimer, SIGNAL(timeout()), SLOT(doEmitLayoutUpdated()));

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
        if (menu) {
            d->addMenu(menu, id);
        }
        d->m_dbusObject->ItemUpdated(id);
    }
    d->m_itemUpdatedIds.clear();
}

void DBusMenuExporter::doEmitLayoutUpdated()
{
    Q_FOREACH(int id, d->m_layoutUpdatedIds) {
        d->m_dbusObject->LayoutUpdated(d->m_revision, id);
    }
    d->m_layoutUpdatedIds.clear();
}

QString DBusMenuExporter::iconNameForAction(QAction *action)
{
    DMRETURN_VALUE_IF_FAIL(action, QString());
#ifdef HAVE_QICON_NAME
    QIcon icon = action->icon();
    return icon.isNull() ? QString() : icon.name();
#else
    return QString();
#endif
}

void DBusMenuExporter::activateAction(QAction *action)
{
    int id = d->idForAction(action);
    DMRETURN_IF_FAIL(id >= 0);
    uint timeStamp = QDateTime::currentDateTime().toTime_t();
    d->m_dbusObject->ItemActivationRequested(id, timeStamp);
}

void DBusMenuExporter::slotActionDestroyed(QObject* object)
{
    d->removeActionInternal(object);
}

#include "dbusmenuexporter.moc"

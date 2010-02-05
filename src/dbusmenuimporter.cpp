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
#include "dbusmenuimporter.h"

// Qt
#include <QCoreApplication>
#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusMetaType>
#include <QDBusReply>
#include <QDBusVariant>
#include <QMenu>
#include <QSignalMapper>
#include <QTime>

// Local
#include "dbusmenuitem.h"
#include "debug_p.h"
#include "utils_p.h"

//#define BENCHMARK
#ifdef BENCHMARK
#include <QTime>
static QTime sChrono;
#endif

typedef void (DBusMenuImporter::*DBusMenuImporterMethod)(int, QDBusPendingCallWatcher*);

static const int MAX_WAIT_TIMEOUT = 50;

static const char *DBUSMENU_PROPERTY_ID = "_dbusmenu_id";
static const char *DBUSMENU_PROPERTY_ICON = "_dbusmenu_icon";

struct Task
{
    Task()
    : m_id(0)
    , m_method(0)
    {}

    int m_id;
    DBusMenuImporterMethod m_method;
};

class DBusMenuImporterPrivate
{
public:
    DBusMenuImporter *q;

    QDBusAbstractInterface *m_interface;
    QMenu *m_menu;
    QMap<QDBusPendingCallWatcher *, Task> m_taskForWatcher;
    QMap<int, QAction *> m_actionForId;
    QSignalMapper m_mapper;

    QDBusPendingCallWatcher *refresh(int id)
    {
        #ifdef BENCHMARK
        DMDEBUG << "Starting refresh chrono for id" << id;
        sChrono.start();
        #endif
        QDBusPendingCall call = m_interface->asyncCall("GetChildren", id, QStringList());
        QDBusPendingCallWatcher *watcher = new QDBusPendingCallWatcher(call, q);
        QObject::connect(watcher, SIGNAL(finished(QDBusPendingCallWatcher*)),
            q, SLOT(dispatch(QDBusPendingCallWatcher*)));

        Task task;
        task.m_id = id;
        task.m_method = &DBusMenuImporter::GetChildrenCallback;
        m_taskForWatcher.insert(watcher, task);

        return watcher;
    }

    /**
     * Init all the immutable action properties here
     * TODO: Document immutable properties?
     */
    QAction *createAction(int id, const QVariantMap &map)
    {
        QAction *action = new QAction(0);
        action->setProperty(DBUSMENU_PROPERTY_ID, id);

        QString type = map.value("type").toString();
        if (type == "separator") {
            action->setSeparator(true);
        }

        if (map.value("children-display").toString() == "submenu") {
            // FIXME: Leak?
            QMenu *menu = q->createMenu(0);
            QObject::connect(menu, SIGNAL(aboutToShow()),
                q, SLOT(slotSubMenuAboutToShow()));
            action->setMenu(menu);
        }

        QString toggleType = map.value("toggle-type").toString();
        if (!toggleType.isEmpty()) {
            action->setCheckable(true);
            if (toggleType == "radio") {
                QActionGroup *group = new QActionGroup(action);
                group->addAction(action);
            }
        }
        updateAction(action, map);

        return action;
    }

    /**
     * Update mutable properties of an action
     */
    void updateAction(QAction *action, const QVariantMap &map)
    {
        if (map.contains("label")) {
            QString text = swapMnemonicChar(map.value("label").toString(), '_', '&');
            action->setText(text);
        }

        if (map.contains("enabled")) {
            action->setEnabled(map.value("enabled").toBool());
        }

        if (action->isCheckable() && map.contains("toggle-state")) {
            action->setChecked(map.value("toggle-state").toInt() == 1);
        }

        if (map.contains("icon-name")) {
            updateActionIcon(action, map.value("icon-name").toString());
        }
    }

    void updateActionIcon(QAction *action, const QString &iconName)
    {
        QString previous = action->property(DBUSMENU_PROPERTY_ICON).toString();
        if (previous == iconName) {
            return;
        }
        action->setProperty(DBUSMENU_PROPERTY_ICON, iconName);
        if (iconName.isEmpty()) {
            action->setIcon(QIcon());
            return;
        }
        action->setIcon(q->iconForName(iconName));
    }
};

DBusMenuImporter::DBusMenuImporter(QDBusAbstractInterface *interface, QObject *parent)
: QObject(parent)
, d(new DBusMenuImporterPrivate)
{
    qDBusRegisterMetaType<DBusMenuItem>();
    qDBusRegisterMetaType<DBusMenuItemList>();

    d->q = this;
    d->m_interface = interface;
    d->m_menu = 0;

    connect(&d->m_mapper, SIGNAL(mapped(int)), SLOT(sendClickedEvent(int)));
    connect(d->m_interface, SIGNAL(ItemUpdated(int)), SLOT(slotItemUpdated(int)));

    d->refresh(0);
}

DBusMenuImporter::~DBusMenuImporter()
{
    delete d->m_menu;
    delete d;
}

QMenu *DBusMenuImporter::menu() const
{
    return d->m_menu;
}

void DBusMenuImporter::dispatch(QDBusPendingCallWatcher *watcher)
{
    Task task = d->m_taskForWatcher.take(watcher);
    if (!task.m_method) {
        DMWARNING << "No task for watcher!";
        return;
    }
    (this->*task.m_method)(task.m_id, watcher);
}

void DBusMenuImporter::slotItemUpdated(int id)
{
    QAction *action = d->m_actionForId.value(id);
    if (!action) {
        DMWARNING << "No action for id" << id;
        return;
    }

    QStringList names;
    names << "label" << "enabled";
    if (action->isCheckable()) {
        names << "checked";
    }

    #ifdef BENCHMARK
    DMDEBUG << "- Starting item update chrono for id" << id;
    #endif

    QDBusPendingCall call = d->m_interface->asyncCall("GetProperties", id, names);
    QDBusPendingCallWatcher* watcher = new QDBusPendingCallWatcher(call, this);
    connect(watcher, SIGNAL(finished(QDBusPendingCallWatcher*)),
        SLOT(dispatch(QDBusPendingCallWatcher*)));

    Task task;
    task.m_id = id;
    task.m_method = &DBusMenuImporter::GetPropertiesCallback;
    d->m_taskForWatcher.insert(watcher, task);
}

void DBusMenuImporter::GetPropertiesCallback(int id, QDBusPendingCallWatcher *watcher)
{
    #ifdef BENCHMARK
    DMDEBUG << "- Parsing updated properties for id" << id << sChrono.elapsed() << "ms";
    #endif
    QDBusReply<QVariantMap> reply = *watcher;
    if (!reply.isValid()) {
        DMWARNING << reply.error().message();
        return;
    }

    QVariantMap properties = reply.value();

    QAction *action = d->m_actionForId.value(id);
    if (!action) {
        DMWARNING << "No action for id" << id;
        return;
    }
    d->updateAction(action, properties);
    #ifdef BENCHMARK
    DMDEBUG << "- Item updated" << id << sChrono.elapsed() << "ms";
    #endif
}

void DBusMenuImporter::GetChildrenCallback(int parentId, QDBusPendingCallWatcher *watcher)
{
    bool wasWaitingForMenu = false;
    QDBusReply<DBusMenuItemList> reply = *watcher;
    if (!reply.isValid()) {
        DMWARNING << reply.error().message();
        return;
    }

    #ifdef BENCHMARK
    DMDEBUG << "- items received:" << sChrono.elapsed() << "ms";
    #endif
    DBusMenuItemList list = reply.value();

    QMenu *menu = 0;
    if (parentId == 0) {
        if (!d->m_menu) {
            d->m_menu = createMenu(0);
            wasWaitingForMenu = true;
        }
        menu = d->m_menu;
    } else {
        QAction *action = d->m_actionForId.value(parentId);
        if (!action) {
            DMWARNING << "No action for id" << parentId;
            return;
        }
        menu = action->menu();
        if (!menu) {
            DMWARNING << "Action" << action->text() << "has no menu";
            return;
        }
    }
    Q_ASSERT(menu);

    menu->clear();

    Q_FOREACH(const DBusMenuItem &dbusMenuItem, list) {
        QAction *action = d->createAction(dbusMenuItem.id, dbusMenuItem.properties);
        d->m_actionForId.insert(dbusMenuItem.id, action);
        menu->addAction(action);

        connect(action, SIGNAL(triggered()),
            &d->m_mapper, SLOT(map()));
        d->m_mapper.setMapping(action, dbusMenuItem.id);
    }
    #ifdef BENCHMARK
    DMDEBUG << "- Menu filled:" << sChrono.elapsed() << "ms";
    #endif

    if (wasWaitingForMenu) {
        emit menuIsReady();
    }
}

void DBusMenuImporter::sendClickedEvent(int id)
{
    DMDEBUG << id;
    QVariant empty = QVariant::fromValue(QDBusVariant(QString()));
    uint timestamp = QDateTime::currentDateTime().toTime_t();
    d->m_interface->asyncCall("Event", id, QString("clicked"), empty, timestamp);
}

void DBusMenuImporter::slotSubMenuAboutToShow()
{
    QMenu *menu = qobject_cast<QMenu*>(sender());
    Q_ASSERT(menu);

    QAction *action = menu->menuAction();
    Q_ASSERT(action);

    int id = action->property(DBUSMENU_PROPERTY_ID).toInt();

    QDBusPendingCallWatcher *watcher = d->refresh(id);

    QTime time;
    time.start();
    while (!watcher->isFinished() && time.elapsed() < MAX_WAIT_TIMEOUT) {
        QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
    }

    // Tricky: watcher has indicated it is finished, but slots connected to its
    // finished() signal have not been called yet. Calling waitForFinished()
    // ensures they get called.
    if (watcher->isFinished()) {
        watcher->waitForFinished();
    } else {
        DMWARNING << "Application did not answer to GetChildren() before timeout";
    }
    #ifdef BENCHMARK
    DMVAR(time.elapsed());
    #endif
}

#include "dbusmenuimporter.moc"

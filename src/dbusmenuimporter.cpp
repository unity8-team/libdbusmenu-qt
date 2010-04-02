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
#include "dbusmenuitem_p.h"
#include "debug_p.h"
#include "utils_p.h"

//#define BENCHMARK
#ifdef BENCHMARK
#include <QTime>
static QTime sChrono;
#endif

typedef void (DBusMenuImporter::*DBusMenuImporterMethod)(int, QDBusPendingCallWatcher*);

static const char *DBUSMENU_INTERFACE = "org.ayatana.dbusmenu";

static const int ABOUT_TO_SHOW_TIMEOUT = 10;
static const int REFRESH_TIMEOUT = 100;

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

    QMenu *createMenu(QWidget *parent)
    {
        QMenu *menu = q->createMenu(0);
        QObject::connect(menu, SIGNAL(aboutToShow()),
            q, SLOT(slotMenuAboutToShow()));
        return menu;
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
            QMenu *menu = createMenu(0);
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
        updateAction(action, map, map.keys());

        return action;
    }

    /**
     * Update mutable properties of an action. A property may be listed in
     * requestedProperties but not in map, this means we should use the default value
     * for this property.
     *
     * @param action the action to update
     * @param map holds the property values
     * @param requestedProperties which properties has been requested
     */
    void updateAction(QAction *action, const QVariantMap &map, const QStringList &requestedProperties)
    {
        if (requestedProperties.contains("label")) {
            updateActionLabel(action, map.value("label"));
        }

        if (requestedProperties.contains("enabled")) {
            updateActionEnabled(action, map.value("enabled"));
        }

        if (requestedProperties.contains("toggle-state")) {
            updateActionChecked(action, map.value("toggle-state"));
        }

        if (requestedProperties.contains("icon-name")) {
            updateActionIcon(action, map.value("icon-name"));
        }

        if (requestedProperties.contains("visible")) {
            updateActionVisible(action, map.value("visible"));
        }
    }

    void updateActionLabel(QAction *action, const QVariant &value)
    {
        QString text = swapMnemonicChar(value.toString(), '_', '&');
        action->setText(text);
    }

    void updateActionEnabled(QAction *action, const QVariant &value)
    {
        action->setEnabled(value.isValid() ? value.toBool(): true);
    }

    void updateActionChecked(QAction *action, const QVariant &value)
    {
        if (action->isCheckable() && value.isValid()) {
            action->setChecked(value.toInt() == 1);
        }
    }

    void updateActionIcon(QAction *action, const QVariant &value)
    {
        QString iconName = value.toString();
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

    void updateActionVisible(QAction *action, const QVariant &value)
    {
        action->setVisible(value.isValid() ? value.toBool() : true);
    }

    QMenu *menuForId(int id) const
    {
        if (id == 0) {
            return q->menu();
        }
        QAction *action = m_actionForId.value(id);
        DMRETURN_VALUE_IF_FAIL(action, 0);
        return action->menu();
    }
};

DBusMenuImporter::DBusMenuImporter(const QString &service, const QString &path, QObject *parent)
: QObject(parent)
, d(new DBusMenuImporterPrivate)
{
    qDBusRegisterMetaType<DBusMenuItem>();
    qDBusRegisterMetaType<DBusMenuItemList>();

    d->q = this;
    d->m_interface = new QDBusInterface(service, path, DBUSMENU_INTERFACE, QDBusConnection::sessionBus(), this);
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
    if (!d->m_menu) {
        d->m_menu = d->createMenu(0);
    }
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
    names << "label" << "enabled" << "visible";
    if (action->isCheckable()) {
        names << "toggle-state";
    }

    #ifdef BENCHMARK
    DMDEBUG << "- Starting item update chrono for id" << id;
    #endif

    QDBusPendingCall call = d->m_interface->asyncCall("GetProperties", id, names);
    QDBusPendingCallWatcher* watcher = new QDBusPendingCallWatcher(call, this);

    // Keep a trace of which properties we requested because if we request the
    // value for a property but receive nothing it must be interpreted as "use
    // the default value" rather than "ignore this property"
    watcher->setProperty("requestedProperties", names);
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
    QStringList requestedProperties = watcher->property("requestedProperties").toStringList();
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
    d->updateAction(action, properties, requestedProperties);
    #ifdef BENCHMARK
    DMDEBUG << "- Item updated" << id << sChrono.elapsed() << "ms";
    #endif
}

void DBusMenuImporter::GetChildrenCallback(int parentId, QDBusPendingCallWatcher *watcher)
{
    QDBusReply<DBusMenuItemList> reply = *watcher;
    if (!reply.isValid()) {
        DMWARNING << reply.error().message();
        return;
    }

    #ifdef BENCHMARK
    DMDEBUG << "- items received:" << sChrono.elapsed() << "ms";
    #endif
    DBusMenuItemList list = reply.value();

    QMenu *menu = d->menuForId(parentId);
    DMRETURN_IF_FAIL(menu);

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
}

void DBusMenuImporter::sendClickedEvent(int id)
{
    DMDEBUG << id;
    QVariant empty = QVariant::fromValue(QDBusVariant(QString()));
    uint timestamp = QDateTime::currentDateTime().toTime_t();
    d->m_interface->asyncCall("Event", id, QString("clicked"), empty, timestamp);
}

static bool waitForWatcher(QDBusPendingCallWatcher *watcher, int maxWait)
{
    QTime time;
    time.start();
    while (!watcher->isFinished() && time.elapsed() < maxWait) {
        QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
    }

    // Tricky: watcher has indicated it is finished, but its finished() signal
    // has not been emitted yet. Calling waitForFinished() ensures it is
    // emitted.
    if (watcher->isFinished()) {
        watcher->waitForFinished();
        return true;
    } else {
        return false;
    }
}

void DBusMenuImporter::slotMenuAboutToShow()
{
    QMenu *menu = qobject_cast<QMenu*>(sender());
    Q_ASSERT(menu);

    QAction *action = menu->menuAction();
    Q_ASSERT(action);

    int id = action->property(DBUSMENU_PROPERTY_ID).toInt();

    QDBusPendingCall call = d->m_interface->asyncCall("AboutToShow", id);
    QDBusPendingCallWatcher *watcher = new QDBusPendingCallWatcher(call, this);
    watcher->setProperty(DBUSMENU_PROPERTY_ID, id);
    connect(watcher, SIGNAL(finished(QDBusPendingCallWatcher*)),
        SLOT(slotAboutToShowDBusCallFinished(QDBusPendingCallWatcher*)));

    if (!waitForWatcher(watcher, ABOUT_TO_SHOW_TIMEOUT)) {
        DMWARNING << "Application did not answer to AboutToShow() before timeout";
    }
    #ifdef BENCHMARK
    DMVAR(time.elapsed());
    #endif
}

void DBusMenuImporter::slotAboutToShowDBusCallFinished(QDBusPendingCallWatcher *watcher)
{
    int id = watcher->property(DBUSMENU_PROPERTY_ID).toInt();

    QDBusPendingReply<bool> reply = *watcher;
    if (reply.isError()) {
        DMWARNING << "Call to AboutToShow() failed:" << reply.error().message();
        return;
    }
    bool needRefresh = reply.argumentAt<0>();

    QMenu *menu = d->menuForId(id);
    DMRETURN_IF_FAIL(menu);

    if (needRefresh || menu->actions().isEmpty()) {
        DMDEBUG << "Menu" << id << "must be refreshed";
        watcher = d->refresh(id);
        if (!waitForWatcher(watcher, REFRESH_TIMEOUT)) {
            DMWARNING << "Application did not refresh before timeout";
        }
    }
}

QMenu *DBusMenuImporter::createMenu(QWidget *parent)
{
    return new QMenu(parent);
}

QIcon DBusMenuImporter::iconForName(const QString &/*name*/)
{
    return QIcon();
}

#include "dbusmenuimporter.moc"

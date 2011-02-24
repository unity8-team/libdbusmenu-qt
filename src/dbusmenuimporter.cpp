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
#include "dbusmenuimporter.h"

// Qt
#include <QCoreApplication>
#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusMetaType>
#include <QDBusReply>
#include <QDBusVariant>
#include <QFont>
#include <QMenu>
#include <QPointer>
#include <QSignalMapper>
#include <QTime>
#include <QTimer>
#include <QToolButton>
#include <QWidgetAction>

// Local
#include "dbusmenuitem_p.h"
#include "dbusmenushortcut_p.h"
#include "debug_p.h"
#include "utils_p.h"

//#define BENCHMARK
#ifdef BENCHMARK
static QTime sChrono;
#endif

typedef void (DBusMenuImporter::*DBusMenuImporterMethod)(int, QDBusPendingCallWatcher*);

static const char *DBUSMENU_INTERFACE = "com.canonical.dbusmenu";

static const int ABOUT_TO_SHOW_TIMEOUT = 3000;
static const int REFRESH_TIMEOUT = 4000;

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

static QAction *createKdeTitle(QAction *action, QWidget *parent)
{
    QToolButton *titleWidget = new QToolButton(0);
    QFont font = titleWidget->font();
    font.setBold(true);
    titleWidget->setFont(font);
    titleWidget->setIcon(action->icon());
    titleWidget->setText(action->text());
    titleWidget->setDown(true);
    titleWidget->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);

    QWidgetAction *titleAction = new QWidgetAction(parent);
    titleAction->setDefaultWidget(titleWidget);
    return titleAction;
}

class DBusMenuImporterPrivate
{
public:
    DBusMenuImporter *q;

    QDBusAbstractInterface *m_interface;
    QMenu *m_menu;
    QMap<QDBusPendingCallWatcher *, Task> m_taskForWatcher;
    typedef QMap<int, QPointer<QAction> > ActionForId;
    ActionForId m_actionForId;
    QSignalMapper m_mapper;
    QTimer *m_pendingLayoutUpdateTimer;

    QSet<int> m_idsRefreshedByAboutToShow;
    QSet<int> m_pendingLayoutUpdates;

    bool m_mustEmitMenuUpdated;

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
        QMenu *menu = q->createMenu(parent);
        QObject::connect(menu, SIGNAL(aboutToShow()),
            q, SLOT(slotMenuAboutToShow()));
        return menu;
    }

    /**
     * Init all the immutable action properties here
     * TODO: Document immutable properties?
     *
     * Note: we remove properties we handle from the map (using QMap::take()
     * instead of QMap::value()) to avoid warnings about these properties in
     * updateAction()
     */
    QAction *createAction(int id, const QVariantMap &_map, QWidget *parent)
    {
        QVariantMap map = _map;
        QAction *action = new QAction(parent);
        action->setProperty(DBUSMENU_PROPERTY_ID, id);

        QString type = map.take("type").toString();
        if (type == "separator") {
            action->setSeparator(true);
        }

        if (map.take("children-display").toString() == "submenu") {
            QMenu *menu = createMenu(parent);
            action->setMenu(menu);
        }

        QString toggleType = map.take("toggle-type").toString();
        if (!toggleType.isEmpty()) {
            action->setCheckable(true);
            if (toggleType == "radio") {
                QActionGroup *group = new QActionGroup(action);
                group->addAction(action);
            }
        }

        bool isKdeTitle = map.take("x-kde-title").toBool();
        updateAction(action, map, map.keys());

        if (isKdeTitle) {
            action = createKdeTitle(action, parent);
        }

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
        Q_FOREACH(const QString &key, requestedProperties) {
            updateActionProperty(action, key, map.value(key));
        }
    }

    void updateActionProperty(QAction *action, const QString &key, const QVariant &value)
    {
        if (key == "label") {
            updateActionLabel(action, value);
        } else if (key == "enabled") {
            updateActionEnabled(action, value);
        } else if (key == "toggle-state") {
            updateActionChecked(action, value);
        } else if (key == "icon-name") {
            updateActionIcon(action, value);
        } else if (key == "visible") {
            updateActionVisible(action, value);
        } else if (key == "shortcut") {
            updateActionShortcut(action, value);
        } else {
            DMWARNING << "Unhandled property update" << key;
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

    void updateActionShortcut(QAction *action, const QVariant &value)
    {
        QDBusArgument arg = value.value<QDBusArgument>();
        DBusMenuShortcut dmShortcut;
        arg >> dmShortcut;
        QKeySequence keySequence = dmShortcut.toKeySequence();
        action->setShortcut(keySequence);
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
    d->m_mustEmitMenuUpdated = false;

    connect(&d->m_mapper, SIGNAL(mapped(int)), SLOT(sendClickedEvent(int)));

    d->m_pendingLayoutUpdateTimer = new QTimer(this);
    d->m_pendingLayoutUpdateTimer->setSingleShot(true);
    connect(d->m_pendingLayoutUpdateTimer, SIGNAL(timeout()), SLOT(processPendingLayoutUpdates()));

    // For some reason, using QObject::connect() does not work but
    // QDBusConnect::connect() does
    QDBusConnection::sessionBus().connect(service, path, DBUSMENU_INTERFACE, "ItemUpdated", "i",
        this, SLOT(slotItemUpdated(int)));
    QDBusConnection::sessionBus().connect(service, path, DBUSMENU_INTERFACE, "LayoutUpdated", "ui",
        this, SLOT(slotLayoutUpdated(uint, int)));
    QDBusConnection::sessionBus().connect(service, path, DBUSMENU_INTERFACE, "ItemPropertyUpdated", "isv",
        this, SLOT(slotItemPropertyUpdated(int, const QString &, const QDBusVariant &)));
    QDBusConnection::sessionBus().connect(service, path, DBUSMENU_INTERFACE, "ItemActivationRequested", "iu",
        this, SLOT(slotItemActivationRequested(int, uint)));

    d->refresh(0);
}

DBusMenuImporter::~DBusMenuImporter()
{
    // Do not use "delete d->m_menu": even if we are being deleted we should
    // leave enough time for the menu to finish what it was doing, for example
    // if it was being displayed.
    d->m_menu->deleteLater();
    delete d;
}

void DBusMenuImporter::slotLayoutUpdated(uint revision, int parentId)
{
    if (d->m_idsRefreshedByAboutToShow.remove(parentId)) {
        return;
    }
    d->m_pendingLayoutUpdates << parentId;
    if (!d->m_pendingLayoutUpdateTimer->isActive()) {
        d->m_pendingLayoutUpdateTimer->start();
    }
}

void DBusMenuImporter::processPendingLayoutUpdates()
{
    QSet<int> ids = d->m_pendingLayoutUpdates;
    d->m_pendingLayoutUpdates.clear();
    Q_FOREACH(int id, ids) {
        d->refresh(id);
    }
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

void DBusMenuImporter::slotItemPropertyUpdated(int id, const QString &key, const QDBusVariant &value)
{
    QAction *action = d->m_actionForId.value(id);
    if (!action) {
        DMWARNING << "No action for id" << id;
        return;
    }
    d->updateActionProperty(action, key, value.variant());
}

void DBusMenuImporter::slotItemActivationRequested(int id, uint /*timestamp*/)
{
    QAction *action = d->m_actionForId.value(id);
    DMRETURN_IF_FAIL(action);
    actionActivationRequested(action);
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
        QAction *action = d->createAction(dbusMenuItem.id, dbusMenuItem.properties, menu);
        DBusMenuImporterPrivate::ActionForId::Iterator it = d->m_actionForId.find(dbusMenuItem.id);
        if (it == d->m_actionForId.end()) {
            d->m_actionForId.insert(dbusMenuItem.id, action);
        } else {
            delete *it;
            *it = action;
        }
        menu->addAction(action);

        connect(action, SIGNAL(triggered()),
            &d->m_mapper, SLOT(map()));
        d->m_mapper.setMapping(action, dbusMenuItem.id);

        if (action->menu()) {
            d->refresh(dbusMenuItem.id);
        }
    }
    #ifdef BENCHMARK
    DMDEBUG << "- Menu filled:" << sChrono.elapsed() << "ms";
    #endif
}

void DBusMenuImporter::sendClickedEvent(int id)
{
    QVariant empty = QVariant::fromValue(QDBusVariant(QString()));
    uint timestamp = QDateTime::currentDateTime().toTime_t();
    d->m_interface->asyncCall("Event", id, QString("clicked"), empty, timestamp);
}

void DBusMenuImporter::updateMenu()
{
    d->m_mustEmitMenuUpdated = true;
    QMetaObject::invokeMethod(menu(), "aboutToShow");
}

static bool waitForWatcher(QDBusPendingCallWatcher * _watcher, int maxWait)
{
    QTime time;
    time.start();
    QPointer<QDBusPendingCallWatcher> watcher(_watcher);
    while (watcher && !watcher->isFinished() && time.elapsed() < maxWait) {
        QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
    }

    if (!watcher) {
        // Watcher died. This can happen if importer got deleted while we were
        // waiting. See:
        // https://bugs.kde.org/show_bug.cgi?id=237156
        return false;
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

    #ifdef BENCHMARK
    QTime time;
    time.start();
    #endif

    QDBusPendingCall call = d->m_interface->asyncCall("AboutToShow", id);
    QDBusPendingCallWatcher *watcher = new QDBusPendingCallWatcher(call, this);
    watcher->setProperty(DBUSMENU_PROPERTY_ID, id);
    connect(watcher, SIGNAL(finished(QDBusPendingCallWatcher*)),
        SLOT(slotAboutToShowDBusCallFinished(QDBusPendingCallWatcher*)));

    QPointer<QObject> guard(this);

    if (!waitForWatcher(watcher, ABOUT_TO_SHOW_TIMEOUT)) {
        DMWARNING << "Application did not answer to AboutToShow() before timeout";
    }

    #ifdef BENCHMARK
    DMVAR(time.elapsed());
    #endif
    // "this" got deleted during the call to waitForWatcher(), get out
    if (!guard) {
        return;
    }

    if (menu == d->m_menu && d->m_mustEmitMenuUpdated) {
        d->m_mustEmitMenuUpdated = false;
        menuUpdated();
    }
    if (menu == d->m_menu) {
        menuReadyToBeShown();
    }
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
        d->m_idsRefreshedByAboutToShow << id;
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

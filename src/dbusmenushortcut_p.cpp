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
#include "dbusmenushortcut_p.h"

// Qt
#include <QtGui/QKeySequence>

// Local
#include "debug_p.h"

// X11
#include <X11/XF86keysym.h>
#include <X11/XKBlib.h>
#define XK_MISCELLANY
#define XK_LATIN1
#define XK_KOREAN
#define XK_XKB_KEYS
#include <X11/keysymdef.h>
#include <X11/Xlib.h>
#include "keytable.cpp"

enum TableColumn { X11_COLUMN = 0, QT_COLUMN };

static int lookupKeysym(int key, TableColumn src, TableColumn dst)
{
    int idx = 0;
    while (KeyTbl[idx]) {
        if (key == (int)KeyTbl[idx + src]) {
            return (int)KeyTbl[idx + dst];
        }
        idx += 2;
    }
    return key;
}

static QStringList stringListFromQtKey(int key)
{
    QStringList lst;
    #define EXTRACT_MODIFIER(qtModifier, string) \
        if (key & qtModifier) { \
            lst << string; \
            key -= qtModifier; \
        }
    EXTRACT_MODIFIER(Qt::CTRL,  "Control");
    EXTRACT_MODIFIER(Qt::META,  "Super");
    EXTRACT_MODIFIER(Qt::ALT,   "Alt");
    EXTRACT_MODIFIER(Qt::SHIFT, "Shift");
    #undef EXTRACT_MODIFIER

    if (key != 0) {
        int xKeysym = lookupKeysym(key, QT_COLUMN, X11_COLUMN);
        lst << XKeysymToString(xKeysym);
    }
    return lst;
}

DBusMenuShortcut DBusMenuShortcut::fromKeySequence(const QKeySequence& sequence)
{
    DBusMenuShortcut shortcut;
    for (uint idx = 0; idx < sequence.count(); ++idx) {
        shortcut << stringListFromQtKey(sequence[idx]);
    }
    return shortcut;
}

static int qtKeyFromStringList(const QStringList& tokens)
{
    int key = 0;
    Q_FOREACH(const QString& token, tokens) {
        #define EXTRACT_MODIFIER(qtModifier, string) \
            if (token == string) { \
                key |= qtModifier; \
                continue; \
            }
        EXTRACT_MODIFIER(Qt::CTRL,  "Control");
        EXTRACT_MODIFIER(Qt::META,  "Super");
        EXTRACT_MODIFIER(Qt::ALT,   "Alt");
        EXTRACT_MODIFIER(Qt::SHIFT, "Shift");
        #undef EXTRACT_MODIFIER

        int xKeysym = XStringToKeysym(token.toUtf8().constData());
        key |= lookupKeysym(xKeysym, X11_COLUMN, QT_COLUMN);
    }
    return key;
}

QKeySequence DBusMenuShortcut::toKeySequence() const
{
    QVector<int> keys(4);
    keys.fill(0);
    int idx = 0;
    Q_FOREACH(const QStringList& tokens, *this) {
        keys[idx] = qtKeyFromStringList(tokens);
        ++idx;
        if (idx == 4) {
            break;
        }
    }
    return QKeySequence(keys[0], keys[1], keys[2], keys[3]);
}

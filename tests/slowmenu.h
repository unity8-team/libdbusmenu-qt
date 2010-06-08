#ifndef SLOWMENU_H
#define SLOWMENU_H

#include <QMenu>

class SlowMenu : public QMenu
{
Q_OBJECT
public:
    SlowMenu();

public Q_SLOTS:
    void slotAboutToShow();
};


#endif /* SLOWMENU_H */

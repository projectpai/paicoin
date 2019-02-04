#include "paicoindialog.h"
#include "authmanager.h"
#include "guiutil.h"

#include <QCoreApplication>

PaicoinDialog::PaicoinDialog(QWidget *parent) :
    QDialog(parent, Qt::WindowTitleHint | Qt::WindowSystemMenuHint | Qt::WindowMinMaxButtonsHint | Qt::WindowCloseButtonHint)
{
    installEventFilter(this);
    connect(&AuthManager::getInstance(), SIGNAL(Authenticate()), this, SLOT(hide()));
}

bool PaicoinDialog::eventFilter(QObject *object, QEvent *event)
{
    if (GUIUtil::isInteractionEvent(event))
        AuthManager::getInstance().RetriggerTimer();

    return QDialog::eventFilter(object, event);
}

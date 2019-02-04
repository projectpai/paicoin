#ifndef CONFIRMATIONDIALOG_H
#define CONFIRMATIONDIALOG_H

#include "paicoindialog.h"

namespace Ui {
class ConfirmationDialog;
}

class ConfirmationDialog : public PaicoinDialog
{
    Q_OBJECT

public:
    explicit ConfirmationDialog(const QString &title, QWidget *parent = nullptr);
    ~ConfirmationDialog();

private:
    Ui::ConfirmationDialog *ui;
};

#endif // CONFIRMATIONDIALOG_H

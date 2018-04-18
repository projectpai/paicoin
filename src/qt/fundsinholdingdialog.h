// Copyright (c) 2018 The PAIcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef FUNDSINHOLDINGDIALOG_H
#define FUNDSINHOLDINGDIALOG_H

#include <QDialog>
#include <QCloseEvent>

namespace Ui {
class FundsInHoldingDialog;
}

class FundsInHoldingDialog : public QDialog
{
    Q_OBJECT

public:
    explicit FundsInHoldingDialog(QWidget *parent = 0);
    ~FundsInHoldingDialog();

public Q_SLOTS:
    virtual int exec();
    virtual void accept();

private:
    Ui::FundsInHoldingDialog *ui;
};

#endif // FUNDSINHOLDINGDIALOG_H

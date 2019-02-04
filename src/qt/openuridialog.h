// Copyright (c) 2011-2015 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef PAICOIN_QT_OPENURIDIALOG_H
#define PAICOIN_QT_OPENURIDIALOG_H

#include "paicoindialog.h"

namespace Ui {
    class OpenURIDialog;
}

class OpenURIDialog : public PaicoinDialog
{
    Q_OBJECT

public:
    explicit OpenURIDialog(QWidget *parent);
    virtual ~OpenURIDialog() override;

    QString getURI();

protected Q_SLOTS:
    void accept() override;

private Q_SLOTS:
    void on_selectFileButton_clicked();

private:
    Ui::OpenURIDialog *ui;
};

#endif // PAICOIN_QT_OPENURIDIALOG_H

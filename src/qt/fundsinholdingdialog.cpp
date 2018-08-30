// Copyright (c) 2018 The PAI Coin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "fundsinholdingdialog.h"
#include "ui_fundsinholdingdialog.h"

#include <QSettings>

FundsInHoldingDialog::FundsInHoldingDialog(QWidget *parent) :
    QDialog(parent, Qt::WindowTitleHint | Qt::WindowSystemMenuHint | Qt::WindowMinMaxButtonsHint | Qt::WindowCloseButtonHint),
    ui(new Ui::FundsInHoldingDialog)
{
    ui->setupUi(this);

    QPainterPath path;
    path.addRoundedRect(QRectF(rect()), 5, 5);
    QRegion region(path.toFillPolygon().toPolygon());
    setMask(region);
    setWindowFlags(windowFlags() | Qt::FramelessWindowHint);
}

FundsInHoldingDialog::~FundsInHoldingDialog()
{
    delete ui;
}

int FundsInHoldingDialog::exec()
{
    QSettings settings;
    if (settings.contains("nDontShowFundsInHolding") && settings.value("nDontShowFundsInHolding").toBool())
        return -1;
    return QDialog::exec();
}

void FundsInHoldingDialog::accept()
{
    QSettings settings;
    settings.setValue("nDontShowFundsInHolding", ui->dontShowThisAgainCheckBox->isChecked());
    QDialog::accept();
}

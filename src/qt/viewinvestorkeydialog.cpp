#include "viewinvestorkeydialog.h"
#include "ui_viewinvestorkeydialog.h"

#include "guiutil.h"

#define PUBKEY_SPLIT_INDEX 33

ViewInvestorKeyDialog::ViewInvestorKeyDialog(QString investorKey, QWidget *parent) :
    QDialog(parent),
    ui(new Ui::ViewInvestorKeyDialog)
{
    ui->setupUi(this);
    ui->labelInvestorKey->setText(investorKey.insert(PUBKEY_SPLIT_INDEX, '\n'));

    connect(ui->pushButtonCopyInvestorKey, SIGNAL(clicked()), this, SLOT(onCopyInvestorKeyClicked()));
}

ViewInvestorKeyDialog::~ViewInvestorKeyDialog()
{
    delete ui;
}

void ViewInvestorKeyDialog::setModel(WalletModel *model)
{
    this->model = model;
}

void ViewInvestorKeyDialog::onCopyInvestorKeyClicked()
{
    GUIUtil::setClipboard(ui->labelInvestorKey->text().remove(PUBKEY_SPLIT_INDEX, 1));
}

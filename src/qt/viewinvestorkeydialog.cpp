#include "viewinvestorkeydialog.h"
#include "ui_viewinvestorkeydialog.h"

#include "guiutil.h"

static constexpr int PUBKEY_SPLIT_INDEX = 33;

ViewInvestorKeyDialog::ViewInvestorKeyDialog(WalletModel *walletModel, QWidget *parent) :
    QDialog(parent),
    ui(new Ui::ViewInvestorKeyDialog)
{
    ui->setupUi(this);
    ui->labelTitle->setText(tr("Investor key:"));

    CPubKey pubKey;
    if (walletModel->getInvestorKey(pubKey)) {
        investorKey = GUIUtil::formatPubKey(pubKey);
        ui->labelInvestorKey->setText(QString(investorKey).insert(PUBKEY_SPLIT_INDEX, '\n'));
    } else {
        investorKey.clear();
        ui->labelInvestorKey->setText(tr("Unavailable"));
    }

    connect(ui->pushButtonCopyInvestorKey, SIGNAL(clicked()), this, SLOT(onCopyInvestorKeyClicked()));
}

ViewInvestorKeyDialog::~ViewInvestorKeyDialog()
{
    delete ui;
}

void ViewInvestorKeyDialog::onCopyInvestorKeyClicked()
{
    if (!investorKey.isEmpty())
    {
        GUIUtil::setClipboard(investorKey);
        ui->labelTitle->setText(tr("Investor key copied!"));
    }
}

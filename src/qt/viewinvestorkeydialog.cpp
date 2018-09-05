#include "viewinvestorkeydialog.h"
#include "ui_viewinvestorkeydialog.h"
#include "walletmodel.h"

#include "guiutil.h"

static constexpr int PUBKEY_SPLIT_INDEX = 33;

ViewInvestorKeyDialog::ViewInvestorKeyDialog(QWidget *parent) :
    QDialog(parent, Qt::WindowTitleHint | Qt::WindowSystemMenuHint | Qt::WindowMinMaxButtonsHint | Qt::WindowCloseButtonHint),
    ui(new Ui::ViewInvestorKeyDialog)
{
    ui->setupUi(this);
    setModel(nullptr);

    connect(ui->pushButtonCopyInvestorKey, SIGNAL(clicked()), this, SLOT(onCopyInvestorKeyClicked()));
}

void ViewInvestorKeyDialog::setModel(WalletModel *model)
{
    ui->labelTitle->setText(tr("Investor key:"));

    CPubKey pubKey;
    if (model != nullptr && model->getInvestorKey(pubKey)) {
        investorKey = GUIUtil::formatPubKey(pubKey);
        ui->labelInvestorKey->setText(QString(investorKey).insert(PUBKEY_SPLIT_INDEX, '\n'));
    } else {
        investorKey.clear();
        ui->labelInvestorKey->setText(tr("Unavailable"));
    }
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

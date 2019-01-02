#include "viewinvestorkeydialog.h"
#include "ui_viewinvestorkeydialog.h"

#include "authmanager.h"
#include "guiutil.h"
#include "walletmodel.h"

static constexpr int PUBKEY_SPLIT_INDEX = 33;

ViewInvestorKeyDialog::ViewInvestorKeyDialog(QWidget *parent) :
    PaicoinDialog(parent),
    ui(new Ui::ViewInvestorKeyDialog)
{
    ui->setupUi(this);
    setModel(nullptr);

    connect(ui->pushButtonCopyInvestorKey, SIGNAL(clicked()), this, SLOT(onCopyInvestorKeyClicked()));
    connect(&AuthManager::getInstance(), SIGNAL(Authenticate()), this, SLOT(reject()));
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

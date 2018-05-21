#include "walletselectionpage.h"
#include "ui_walletselectionpage.h"

WalletSelectionPage::WalletSelectionPage(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::WalletSelectionPage)
{
    ui->setupUi(this);

    connect(ui->pushButtonCreate, SIGNAL(clicked()), this, SLOT(PushButtonCreateClicked()));
    connect(ui->pushButtonRestore, SIGNAL(clicked()), this, SLOT(PushButtonRestoreClicked()));
}

WalletSelectionPage::~WalletSelectionPage()
{
    delete ui;
}

void WalletSelectionPage::PushButtonCreateClicked()
{
    Q_EMIT goToCreateNewWallet();
}

void WalletSelectionPage::PushButtonRestoreClicked()
{
    Q_EMIT goToRestoreWallet();
}

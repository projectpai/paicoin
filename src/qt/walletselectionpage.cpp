#include "walletselectionpage.h"
#include "ui_walletselectionpage.h"

WalletSelectionPage::WalletSelectionPage(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::WalletSelectionPage)
{
    ui->setupUi(this);

    connect(ui->pushButtonCreate, SIGNAL(clicked()), this, SLOT(on_pushButtonCreate_clicked()));
    connect(ui->pushButtonRestore, SIGNAL(clicked()), this, SLOT(on_pushButtonRestore_clicked()));
}

WalletSelectionPage::~WalletSelectionPage()
{
    delete ui;
}

void WalletSelectionPage::on_pushButtonCreate_clicked()
{
    Q_EMIT goToCreateNewWallet();
}

void WalletSelectionPage::on_pushButtonRestore_clicked()
{
    Q_EMIT toToRestoreWallet();
}

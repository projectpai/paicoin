#include "restorewalletpage.h"
#include "ui_restorewalletpage.h"

RestoreWalletPage::RestoreWalletPage(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::RestoreWalletPage)
{
    ui->setupUi(this);

    QString hintFormat = QString(tr("Word #%1"));
    ui->lineEdit01->setPlaceholderText(hintFormat.arg(QString::number(1)));
    ui->lineEdit02->setPlaceholderText(hintFormat.arg(QString::number(2)));
    ui->lineEdit03->setPlaceholderText(hintFormat.arg(QString::number(3)));
    ui->lineEdit04->setPlaceholderText(hintFormat.arg(QString::number(4)));
    ui->lineEdit05->setPlaceholderText(hintFormat.arg(QString::number(5)));
    ui->lineEdit06->setPlaceholderText(hintFormat.arg(QString::number(6)));
    ui->lineEdit07->setPlaceholderText(hintFormat.arg(QString::number(7)));
    ui->lineEdit08->setPlaceholderText(hintFormat.arg(QString::number(8)));
    ui->lineEdit09->setPlaceholderText(hintFormat.arg(QString::number(9)));
    ui->lineEdit10->setPlaceholderText(hintFormat.arg(QString::number(10)));
    ui->lineEdit11->setPlaceholderText(hintFormat.arg(QString::number(11)));
    ui->lineEdit12->setPlaceholderText(hintFormat.arg(QString::number(12)));

    connect(ui->pushButtonBack, SIGNAL(clicked()), this, SLOT(onBackClicked()));
    connect(ui->pushButtonRestoreWallet, SIGNAL(clicked()), this, SLOT(onRestoreWalletClicked()));
}

RestoreWalletPage::~RestoreWalletPage()
{
    delete ui;
}

void RestoreWalletPage::onRestoreWalletClicked()
{
    paperKeys.clear();
    paperKeys << ui->lineEdit01->text() << ui->lineEdit02->text() << ui->lineEdit03->text() <<
                 ui->lineEdit04->text() << ui->lineEdit05->text() << ui->lineEdit06->text() <<
                 ui->lineEdit07->text() << ui->lineEdit08->text() << ui->lineEdit09->text() <<
                 ui->lineEdit10->text() << ui->lineEdit11->text() << ui->lineEdit12->text();

    bool nonEmptyStrings = true;
    for (QString word : paperKeys)
    {
        if (word.isEmpty())
        {
            nonEmptyStrings = false;
            break;
        }
    }

    if (nonEmptyStrings)
        Q_EMIT restoreWallet(paperKeys);
}

void RestoreWalletPage::onBackClicked()
{
    Q_EMIT backToPreviousPage();
}

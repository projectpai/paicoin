#include "welcomepage.h"
#include "ui_welcomepage.h"

WelcomePage::WelcomePage(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::WelcomePage)
{
    ui->setupUi(this);

    connect(ui->pushButtonEasy, SIGNAL(clicked()), this, SLOT(on_pushButtonEasy_clicked()));
    connect(ui->pushButtonAdvanced, SIGNAL(clicked()), this, SLOT(on_pushButtonAdvanced_clicked()));
}

WelcomePage::~WelcomePage()
{
    delete ui;
}

void WelcomePage::on_pushButtonEasy_clicked()
{
    Q_EMIT goToWalletSelection();
}

void WelcomePage::on_pushButtonAdvanced_clicked()
{
    Q_EMIT goToIntro();
}

#include "welcomepage.h"
#include "ui_welcomepage.h"

#include "intro.h"
#include "versioncheckutil.h"
#include "updateavailabledialog.h"

WelcomePage::WelcomePage(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::WelcomePage)
{
    ui->setupUi(this);

    connect(ui->pushButtonEasy, SIGNAL(clicked()), this, SLOT(on_pushButtonEasy_clicked()));
    connect(ui->pushButtonAdvanced, SIGNAL(clicked()), this, SLOT(on_pushButtonAdvanced_clicked()), Qt::UniqueConnection);
    connect(&VersionCheckUtil::getInstance(), SIGNAL(UpdateNeeded()), this, SLOT(ShowUpdateAvailableDialog()));
}

WelcomePage::~WelcomePage()
{
    delete ui;
}

void WelcomePage::on_pushButtonEasy_clicked()
{
    Intro::pickDataDirectory(true);
    Q_EMIT goToWalletSelection();
}

void WelcomePage::on_pushButtonAdvanced_clicked()
{
    Q_EMIT goToIntro();
}

void WelcomePage::ShowUpdateAvailableDialog()
{
    disconnect(&VersionCheckUtil::getInstance(), SIGNAL(UpdateNeeded()),
               this, SLOT(ShowUpdateAvailableDialog()));
    UpdateAvailableDialog dialog(this);
    dialog.exec();
}

void WelcomePage::showEvent(QShowEvent *event)
{
    VersionCheckUtil::getInstance().Check();
}

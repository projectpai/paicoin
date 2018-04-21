#include "paperkeyintropage.h"
#include "ui_paperkeyintropage.h"

PaperKeyIntroPage::PaperKeyIntroPage(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::PaperKeyIntroPage)
{
    ui->setupUi(this);

    connect(ui->pushButtonBack, SIGNAL(clicked()), this, SLOT(onBackClicked()));
    connect(ui->pushButtonWriteDown, SIGNAL(clicked()), this, SLOT(onWriteDownClicked()));
}

PaperKeyIntroPage::~PaperKeyIntroPage()
{
    delete ui;
}

void PaperKeyIntroPage::onWriteDownClicked()
{
    Q_EMIT writeDownsClicked();
}

void PaperKeyIntroPage::onBackClicked()
{
    Q_EMIT backToPreviousPage();
}

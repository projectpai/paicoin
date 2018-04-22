#include "paperkeywritedownpage.h"
#include "ui_paperkeywritedownpage.h"

PaperKeyWritedownPage::PaperKeyWritedownPage(QStringList words, QWidget *parent) :
    QWidget(parent),
    ui(new Ui::PaperKeyWritedownPage),
    paperKeyList(words),
    currentWordCount(1)
{
    ui->setupUi(this);

    updateCurrentWordAndCount();

    connect(ui->pushButtonBack, SIGNAL(clicked()), this, SLOT(onBackClicked()));
    connect(ui->pushButtonPreviousWord, SIGNAL(clicked()), this, SLOT(onPreviousClicked()));
    connect(ui->pushButtonNextWord, SIGNAL(clicked()), this, SLOT(onNextClicked()));
}

PaperKeyWritedownPage::~PaperKeyWritedownPage()
{
    delete ui;
}

void PaperKeyWritedownPage::onPreviousClicked()
{
    currentWordCount--;
    updateCurrentWordAndCount();
}

void PaperKeyWritedownPage::onNextClicked()
{
    if (currentWordCount < PAPER_KEY_WORD_COUNT)
    {
        currentWordCount++;
        updateCurrentWordAndCount();
    }
    else
    {
        Q_EMIT paperKeyWritten();
    }
}

void PaperKeyWritedownPage::onBackClicked()
{
    Q_EMIT backToPreviousPage();
}

void PaperKeyWritedownPage::updateCurrentWordAndCount()
{
    if (currentWordCount == 1)
        ui->pushButtonPreviousWord->hide();
    else if (ui->pushButtonPreviousWord->isHidden())
        ui->pushButtonPreviousWord->show();
    ui->labelWord->setText(paperKeyList.at(currentWordCount - 1));
    QString progress = tr("%1 of %2");
    ui->labelProgress->setText(progress.arg(QString::number(currentWordCount), QString::number(PAPER_KEY_WORD_COUNT)));
}

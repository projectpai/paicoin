#include "paperkeywritedownpage.h"
#include "ui_paperkeywritedownpage.h"

#include <sstream>

PaperKeyWritedownPage::PaperKeyWritedownPage(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::PaperKeyWritedownPage),
    currentWordCount(1)
{
    ui->setupUi(this);

    connectCustomSignalsAndSlots();
}

PaperKeyWritedownPage::PaperKeyWritedownPage(QStringList words, QWidget *parent) :
    QWidget(parent),
    ui(new Ui::PaperKeyWritedownPage),
    paperKeyList(words),
    currentWordCount(1)
{
    ui->setupUi(this);

    updateCurrentWordAndCount();
    connectCustomSignalsAndSlots();
}

PaperKeyWritedownPage::PaperKeyWritedownPage(std::string phrase, QWidget *parent) :
    QWidget(parent),
    ui(new Ui::PaperKeyWritedownPage),
    currentWordCount(1)
{
    ui->setupUi(this);

    setPhrase(phrase);

    connectCustomSignalsAndSlots();
}

PaperKeyWritedownPage::~PaperKeyWritedownPage()
{
    delete ui;
}

void PaperKeyWritedownPage::setPhrase(const QStringList &phrase)
{
    paperKeyList = phrase;
    updateCurrentWordAndCount();
}

void PaperKeyWritedownPage::setPhrase(const std::string &phrase)
{
    std::stringstream phraseStream(phrase);
    std::string item;
    while (std::getline(phraseStream, item, ' ')) {
        paperKeyList << item.c_str();
    }
    updateCurrentWordAndCount();
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
        currentWordCount = 1;
        updateCurrentWordAndCount();
        Q_EMIT paperKeyWritten(paperKeyList);
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

void PaperKeyWritedownPage::connectCustomSignalsAndSlots()
{
    connect(ui->pushButtonBack, SIGNAL(clicked()), this, SLOT(onBackClicked()));
    connect(ui->pushButtonPreviousWord, SIGNAL(clicked()), this, SLOT(onPreviousClicked()));
    connect(ui->pushButtonNextWord, SIGNAL(clicked()), this, SLOT(onNextClicked()));
}

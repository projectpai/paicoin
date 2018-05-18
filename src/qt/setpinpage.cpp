#include "setpinpage.h"
#include "ui_setpinpage.h"

#include <QPainter>
#include <QPixmap>
#include <QKeyEvent>

SetPinPage::SetPinPage(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::SetPinPage),
    pixmapBlack(new QPixmap(Ui::DotWidth, Ui::DotHeight)),
    pixmapGray(new QPixmap(Ui::DotWidth, Ui::DotHeight)),
    pageState(PinPageState::Init),
    pin(""),
    pinToVerify(""),
    initialPinEntered(false)
{
    ui->setupUi(this);
    qApp->installEventFilter(this);

    connect(this, SIGNAL(digitClicked(char)), this, SLOT(onDigitClicked(char)));
    connect(this, SIGNAL(backspaceClicked()), this, SLOT(onBackspaceClicked()));
    connect(this, SIGNAL(pinEntered()), this, SLOT(onPinEntered()));
    connect(this, SIGNAL(pinReEntered()), this, SLOT(onPinReEntered()));
    connect(ui->pushButton, SIGNAL(clicked()), this, SLOT(onBackClicked()));

    pixmapBlack->fill(Qt::transparent);
    pixmapGray->fill(Qt::transparent);

    painterBlack = new QPainter(pixmapBlack);
    painterGray = new QPainter(pixmapGray);

    painterBlack->setBrush(QBrush(Qt::black));
    painterGray->setBrush(QBrush(QColor(155, 155, 155)));

    painterBlack->setPen(Qt::NoPen);
    painterBlack->setRenderHint(QPainter::Antialiasing, true);
    painterBlack->drawEllipse(4, 4, 21, 21);

    painterGray->setPen(Qt::NoPen);
    painterGray->setRenderHint(QPainter::Antialiasing, true);
    painterGray->drawEllipse(4, 4, 21, 21);
}

SetPinPage::~SetPinPage()
{
    delete ui;
}

void SetPinPage::paintEvent(QPaintEvent *event)
{
    QWidget::paintEvent(event);

    int numOfSelectedDots = (pageState == PinPageState::Init || pageState == PinPageState::RequiredEntry)
            ? pin.length() : pinToVerify.length();
    ui->labelDot1->setPixmap(numOfSelectedDots <= 0 ? *pixmapGray : *pixmapBlack);
    numOfSelectedDots--;
    ui->labelDot2->setPixmap(numOfSelectedDots <= 0 ? *pixmapGray : *pixmapBlack);
    numOfSelectedDots--;
    ui->labelDot3->setPixmap(numOfSelectedDots <= 0 ? *pixmapGray : *pixmapBlack);
    numOfSelectedDots--;
    ui->labelDot4->setPixmap(numOfSelectedDots <= 0 ? *pixmapGray : *pixmapBlack);
    numOfSelectedDots--;
    ui->labelDot5->setPixmap(numOfSelectedDots <= 0 ? *pixmapGray : *pixmapBlack);
    numOfSelectedDots--;
    ui->labelDot6->setPixmap(numOfSelectedDots <= 0 ? *pixmapGray : *pixmapBlack);
}

bool SetPinPage::eventFilter(QObject* obj, QEvent* event)
{
    if(event->type() == QEvent::KeyPress)
    {
        QKeyEvent* key = static_cast<QKeyEvent*>(event);
        if (key->key() >= Qt::Key_0 && key->key() <= Qt::Key_9)
        {
            Q_EMIT digitClicked(char(key->key()));
            return true;
        }
        else if (key->key() == Qt::Key_Backspace)
        {
            Q_EMIT backspaceClicked();
            return true;
        }
    }
    return QObject::eventFilter(obj, event);
}

void SetPinPage::initSetPinLayout()
{
    pageState = PinPageState::Init;
    pin.clear();
    pinToVerify.clear();
    ui->labelTitle->setText(tr("Set PIN"));
    ui->labelNotice->show();
    if (!ui->pushButton->isVisible())
        ui->pushButton->show();
}

void SetPinPage::initReEnterPinLayout()
{
    pageState = PinPageState::ReEnter;
    pinToVerify.clear();
    ui->labelTitle->setText(tr("Re-Enter PIN"));
    ui->labelNotice->hide();
    if (!ui->pushButton->isVisible())
        ui->pushButton->show();
}

void SetPinPage::initPinRequiredLayout()
{
    pageState = PinPageState::RequiredEntry;
    pin.clear();
    ui->labelTitle->setText(tr("PIN Required"));
    ui->labelSubtitle->setText(tr("Please enter your PIN to continue"));
    ui->labelNotice->hide();
    ui->pushButton->hide();
}

void SetPinPage::onDigitClicked(char digit)
{
    if (pageState == PinPageState::Init || pageState == PinPageState::RequiredEntry)
    {
        if (pin.length() < 6)
            pin.append(digit);
        if (pin.length() == 6)
        {
            Q_EMIT pinEntered();
        }
    }
    else if (pageState == PinPageState::ReEnter)
    {
        if (pinToVerify.length() < 6)
            pinToVerify.append(digit);
        if (pinToVerify.length() == 6)
        {
            Q_EMIT pinReEntered();
        }
    }
    repaint();
}

void SetPinPage::onBackspaceClicked()
{
    if (pageState == PinPageState::Init)
    {
        pin.remove(pin.length() - 1, 1);
    }
    else if (pageState == PinPageState::ReEnter)
    {
        pinToVerify.remove(pinToVerify.length() - 1, 1);
    }
    repaint();
}

void SetPinPage::onPinEntered()
{
    if (pageState == PinPageState::Init)
    {
        // Rebuild UI for re-enter PIN functionality
        initReEnterPinLayout();
    }
    else if (pageState == PinPageState::RequiredEntry)
    {
        Q_EMIT pinReadyForAuthentication(pin.toStdString());
    }
    repaint();
}

void SetPinPage::onPinReEntered()
{
    // Rebuild UI for re-enter PIN functionality
    pageState = PinPageState::ReadyToVerify;

    if (pin.compare(pinToVerify) == 0)
    {
        Q_EMIT pinReadyForConfirmation(pin.toStdString());
        initSetPinLayout(); // Re-init page in the case of getting back to previous screen
    }
    else
    {
        Q_EMIT pinValidationFailed();
    }

    repaint();
}

void SetPinPage::onBackClicked()
{
    if (pageState == PinPageState::Init)
    {
        Q_EMIT backToPreviousPage();
    }
    else
    {
        pageState = PinPageState::Init;
        initSetPinLayout();
        repaint();
    }
}

#include "restorewalletpage.h"
#include "ui_restorewalletpage.h"

RestoreWalletPage::RestoreWalletPage(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::RestoreWalletPage)
{
    ui->setupUi(this);

    QString hintFormat = QString(tr("Word #%1"));
    ui->lineEdit01->setPlaceholderText(hintFormat.arg(QString::number(1)));
    ui->lineEdit01->installEventFilter(this);
    ui->lineEdit02->setPlaceholderText(hintFormat.arg(QString::number(2)));
    ui->lineEdit02->installEventFilter(this);
    ui->lineEdit03->setPlaceholderText(hintFormat.arg(QString::number(3)));
    ui->lineEdit03->installEventFilter(this);
    ui->lineEdit04->setPlaceholderText(hintFormat.arg(QString::number(4)));
    ui->lineEdit04->installEventFilter(this);
    ui->lineEdit05->setPlaceholderText(hintFormat.arg(QString::number(5)));
    ui->lineEdit05->installEventFilter(this);
    ui->lineEdit06->setPlaceholderText(hintFormat.arg(QString::number(6)));
    ui->lineEdit06->installEventFilter(this);
    ui->lineEdit07->setPlaceholderText(hintFormat.arg(QString::number(7)));
    ui->lineEdit07->installEventFilter(this);
    ui->lineEdit08->setPlaceholderText(hintFormat.arg(QString::number(8)));
    ui->lineEdit08->installEventFilter(this);
    ui->lineEdit09->setPlaceholderText(hintFormat.arg(QString::number(9)));
    ui->lineEdit09->installEventFilter(this);
    ui->lineEdit10->setPlaceholderText(hintFormat.arg(QString::number(10)));
    ui->lineEdit10->installEventFilter(this);
    ui->lineEdit11->setPlaceholderText(hintFormat.arg(QString::number(11)));
    ui->lineEdit11->installEventFilter(this);
    ui->lineEdit12->setPlaceholderText(hintFormat.arg(QString::number(12)));
    ui->lineEdit12->installEventFilter(this);

    connect(ui->pushButtonBack, SIGNAL(clicked()), this, SLOT(onBackClicked()));
    connect(ui->pushButtonRestoreWallet, SIGNAL(clicked()), this, SLOT(onRestoreWalletClicked()));
}

RestoreWalletPage::~RestoreWalletPage()
{
    delete ui;
}

bool RestoreWalletPage::eventFilter(QObject *object, QEvent *event)
{
    if (object != nullptr && event != nullptr)
    {
        if (event->type() == QEvent::FocusOut && typeid(*object) == typeid(QLineEdit))
        {
            QLineEdit* currentLineEdit = (QLineEdit*) object;
            if (currentLineEdit->text().isEmpty())
            {
                currentLineEdit->setStyleSheet("color:black;selection-background-color: darkgray;");
            }
            else
            {
                if (b39.WordIsValid(currentLineEdit->text().toStdString().c_str()))
                {
                    currentLineEdit->setStyleSheet("color:black;");
                }
                else
                {
                    currentLineEdit->setStyleSheet("color:red;");
                }
            }
        }
    }
    return false;
}

void RestoreWalletPage::onRestoreWalletClicked()
{
    paperKeys.clear();
    paperKeys << ui->lineEdit01->text() << ui->lineEdit02->text() << ui->lineEdit03->text()
              << ui->lineEdit04->text() << ui->lineEdit05->text() << ui->lineEdit06->text()
              << ui->lineEdit07->text() << ui->lineEdit08->text() << ui->lineEdit09->text()
              << ui->lineEdit10->text() << ui->lineEdit11->text() << ui->lineEdit12->text();

    bool nonEmptyStrings = true;
    for (QString word : paperKeys)
    {
        if (word.isEmpty() || !b39.WordIsValid(word.toStdString().c_str()))
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

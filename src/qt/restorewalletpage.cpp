#include "restorewalletpage.h"
#include "ui_restorewalletpage.h"
#include <QMessageBox>

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

void RestoreWalletPage::clear()
{
    ui->lineEdit01->clear();
    ui->lineEdit01->setStyleSheet("color:black;");
    ui->lineEdit02->clear();
    ui->lineEdit02->setStyleSheet("color:black;");
    ui->lineEdit03->clear();
    ui->lineEdit03->setStyleSheet("color:black;");
    ui->lineEdit04->clear();
    ui->lineEdit04->setStyleSheet("color:black;");
    ui->lineEdit05->clear();
    ui->lineEdit05->setStyleSheet("color:black;");
    ui->lineEdit06->clear();
    ui->lineEdit06->setStyleSheet("color:black;");
    ui->lineEdit07->clear();
    ui->lineEdit07->setStyleSheet("color:black;");
    ui->lineEdit08->clear();
    ui->lineEdit08->setStyleSheet("color:black;");
    ui->lineEdit09->clear();
    ui->lineEdit09->setStyleSheet("color:black;");
    ui->lineEdit10->clear();
    ui->lineEdit10->setStyleSheet("color:black;");
    ui->lineEdit11->clear();
    ui->lineEdit11->setStyleSheet("color:black;");
    ui->lineEdit12->clear();
    ui->lineEdit12->setStyleSheet("color:black;");

    validLineSet.clear();
    ui->pushButtonRestoreWallet->setEnabled(false);
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
                validLineSet.erase(currentLineEdit);
            }
            else
            {
                if (b39.WordIsValid(currentLineEdit->text().toStdString().c_str()))
                {
                    currentLineEdit->setStyleSheet("color:black;");
                    validLineSet.insert(currentLineEdit);
                }
                else
                {
                    currentLineEdit->setStyleSheet("color:red;");
                    validLineSet.erase(currentLineEdit);
                }
            }
        }
    }

    ui->pushButtonRestoreWallet->setEnabled(false);
    if (validLineSet.size() == 12 ) // only if all 12 lineEdits are valid we enable the button
        ui->pushButtonRestoreWallet->setEnabled(true);

    return false;
}

void RestoreWalletPage::onRestoreWalletClicked()
{
    QStringList paperKeys;
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

    if (!nonEmptyStrings)
      return;

    std::string phrase = paperKeys.join(' ').toStdString();
    if (!b39.PhraseIsValid(phrase.c_str()))
    {
        QMessageBox::warning(this
                             , tr("Warning")
                             , tr("The Paper Key you entered is invalid. Please double-check each word and try again."));
        return;
    }

    Q_EMIT restoreWallet(phrase);
}

void RestoreWalletPage::onBackClicked()
{
    Q_EMIT backToPreviousPage();
}

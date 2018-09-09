#include "reviewpaperkeydialog.h"
#include "ui_reviewpaperkeydialog.h"
#include "walletmodel.h"
#include "authmanager.h"

#include <sstream>

ReviewPaperKeyDialog::ReviewPaperKeyDialog(QWidget *parent) :
    QDialog(parent, Qt::WindowTitleHint | Qt::WindowSystemMenuHint | Qt::WindowMinMaxButtonsHint | Qt::WindowCloseButtonHint),
    ui(new Ui::ReviewPaperKeyDialog)
{
    ui->setupUi(this);

    setModel(nullptr);

    connect(&AuthManager::getInstance(), SIGNAL(Authenticate()), this, SLOT(reject()));
}

ReviewPaperKeyDialog::~ReviewPaperKeyDialog()
{
    delete ui;
}

void ReviewPaperKeyDialog::setModel(WalletModel *walletModel)
{
    if (walletModel == nullptr)
    {
        ui->labelPaperKey->setText(tr("Unavailable"));
    }
    else
    {
        SecureString secPaperKey = walletModel->getCurrentPaperKey();
        std::string paperKey(secPaperKey.begin(), secPaperKey.end());

        QString message = "<style>" \
                          "table { width: 100%; }"  \
                          "td { padding-right: 20px; }"  \
                          "</style>" \
                          "<table><tr>";

        std::stringstream stream(paperKey);
        std::string word;
        for (int i = 1; std::getline(stream, word, ' '); ++i ) {
            message += QString("<td>%1) %2</td>").arg( QString::number(i), word.c_str());
            message += (i % 3 == 0) ? "</tr><tr>" : "";
        }
        message += "</tr></table>";

        ui->labelPaperKey->setText(message);
    }
}

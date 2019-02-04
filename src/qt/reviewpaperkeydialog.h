#ifndef REVIEWPAPERKEYDIALOG_H
#define REVIEWPAPERKEYDIALOG_H

#include "paicoindialog.h"

class WalletModel;

namespace Ui {
class ReviewPaperKeyDialog;
}

class ReviewPaperKeyDialog : public PaicoinDialog
{
    Q_OBJECT

public:
    explicit ReviewPaperKeyDialog(QWidget *parent = nullptr);
    ~ReviewPaperKeyDialog();

    void setModel(WalletModel *walletModel);

private:
    Ui::ReviewPaperKeyDialog *ui;
};

#endif // REVIEWPAPERKEYDIALOG_H

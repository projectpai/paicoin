#ifndef REVIEWPAPERKEYDIALOG_H
#define REVIEWPAPERKEYDIALOG_H

#include <QDialog>

class WalletModel;

namespace Ui {
class ReviewPaperKeyDialog;
}

class ReviewPaperKeyDialog : public QDialog
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

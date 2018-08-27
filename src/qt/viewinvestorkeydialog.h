#ifndef VIEWINVESTORKEYDIALOG_H
#define VIEWINVESTORKEYDIALOG_H

#include <QDialog>
#include "walletmodel.h"

namespace Ui {
class ViewInvestorKeyDialog;
}

class ViewInvestorKeyDialog : public QDialog
{
    Q_OBJECT

public:
    explicit ViewInvestorKeyDialog(WalletModel *walletModel, QWidget *parent = nullptr);
    ~ViewInvestorKeyDialog();

private Q_SLOTS:
    void onCopyInvestorKeyClicked();

private:
    Ui::ViewInvestorKeyDialog *ui;
    QString investorKey;
};

#endif // VIEWINVESTORKEYDIALOG_H

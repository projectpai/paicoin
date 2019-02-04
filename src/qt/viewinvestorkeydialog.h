#ifndef VIEWINVESTORKEYDIALOG_H
#define VIEWINVESTORKEYDIALOG_H

#include "paicoindialog.h"

class WalletModel;

namespace Ui {
class ViewInvestorKeyDialog;
}

class ViewInvestorKeyDialog : public PaicoinDialog
{
    Q_OBJECT

public:
    explicit ViewInvestorKeyDialog(QWidget *parent);
    ~ViewInvestorKeyDialog();

    void setModel(WalletModel *model);

private Q_SLOTS:
    void onCopyInvestorKeyClicked();

private:
    Ui::ViewInvestorKeyDialog *ui;
    QString investorKey;
};

#endif // VIEWINVESTORKEYDIALOG_H

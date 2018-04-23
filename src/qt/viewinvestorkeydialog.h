#ifndef VIEWINVESTORKEYDIALOG_H
#define VIEWINVESTORKEYDIALOG_H

#include <QDialog>

class WalletModel;

namespace Ui {
class ViewInvestorKeyDialog;
}

class ViewInvestorKeyDialog : public QDialog
{
    Q_OBJECT

public:
    explicit ViewInvestorKeyDialog(QString investorKey, QWidget *parent = 0);
    ~ViewInvestorKeyDialog();

public:
    void setModel(WalletModel *model);

private Q_SLOTS:
    void onCopyInvestorKeyClicked();

private:
    Ui::ViewInvestorKeyDialog *ui;
    WalletModel *model;
};

#endif // VIEWINVESTORKEYDIALOG_H

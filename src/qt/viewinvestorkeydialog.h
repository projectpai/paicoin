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

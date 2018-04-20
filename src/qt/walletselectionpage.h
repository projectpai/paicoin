#ifndef WALLETSELECTIONPAGE_H
#define WALLETSELECTIONPAGE_H

#include <QWidget>

namespace Ui {
class WalletSelectionPage;
}

class WalletSelectionPage : public QWidget
{
    Q_OBJECT

public:
    explicit WalletSelectionPage(QWidget *parent = 0);
    ~WalletSelectionPage();

Q_SIGNALS:
    void goToCreateNewWallet();
    void toToRestoreWallet();

private Q_SLOTS:
    void on_pushButtonCreate_clicked();
    void on_pushButtonRestore_clicked();

private:
    Ui::WalletSelectionPage *ui;
};

#endif // WALLETSELECTIONPAGE_H

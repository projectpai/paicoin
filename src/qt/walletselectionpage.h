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
    void goToRestoreWallet();

private Q_SLOTS:
    void PushButtonCreateClicked();
    void PushButtonRestoreClicked();

private:
    Ui::WalletSelectionPage *ui;
};

#endif // WALLETSELECTIONPAGE_H

#ifndef RESTOREWALLETPAGE_H
#define RESTOREWALLETPAGE_H

#include "wallet/bip39mnemonic.h"

#include <QWidget>

namespace Ui {
class RestoreWalletPage;
}

class RestoreWalletPage : public QWidget
{
    Q_OBJECT

public:
    explicit RestoreWalletPage(QWidget *parent = 0);
    ~RestoreWalletPage();
    void clear();

Q_SIGNALS:
    void backToPreviousPage();
    void restoreWallet(QStringList paperKeys);

protected:
    bool eventFilter(QObject *object, QEvent *event);

private Q_SLOTS:
    void onRestoreWalletClicked();
    void onBackClicked();

private:
    Ui::RestoreWalletPage *ui;
    QStringList paperKeys;
    BIP39Mnemonic b39;
};

#endif // RESTOREWALLETPAGE_H

#ifndef RESTOREWALLETPAGE_H
#define RESTOREWALLETPAGE_H

#include "wallet/bip39mnemonic.h"

#include <QWidget>
#include <QtWidgets/QLineEdit>
#include <set>

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
    void restoreWallet(std::string phrase);

protected:
    bool eventFilter(QObject *object, QEvent *event);

private Q_SLOTS:
    void onRestoreWalletClicked();
    void onBackClicked();

private:
    Ui::RestoreWalletPage *ui;
    BIP39Mnemonic b39;
    std::set<QLineEdit*> validLineSet;
};

#endif // RESTOREWALLETPAGE_H

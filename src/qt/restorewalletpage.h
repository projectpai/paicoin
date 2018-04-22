#ifndef RESTOREWALLETPAGE_H
#define RESTOREWALLETPAGE_H

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

Q_SIGNALS:
    void backToPreviousPage();
    void restoreWallet(QStringList paperKeys);

private Q_SLOTS:
    void onRestoreWalletClicked();
    void onBackClicked();

private:
    Ui::RestoreWalletPage *ui;
    QStringList paperKeys;
};

#endif // RESTOREWALLETPAGE_H

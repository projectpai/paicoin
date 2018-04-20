#ifndef WELCOMEPAGE_H
#define WELCOMEPAGE_H

#include <QWidget>

namespace Ui {
class WelcomePage;
}

class WelcomePage : public QWidget
{
    Q_OBJECT

public:
    explicit WelcomePage(QWidget *parent = 0);
    ~WelcomePage();

Q_SIGNALS:
    void goToWalletSelection();
    void goToIntro();

private Q_SLOTS:
    void on_pushButtonEasy_clicked();
    void on_pushButtonAdvanced_clicked();

private:
    Ui::WelcomePage *ui;
};

#endif // WELCOMEPAGE_H

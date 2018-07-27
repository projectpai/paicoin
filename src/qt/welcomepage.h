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
    explicit WelcomePage(QWidget *parent = nullptr);
    ~WelcomePage();

Q_SIGNALS:
    void goToWalletSelection();
    void goToIntro();

private Q_SLOTS:
    void on_pushButtonEasy_clicked();
    void on_pushButtonAdvanced_clicked();
    void ShowUpdateAvailableDialog();

protected:
    void showEvent(QShowEvent *event) override;

private:
    Ui::WelcomePage *ui;
};

#endif // WELCOMEPAGE_H

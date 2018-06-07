#ifndef SETPINPAGE_H
#define SETPINPAGE_H

#include <QWidget>

namespace Ui {
class SetPinPage;
const int DotWidth = 30;
const int DotHeight = 30;
}

enum class PinPageState : std::int8_t
{
    Init,
    ReEnter,
    RequiredEntry,
    ReadyToVerify
};

class SetPinPage : public QWidget
{
    Q_OBJECT

public:
    explicit SetPinPage(QWidget *parent = 0);
    ~SetPinPage();

public:
    void initSetPinLayout();
    void initReEnterPinLayout();
    void initPinRequiredLayout();

Q_SIGNALS:
    void digitClicked(char digit);
    void backspaceClicked();
    void backToPreviousPage();
    void pinEntered();
    void pinReEntered();
    void pinReadyForConfirmation(const std::string &pin);
    void pinReadyForAuthentication(const std::string &pin);
    void pinValidationFailed();

protected:
    void paintEvent(QPaintEvent *event) override;
    bool eventFilter(QObject* obj, QEvent* event) override;

private Q_SLOTS:
    void onDigitClicked(char digit);
    void onBackspaceClicked();
    void onPinEntered();
    void onPinReEntered();
    void onBackClicked();

private:
    Ui::SetPinPage *ui;
    QPixmap *pixmapBlack;
    QPixmap *pixmapGray;
    QPainter *painterBlack;
    QPainter *painterGray;
    PinPageState pageState;
    QString pin;
    QString pinToVerify;
    bool initialPinEntered;
    int currentNumOfSelectedDots;
};

#endif // SETPINPAGE_H

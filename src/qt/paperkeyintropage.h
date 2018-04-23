#ifndef PAPERKEYINTROPAGE_H
#define PAPERKEYINTROPAGE_H

#include <QWidget>

namespace Ui {
class PaperKeyIntroPage;
}

class PaperKeyIntroPage : public QWidget
{
    Q_OBJECT

public:
    explicit PaperKeyIntroPage(QWidget *parent = 0);
    ~PaperKeyIntroPage();

Q_SIGNALS:
    void writeDownsClicked();
    void backToPreviousPage();

private Q_SLOTS:
    void onWriteDownClicked();
    void onBackClicked();

private:
    Ui::PaperKeyIntroPage *ui;
};

#endif // PAPERKEYINTROPAGE_H

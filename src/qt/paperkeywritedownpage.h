#ifndef PAPERKEYWRITEDOWNPAGE_H
#define PAPERKEYWRITEDOWNPAGE_H

#include <QWidget>

#define PAPER_KEY_WORD_COUNT 12

namespace Ui {
class PaperKeyWritedownPage;
}

class PaperKeyWritedownPage : public QWidget
{
    Q_OBJECT

public:
    explicit PaperKeyWritedownPage(QStringList words, QWidget *parent = 0);
    ~PaperKeyWritedownPage();

Q_SIGNALS:
    void backToPreviousPage();
    void paperKeyWritten();

private Q_SLOTS:
    void onPreviousClicked();
    void onNextClicked();
    void onBackClicked();

private:
    void updateCurrentWordAndCount();

private:
    Ui::PaperKeyWritedownPage *ui;
    QStringList paperKeyList;
    int currentWordCount;
};

#endif // PAPERKEYWRITEDOWNPAGE_H

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
    explicit PaperKeyWritedownPage(QWidget *parent = 0);
    explicit PaperKeyWritedownPage(QStringList words, QWidget *parent = 0);
    explicit PaperKeyWritedownPage(std::string phrase, QWidget *parent = 0);
    ~PaperKeyWritedownPage();

public:
    void setPhrase(const QStringList &phrase);
    void setPhrase(const std::string &phrase);

Q_SIGNALS:
    void backToPreviousPage();
    void paperKeyWritten(const QStringList &phrase);

private Q_SLOTS:
    void onPreviousClicked();
    void onNextClicked();
    void onBackClicked();

private:
    void updateCurrentWordAndCount();
    void connectCustomSignalsAndSlots();

private:
    Ui::PaperKeyWritedownPage *ui;
    QStringList paperKeyList;
    int currentWordCount;
};

#endif // PAPERKEYWRITEDOWNPAGE_H

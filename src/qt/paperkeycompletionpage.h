#ifndef PAPERKEYCOMPLETION_H
#define PAPERKEYCOMPLETION_H

#include <QWidget>
#include <QLineEdit>

#define PAPER_KEY_WORD_COUNT 12

namespace Ui {
class PaperKeyCompletionPage;
const QString LineEditStyleSheetFormat = "background:#fff;background-image: url(:icons/%1);background-repeat: no-repeat;background-position: right;border:1px solid #cbcbcb;";
const QString ButtonEnabledStyleSheet = "background:#1972ea;color:#fff";
const QString ButtonDisabledStyleSheet = "background:#92baf1;color:#fff";
const QString IconNamePass = "pass";
const QString IconNameFail = "fail";
}

class PaperKeyCompletionPage : public QWidget
{
    Q_OBJECT

public:
    explicit PaperKeyCompletionPage(QWidget *parent = 0);
    explicit PaperKeyCompletionPage(QStringList words, QWidget *parent = 0);
    ~PaperKeyCompletionPage();

public:
    void setPaperKeyList(const QStringList &paperKeyList);

Q_SIGNALS:
    void backToPreviousPage();
    void paperKeyProven();

private Q_SLOTS:
    void onSubmitClicked();
    void onBackClicked();
    void textChangedFirst(QString text);
    void textChangedSecond(QString text);

private:
    void SelectRandomKeys();
    void EnableSubmitIfEqual();
    void ConnectSignalsAndSlots();
    void SetLineEditState(QLineEdit* lineEdit, bool match);

private:
    Ui::PaperKeyCompletionPage *ui;
    QStringList paperKeyList;
    QString firstWord;
    QString secondWord;
    int firstIndex;
    int secondIndex;
    bool firstEquality;
    bool secondEquality;
};

#endif // PAPERKEYCOMPLETION_H

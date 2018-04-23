#ifndef HOLDINGPERIODCOMPLETEDIALOG_H
#define HOLDINGPERIODCOMPLETEDIALOG_H

#include <QDialog>

namespace Ui {
class HoldingPeriodCompleteDialog;
const QString UpdateLink = "https://lanier.ai";
}

class HoldingPeriodCompleteDialog : public QDialog
{
    Q_OBJECT

public:
    explicit HoldingPeriodCompleteDialog(bool needsUpdate = false, QWidget *parent = 0);
    ~HoldingPeriodCompleteDialog();

public Q_SLOTS:
    virtual void accept();

private:
    Ui::HoldingPeriodCompleteDialog *ui;
    bool fNeedsUpdate;
};

#endif // HOLDINGPERIODCOMPLETEDIALOG_H

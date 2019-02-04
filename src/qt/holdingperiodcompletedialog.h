#ifndef HOLDINGPERIODCOMPLETEDIALOG_H
#define HOLDINGPERIODCOMPLETEDIALOG_H

#include "paicoindialog.h"

namespace Ui {
class HoldingPeriodCompleteDialog;
const QString UpdateLink = "https://paiup.com";
}

class HoldingPeriodCompleteDialog : public PaicoinDialog
{
    Q_OBJECT

public:
    explicit HoldingPeriodCompleteDialog(bool needsUpdate = false, QWidget *parent = nullptr);
    ~HoldingPeriodCompleteDialog();

public Q_SLOTS:
    virtual void accept();
    virtual void unlock();

Q_SIGNALS:
    void unlockInvestment();

private:
    Ui::HoldingPeriodCompleteDialog *ui;
    bool fNeedsUpdate;
};

#endif // HOLDINGPERIODCOMPLETEDIALOG_H

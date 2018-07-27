#ifndef UPDATEAVAILABLEDIALOG_H
#define UPDATEAVAILABLEDIALOG_H

#include <QDialog>

namespace Ui {
class UpdateAvailableDialog;
}

class UpdateAvailableDialog : public QDialog
{
    Q_OBJECT

public:
    explicit UpdateAvailableDialog(QWidget *parent = nullptr);
    ~UpdateAvailableDialog();

public Q_SLOTS:
    virtual void accept();

private:
    Ui::UpdateAvailableDialog *ui;
};

#endif // UPDATEAVAILABLEDIALOG_H

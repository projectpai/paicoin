#ifndef PAICOINDIALOG_H
#define PAICOINDIALOG_H

#include <QDialog>

class PaicoinDialog : public QDialog
{
    Q_OBJECT

public:
    PaicoinDialog(QWidget *parent);
    virtual ~PaicoinDialog() override = default;

protected:
    virtual bool eventFilter(QObject *object, QEvent *event) override;
};

#endif // PAICOINDIALOG_H

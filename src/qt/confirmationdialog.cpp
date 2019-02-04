#include "confirmationdialog.h"
#include "ui_confirmationdialog.h"

ConfirmationDialog::ConfirmationDialog(const QString &title, QWidget *parent) :
    PaicoinDialog(parent),
    ui(new Ui::ConfirmationDialog)
{
    ui->setupUi(this);
    ui->labelTitle->setText(title);
    adjustSize();

    QPainterPath path;
    path.addRoundedRect(QRectF(rect()), 5, 5);
    QRegion region(path.toFillPolygon().toPolygon());
    setMask(region);
    setWindowFlags(windowFlags() | Qt::FramelessWindowHint);
}

ConfirmationDialog::~ConfirmationDialog()
{
    delete ui;
}

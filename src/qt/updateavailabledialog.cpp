#include "updateavailabledialog.h"
#include "ui_updateavailabledialog.h"
#include "paicoin-config.h"

#include <QDesktopServices>
#include <QUrl>

UpdateAvailableDialog::UpdateAvailableDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::UpdateAvailableDialog)
{
    ui->setupUi(this);

    QPainterPath path;
    path.addRoundedRect(QRectF(rect()), 5, 5);
    QRegion region(path.toFillPolygon().toPolygon());
    setMask(region);
    setWindowFlags(windowFlags() | Qt::FramelessWindowHint);
}

UpdateAvailableDialog::~UpdateAvailableDialog()
{
    delete ui;
}

void UpdateAvailableDialog::accept()
{
    QDesktopServices::openUrl(QUrl(QString(CLIENT_VERSION_DOWNLOAD_URL)));
    QDialog::accept();
}

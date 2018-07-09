#include "holdingperiodcompletedialog.h"
#include "ui_holdingperiodcompletedialog.h"

#include <QDesktopServices>
#include <QUrl>

HoldingPeriodCompleteDialog::HoldingPeriodCompleteDialog(bool needsUpdate, QWidget *parent) :
    QDialog(parent),
    ui(new Ui::HoldingPeriodCompleteDialog),
    fNeedsUpdate(needsUpdate)
{
    ui->setupUi(this);

    if (fNeedsUpdate)
    {
        ui->labelInfo->setText(tr("Please update PAIcoin wallet to unlock your investment.\nYou may update by downloading the latest version from Lanier.ai\n\nIf you do not update the application, your investment will remain in holding."));
        ui->pushButtonOkay->hide();
    }
    else
    {
        ui->labelInfo->setText(tr("Congratulations! Your initial PAIcoin investment is now available for transactions."));
        ui->pushButtonGoTo->hide();
        ui->pushButtonNotNow->hide();
        connect(ui->pushButtonOkay, SIGNAL(clicked()), this, SLOT(unlock()));
    }

    QPainterPath path;
    path.addRoundedRect(QRectF(rect()), 5, 5);
    QRegion region(path.toFillPolygon().toPolygon());
    setMask(region);
    setWindowFlags(windowFlags() | Qt::FramelessWindowHint);
}

HoldingPeriodCompleteDialog::~HoldingPeriodCompleteDialog()
{
    delete ui;
}

void HoldingPeriodCompleteDialog::accept()
{
    if (fNeedsUpdate)
        QDesktopServices::openUrl(QUrl(Ui::UpdateLink));

    QDialog::accept();
}

void HoldingPeriodCompleteDialog::unlock()
{
    Q_EMIT unlockInvestment();
    QDialog::accept();
}

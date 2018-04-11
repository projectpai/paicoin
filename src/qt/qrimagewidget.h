#ifndef QRIMAGEWIDGET_H
#define QRIMAGEWIDGET_H

#include "guiutil.h"

#include <QLabel>
#include <QMouseEvent>
#include <QMenu>

/* Label widget for QR code. This image can be dragged, dropped, copied and saved
 * to disk.
 */
class QRImageWidget : public QLabel
{
#ifdef QT_IDE_BUILD
    Q_OBJECT
#endif

public:
    explicit QRImageWidget(QWidget *parent = 0);
    virtual ~QRImageWidget();
    QImage exportImage();

public Q_SLOTS:
    void saveImage();
    void copyImage();

protected:
    virtual void mousePressEvent(QMouseEvent *event);
    virtual void contextMenuEvent(QContextMenuEvent *event);

private:
    QMenu *contextMenu;
};

#endif // QRIMAGEWIDGET_H

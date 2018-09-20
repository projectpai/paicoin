#ifndef QRIMAGEWIDGET_H
#define QRIMAGEWIDGET_H

#include "guiutil.h"

#include <QLabel>
#include <QMouseEvent>
#include <QMenu>
#include <QObject>

/* Label widget for QR code. This image can be dragged, dropped, copied and saved
 * to disk.
 */
class QRImageWidget : public QLabel
{
    Q_OBJECT

public:
    explicit QRImageWidget(QWidget *parent = nullptr);
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

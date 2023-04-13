#include "softwarewindow.h"

#include <QtGui>

SoftwareWindow::SoftwareWindow(QWindow* parent)
    : OutputWindow(parent), m_backingStore(new QBackingStore(this))
{

}

void SoftwareWindow::resizeEvent(QResizeEvent *resizeEvent)
{
    m_backingStore->resize(resizeEvent->size());
}

// see https://doc.qt.io/qt-6/qtgui-rasterwindow-example.html
void SoftwareWindow::paintEvent(QPaintEvent *event)
{
    if (!isExposed()) { return; }
    
    QRect rect(0, 0, width(), height());
    m_backingStore->beginPaint(rect);

    QPaintDevice *device = m_backingStore->paintDevice();
    QPainter painter(device);

    painter.fillRect(0, 0, width(), height(), QGradient::NightFade);
    
    painter.drawText(QRectF(0, 0, width(), height()), Qt::AlignCenter, QStringLiteral("Software Window"));

    painter.end();

    m_backingStore->endPaint();
    m_backingStore->flush(rect);
}

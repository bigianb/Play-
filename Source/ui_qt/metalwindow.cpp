#include "metalwindow.h"

MetalWindow::MetalWindow(QWindow* parent)
    : OutputWindow(parent)
{
	setSurfaceType(QWindow::MetalSurface);
}

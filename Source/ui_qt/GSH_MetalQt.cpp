#include "GSH_MetalQt.h"
#include <QWindow>


CGSH_MetalQt::CGSH_MetalQt(QWindow* renderWindow)
    : m_renderWindow(renderWindow)
{
}

CGSH_MetalQt::FactoryFunction CGSH_MetalQt::GetFactoryFunction(QWindow* renderWindow)
{
	return [renderWindow]() { return new CGSH_MetalQt(renderWindow); };
}

void CGSH_MetalQt::InitializeImpl()
{
    setWinId(m_renderWindow->winId());
	CGSH_Metal::InitializeImpl();
}

void CGSH_MetalQt::ReleaseImpl()
{
	CGSH_Metal::ReleaseImpl();
}

void CGSH_MetalQt::PresentBackbuffer()
{
}

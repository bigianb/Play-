#include <QSurface>
#include <QWindow>

#include "GSH_SoftwareQt.h"


CGSH_SoftwareQt::CGSH_SoftwareQt(QSurface* renderSurface)
    : m_renderSurface(renderSurface)
{
}

CGSH_SoftwareQt::FactoryFunction CGSH_SoftwareQt::GetFactoryFunction(QSurface* renderSurface)
{
	return [renderSurface]() { return new CGSH_SoftwareQt(renderSurface); };
}

void CGSH_SoftwareQt::InitializeImpl()
{

}

void CGSH_SoftwareQt::ReleaseImpl()
{

}


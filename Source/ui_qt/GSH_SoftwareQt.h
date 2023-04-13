#pragma once

#include "gs/GSH_Null.h"

class QSurface;


class CGSH_SoftwareQt : public CGSH_Null
{
public:
    CGSH_SoftwareQt(QSurface*);
	virtual ~CGSH_SoftwareQt() = default;

	static FactoryFunction GetFactoryFunction(QSurface*);

	void InitializeImpl() override;
	void ReleaseImpl() override;

private:
	QSurface* m_renderSurface = nullptr;

};

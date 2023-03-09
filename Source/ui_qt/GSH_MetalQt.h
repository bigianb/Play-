#pragma once

#include "gs/GSH_Metal/GSH_Metal.h"

class QWindow;

class CGSH_MetalQt : public CGSH_Metal
{
public:
	CGSH_MetalQt(QWindow*);
	virtual ~CGSH_MetalQt() = default;

	static FactoryFunction GetFactoryFunction(QWindow*);

	void InitializeImpl() override;
	void ReleaseImpl() override;
	void PresentBackbuffer() override;

private:
	QWindow* m_renderWindow = nullptr;
};

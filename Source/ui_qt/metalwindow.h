#pragma once

#include "outputwindow.h"

class MetalWindow : public OutputWindow
{
	Q_OBJECT
public:
	explicit MetalWindow(QWindow* parent = 0);
	~MetalWindow() = default;
};

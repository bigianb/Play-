#pragma once

#include "outputwindow.h"
#include <QBackingStore>
#include <QScopedPointer>

class SoftwareWindow : public OutputWindow
{
	Q_OBJECT
public:
	explicit SoftwareWindow(QWindow* parent = 0);
	~SoftwareWindow() = default;
    
protected:
    void resizeEvent(QResizeEvent *event) override;
    void paintEvent(QPaintEvent *event) override;
    
    QScopedPointer<QBackingStore> m_backingStore;
};

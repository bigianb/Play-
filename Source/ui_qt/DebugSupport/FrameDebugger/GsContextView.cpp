#include "GsContextView.h"
#include "GsStateUtils.h"
#include "gs/GsDebuggerInterface.h"
#include "gs/GsPixelFormats.h"
#include "../../../AppConfig.h"

#include <QVBoxLayout>

enum TAB_IDS
{
	TAB_ID_FRAMEBUFFER,
	TAB_ID_TEXTURE_BASE,
	TAB_ID_TEXTURE_MIP1,
	TAB_ID_TEXTURE_MIP2,
	TAB_ID_TEXTURE_MIP3,
	TAB_ID_TEXTURE_MIP4,
	TAB_ID_TEXTURE_MIP5,
	TAB_ID_TEXTURE_MIP6
};

CGsContextView::CGsContextView(QWidget* parent, QComboBox* contextBuffer, QPushButton* fitButton, QPushButton* saveButton, CGSHandler* gs, unsigned int contextId)
    : QWidget(parent)
    , m_contextId(contextId)
    , m_gs(gs)
{

	m_bufferView = std::make_unique<CPixelBufferView>(this, contextBuffer);
	auto layout = new QVBoxLayout;
	setLayout(layout);
	layout->addWidget(m_bufferView.get());

	connect(fitButton, &QPushButton::clicked, [&]() {
		m_bufferView->FitBitmap();
	});
	connect(saveButton, &QPushButton::clicked, [&]() {
		m_bufferView->OnSaveBitmap();
	});
}

void CGsContextView::SetFbDisplayMode(FB_DISPLAY_MODE fbDisplayMode)
{
	m_fbDisplayMode = fbDisplayMode;
	UpdateBufferView();
}

void CGsContextView::UpdateState(CGSHandler* gs, CGsPacketMetadata*, DRAWINGKICK_INFO* drawingKick)
{
	assert(gs == m_gs);
	m_drawingKick = (*drawingKick);
	UpdateBufferView();
}

void CGsContextView::SetSelection(int selected)
{
	m_selected = selected;
	UpdateBufferView();
}

void CGsContextView::UpdateBufferView()
{
	uint32 selectedId = m_selected;
	switch(selectedId)
	{
	case TAB_ID_FRAMEBUFFER:
		UpdateFramebufferView();
		break;
	case TAB_ID_TEXTURE_BASE:
	case TAB_ID_TEXTURE_MIP1:
	case TAB_ID_TEXTURE_MIP2:
	case TAB_ID_TEXTURE_MIP3:
	case TAB_ID_TEXTURE_MIP4:
	case TAB_ID_TEXTURE_MIP5:
	case TAB_ID_TEXTURE_MIP6:
	{
		uint64 tex0Reg = m_gs->GetRegisters()[GS_REG_TEX0_1 + m_contextId];
		uint64 tex1Reg = m_gs->GetRegisters()[GS_REG_TEX1_1 + m_contextId];
		uint64 miptbp1Reg = m_gs->GetRegisters()[GS_REG_MIPTBP1_1 + m_contextId];
		uint64 miptbp2Reg = m_gs->GetRegisters()[GS_REG_MIPTBP2_1 + m_contextId];
		auto tex0 = make_convertible<CGSHandler::TEX0>(tex0Reg);
		auto tex1 = make_convertible<CGSHandler::TEX1>(tex1Reg);

		uint32 mipLevel = selectedId - TAB_ID_TEXTURE_BASE;
		Framework::CBitmap texture, clutTexture, alphaTexture;

		if(mipLevel <= tex1.nMaxMip)
		{
			if(auto debuggerInterface = dynamic_cast<CGsDebuggerInterface*>(m_gs))
			{
				texture = debuggerInterface->GetTexture(tex0Reg, tex1.nMaxMip, miptbp1Reg, miptbp2Reg, mipLevel);
			}
		}

		if(!texture.IsEmpty() && CGsPixelFormats::IsPsmIDTEX(tex0.nPsm))
		{
			ColorArray convertedClut;
			m_gs->MakeLinearCLUT(tex0, convertedClut);
			clutTexture = LookupBitmap(texture, convertedClut);

			if(tex0.nCPSM == CGSHandler::PSMCT32)
			{
				alphaTexture = ExtractAlpha32(clutTexture);
			}
		}

		if(!texture.IsEmpty() && CGsPixelFormats::IsPsmIDTEX4(tex0.nPsm))
		{
			//Too hard to see if pixel brightness is not boosted
			BrightenBitmap(texture);
		}

		if(!texture.IsEmpty() && tex0.nPsm == CGSHandler::PSMCT32)
		{
			alphaTexture = ExtractAlpha32(texture);
		}

		CPixelBufferView::PixelBufferArray pixelBuffers;
		pixelBuffers.emplace_back("Raw", std::move(texture));
		if(!clutTexture.IsEmpty())
		{
			pixelBuffers.emplace_back("+ CLUT", std::move(clutTexture));
		}
		if(!alphaTexture.IsEmpty())
		{
			pixelBuffers.emplace_back("Alpha", std::move(alphaTexture));
		}
		m_bufferView->SetPixelBuffers(std::move(pixelBuffers));
	}
	break;
	}
	m_bufferView->repaint();
}

void CGsContextView::UpdateFramebufferView()
{
	uint64 frameReg = m_gs->GetRegisters()[GS_REG_FRAME_1 + m_contextId];
	auto frame = make_convertible<CGSHandler::FRAME>(frameReg);

	Framework::CBitmap framebuffer;
	if(auto debuggerInterface = dynamic_cast<CGsDebuggerInterface*>(m_gs))
	{
		framebuffer = debuggerInterface->GetFramebuffer(frame);
	}
	if(framebuffer.IsEmpty())
	{
		m_bufferView->SetPixelBuffers(CPixelBufferView::PixelBufferArray());
		return;
	}

	auto scale = CAppConfig::GetInstance().GetPreferenceInteger(PREF_CGSH_OPENGL_RESOLUTION_FACTOR);
	//Clip framebuffer
	if(m_fbDisplayMode == FB_DISPLAY_MODE_448P)
	{
		framebuffer = framebuffer.ResizeCanvas(640 * scale, 448 * scale);
	}
	else if(m_fbDisplayMode == FB_DISPLAY_MODE_448I)
	{
		framebuffer = framebuffer.ResizeCanvas(640 * scale, 224 * scale);
	}

	Framework::CBitmap alphaFramebuffer;
	if(frame.nPsm == CGSHandler::PSMCT32)
	{
		assert(framebuffer.GetBitsPerPixel() == 32);
		alphaFramebuffer = ExtractAlpha32(framebuffer);
	}

	auto postProcessFramebuffer =
	    [this, scale](Framework::CBitmap src) {
		    if(!src.IsEmpty())
		    {
			    RenderDrawKick(src);
			    if(m_fbDisplayMode == FB_DISPLAY_MODE_448I)
			    {
				    src = src.Resize(640 * scale, 448 * scale);
			    }
		    }
		    return src;
	    };

	framebuffer = postProcessFramebuffer(std::move(framebuffer));
	alphaFramebuffer = postProcessFramebuffer(std::move(alphaFramebuffer));

	CPixelBufferView::PixelBufferArray pixelBuffers;
	pixelBuffers.emplace_back("Raw", std::move(framebuffer));
	if(!alphaFramebuffer.IsEmpty())
	{
		pixelBuffers.emplace_back("Alpha", std::move(alphaFramebuffer));
	}
	m_bufferView->SetPixelBuffers(std::move(pixelBuffers));
}

void CGsContextView::RenderDrawKick(Framework::CBitmap& bitmap)
{
	if(m_drawingKick.primType == CGSHandler::PRIM_INVALID) return;
	if(m_drawingKick.context != m_contextId) return;

	auto primHighlightColor = Framework::CColor(0, 0xFF, 0, 0xFF);
	int scale = CAppConfig::GetInstance().GetPreferenceInteger(PREF_CGSH_OPENGL_RESOLUTION_FACTOR);
	switch(m_drawingKick.primType)
	{
	case CGSHandler::PRIM_TRIANGLE:
	case CGSHandler::PRIM_TRIANGLESTRIP:
	case CGSHandler::PRIM_TRIANGLEFAN:
	{
		int x1 = (static_cast<int16>(m_drawingKick.vertex[0].x) / 16) * scale;
		int y1 = (static_cast<int16>(m_drawingKick.vertex[0].y) / 16) * scale;
		int x2 = (static_cast<int16>(m_drawingKick.vertex[1].x) / 16) * scale;
		int y2 = (static_cast<int16>(m_drawingKick.vertex[1].y) / 16) * scale;
		int x3 = (static_cast<int16>(m_drawingKick.vertex[2].x) / 16) * scale;
		int y3 = (static_cast<int16>(m_drawingKick.vertex[2].y) / 16) * scale;
		bitmap.DrawLine(x1, y1, x2, y2, primHighlightColor);
		bitmap.DrawLine(x1, y1, x3, y3, primHighlightColor);
		bitmap.DrawLine(x2, y2, x3, y3, primHighlightColor);
	}
	break;
	case CGSHandler::PRIM_SPRITE:
	{
		int x1 = (static_cast<int16>(m_drawingKick.vertex[0].x) / 16) * scale;
		int y1 = (static_cast<int16>(m_drawingKick.vertex[0].y) / 16) * scale;
		int x2 = (static_cast<int16>(m_drawingKick.vertex[1].x) / 16) * scale;
		int y2 = (static_cast<int16>(m_drawingKick.vertex[1].y) / 16) * scale;
		bitmap.DrawLine(x1, y1, x1, y2, primHighlightColor);
		bitmap.DrawLine(x1, y2, x2, y2, primHighlightColor);
		bitmap.DrawLine(x2, y2, x2, y1, primHighlightColor);
		bitmap.DrawLine(x2, y1, x1, y1, primHighlightColor);
	}
	break;
	}
}

void CGsContextView::BrightenBitmap(Framework::CBitmap& bitmap)
{
	assert(!bitmap.IsEmpty());
	assert(bitmap.GetBitsPerPixel() == 8);
	auto pixels = reinterpret_cast<uint8*>(bitmap.GetPixels());
	for(uint32 y = 0; y < bitmap.GetHeight(); y++)
	{
		for(uint32 x = 0; x < bitmap.GetWidth(); x++)
		{
			pixels[x] <<= 4;
		}
		pixels += bitmap.GetPitch();
	}
}

uint32 CGsContextView::Color_Ps2ToRGBA(uint32 color)
{
	uint8 r = (color & 0xFF);
	uint8 b = (color & 0xFF00) >> 8;
	uint8 g = (color & 0xFF0000) >> 16;
	uint8 a = (color & 0xFF000000) >> 24;
	return g << 0 | b << 8 | r << 16 | a << 24;
}

Framework::CBitmap CGsContextView::LookupBitmap(const Framework::CBitmap& srcBitmap, const ColorArray& clut)
{
	assert(!srcBitmap.IsEmpty());
	assert(srcBitmap.GetBitsPerPixel() == 8);
	auto dstBitmap = Framework::CBitmap(srcBitmap.GetWidth(), srcBitmap.GetHeight(), 32);
	auto srcPixels = reinterpret_cast<uint8*>(srcBitmap.GetPixels());
	auto dstPixels = reinterpret_cast<uint32*>(dstBitmap.GetPixels());
	for(uint32 y = 0; y < srcBitmap.GetHeight(); y++)
	{
		for(uint32 x = 0; x < srcBitmap.GetWidth(); x++)
		{
			uint8 index = srcPixels[x];
			uint32 color = clut[index];
			uint32 newColor = Color_Ps2ToRGBA(color);
			dstPixels[x] = newColor;
		}
		srcPixels += srcBitmap.GetPitch();
		dstPixels += dstBitmap.GetPitch() / 4;
	}
	return std::move(dstBitmap);
}

Framework::CBitmap CGsContextView::ExtractAlpha32(const Framework::CBitmap& srcBitmap)
{
	assert(!srcBitmap.IsEmpty());
	assert(srcBitmap.GetBitsPerPixel() == 32);
	auto dstBitmap = Framework::CBitmap(srcBitmap.GetWidth(), srcBitmap.GetHeight(), 32);
	auto srcPixels = reinterpret_cast<uint32*>(srcBitmap.GetPixels());
	auto dstPixels = reinterpret_cast<uint32*>(dstBitmap.GetPixels());
	for(uint32 y = 0; y < srcBitmap.GetHeight(); y++)
	{
		for(uint32 x = 0; x < srcBitmap.GetWidth(); x++)
		{
			uint32 color = srcPixels[x];
			uint32 alpha = color >> 24;
			dstPixels[x] = (alpha) | (alpha << 8) | (alpha << 16) | 0xFF000000;
		}
		srcPixels += srcBitmap.GetPitch() / 4;
		dstPixels += dstBitmap.GetPitch() / 4;
	}
	return std::move(dstBitmap);
}

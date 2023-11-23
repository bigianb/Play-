#pragma once

#include <vector>
#include "../GSHandler.h"
#include "../GsDebuggerInterface.h"
#include "../GsCachedArea.h"
#include "../GsTextureCache.h"

namespace MTL {
class Device;
class CommandQueue;
class RenderPipelineState;
}

class CGSH_Metal : public CGSHandler, public CGsDebuggerInterface
{
public:
	CGSH_Metal();
	virtual ~CGSH_Metal() = default;

	void SetPresentationParams(const CGSHandler::PRESENTATION_PARAMS&) override;

	void ProcessHostToLocalTransfer() override;
	void ProcessLocalToHostTransfer() override;
	void ProcessLocalToLocalTransfer() override;
	void ProcessClutTransfer(uint32, uint32) override;

	Framework::CBitmap GetScreenshot() override;

	//Debugger Interface
	bool GetDepthTestingEnabled() const override;
	void SetDepthTestingEnabled(bool) override;

	bool GetAlphaBlendingEnabled() const override;
	void SetAlphaBlendingEnabled(bool) override;
	bool GetAlphaTestingEnabled() const override;
	void SetAlphaTestingEnabled(bool) override;

	Framework::CBitmap GetFramebuffer(uint64) override;
	Framework::CBitmap GetDepthbuffer(uint64, uint64) override;
	Framework::CBitmap GetTexture(uint64, uint32, uint64, uint64, uint32) override;
	int GetFramebufferScale() override;

	const VERTEX* GetInputVertices() const override;

protected:
    void setWinId(unsigned long long wId) {m_wId = wId;}
    
	void WriteRegisterImpl(uint8, uint64) override;
	void InitializeImpl() override;
	void ReleaseImpl() override;
	void ResetImpl() override;
	void MarkNewFrame() override;
    void FlipImpl(const DISPLAY_INFO&) override;
	void BeginTransferWrite() override;
	void TransferWrite(const uint8*, uint32) override;
	void WriteBackMemoryCache() override;
	void SyncMemoryCache() override;
	void SyncCLUT(const TEX0&) override;

private:
	struct CLUTKEY : public convertible<uint64>
	{
		uint32 idx4 : 1;
		uint32 cbp : 14;
		uint32 cpsm : 4;
		uint32 csm : 1;
		uint32 csa : 5;
		uint32 cbw : 6;
		uint32 cou : 6;
		uint32 cov : 10;
		uint32 reserved : 16;
	};
	static_assert(sizeof(CLUTKEY) == sizeof(uint64), "CLUTKEY too big for an uint64");

	enum
	{
		CLUT_CACHE_SIZE = 32,
	};

	virtual void PresentBackbuffer() = 0;

	void CreateDescriptorPool();
	void CreateMemoryBuffer();
	void CreateClutBuffer();

	void VertexKick(uint8, uint64);
	void SetRenderingContext(uint64);

    void FlushVertexBuffer();
    void DoRenderPass();
    
	void Prim_Point();
	void Prim_Line();
	void Prim_Triangle();
	void Prim_Sprite();

    void buildShaders();

	Framework::CBitmap GetFramebufferImpl(uint64);
	Framework::CBitmap GetDepthbufferImpl(uint64, uint64);
	Framework::CBitmap GetTextureImpl(uint64, uint32, uint64, uint64, uint32);

	template <typename PixelIndexor, uint32 mask = ~0U>
	static Framework::CBitmap ReadImage32(uint8* ram, uint32 bufferPtr, uint32 bufferWidth, uint32 width, uint32 height)
	{
		auto bitmap = Framework::CBitmap(width, height, 32);
		auto bitmapPixels = reinterpret_cast<uint32*>(bitmap.GetPixels());
		PixelIndexor indexor(ram, bufferPtr, bufferWidth);
		for(unsigned int y = 0; y < height; y++)
		{
			for(unsigned int x = 0; x < width; x++)
			{
				uint32 pixel = indexor.GetPixel(x, y) & mask;
				uint32 r = (pixel & 0x000000FF) >> 0;
				uint32 g = (pixel & 0x0000FF00) >> 8;
				uint32 b = (pixel & 0x00FF0000) >> 16;
				uint32 a = (pixel & 0xFF000000) >> 24;
				(*bitmapPixels) = b | (g << 8) | (r << 16) | (a << 24);
				bitmapPixels++;
			}
		}
		return bitmap;
	}

	template <typename PixelIndexor>
	static Framework::CBitmap ReadImage16(uint8* ram, uint32 bufferPtr, uint32 bufferWidth, uint32 width, uint32 height)
	{
		auto bitmap = Framework::CBitmap(width, height, 32);
		auto bitmapPixels = reinterpret_cast<uint32*>(bitmap.GetPixels());
		PixelIndexor indexor(ram, bufferPtr, bufferWidth);
		for(unsigned int y = 0; y < height; y++)
		{
			for(unsigned int x = 0; x < width; x++)
			{
				uint16 pixel = indexor.GetPixel(x, y);
				uint32 r = ((pixel & 0x001F) >> 0) << 3;
				uint32 g = ((pixel & 0x03E0) >> 5) << 3;
				uint32 b = ((pixel & 0x7C00) >> 10) << 3;
				uint32 a = (((pixel & 0x8000) >> 15) != 0) ? 0xFF : 0;
				(*bitmapPixels) = b | (g << 8) | (r << 16) | (a << 24);
				bitmapPixels++;
			}
		}
		return bitmap;
	}

	template <typename PixelIndexor>
	static Framework::CBitmap ReadImage8(uint8* ram, uint32 bufferPtr, uint32 bufferWidth, uint32 width, uint32 height)
	{
		auto bitmap = Framework::CBitmap(width, height, 8);
		auto bitmapPixels = reinterpret_cast<uint8*>(bitmap.GetPixels());
		PixelIndexor indexor(ram, bufferPtr, bufferWidth);
		for(unsigned int y = 0; y < height; y++)
		{
			for(unsigned int x = 0; x < width; x++)
			{
				uint8 pixel = indexor.GetPixel(x, y);
				(*bitmapPixels) = pixel;
				bitmapPixels++;
			}
		}
		return bitmap;
	}

    // Window ID (a NSView*)
    unsigned long long m_wId;
        
    MTL::Device* m_device = nullptr;
    MTL::CommandQueue* m_commandQueue = nullptr;
    MTL::RenderPipelineState* m_pso = nullptr;
    
	//Draw context
	VERTEX m_vtxBuffer[3];
	uint32 m_vtxCount = 0;
	uint32 m_primitiveType = 0;
	PRMODE m_primitiveMode;
	uint32 m_fbBasePtr = 0;
	float m_primOfsX = 0;
	float m_primOfsY = 0;
	uint32 m_texWidth = 0;
	uint32 m_texHeight = 0;
	CLUTKEY m_clutStates[CLUT_CACHE_SIZE];
	uint32 m_nextClutCacheIndex = 0;

    // Accumulates the image transfer data until it is complete.
    std::vector<uint8> m_xferBuffer;
    
    struct PRIM_VERTEX
    {
        float x, y;
        uint32 z;
        uint32 color;
        float s, t, q;
        float f;
    };

    enum VERTEX_BUFFER_SIZE
    {
        VERTEX_BUFFER_SIZE = 0x1000,
    };

    typedef std::vector<PRIM_VERTEX> VertexBuffer;
    VertexBuffer m_vertexBuffer;

	bool m_depthTestingEnabled = true;
	bool m_alphaBlendingEnabled = true;
	bool m_alphaTestingEnabled = true;
};

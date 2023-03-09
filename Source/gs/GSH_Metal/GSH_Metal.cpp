#define NS_PRIVATE_IMPLEMENTATION
#define CA_PRIVATE_IMPLEMENTATION
#define MTL_PRIVATE_IMPLEMENTATION
#include <Foundation/Foundation.hpp>
#include <Metal/Metal.hpp>
#include <QuartzCore/QuartzCore.hpp>
#include <simd/simd.h>

#include <cstring>
#include "../GsPixelFormats.h"
#include "../../Log.h"
#include "../../AppConfig.h"
#include "GSH_Metal.h"

#define LOG_NAME ("gsh_metal")

#include "metalutil.h"

CGSH_Metal::CGSH_Metal()
{
    m_vertexBuffer.reserve(VERTEX_BUFFER_SIZE);
}


void CGSH_Metal::InitializeImpl()
{
    // Create means we own it and need to release it.
    m_device = MTL::CreateSystemDefaultDevice();
    
    // FIXME: this needs to be done on the main thread.
    assign_device(m_wId, m_device);
    
    m_commandQueue = m_device->newCommandQueue();
    
    buildShaders();
    
    m_vtxCount = 0;
}

void CGSH_Metal::buildShaders()
{
    using NS::StringEncoding::UTF8StringEncoding;

    const char* shaderSrc = R"(
        #include <metal_stdlib>
        using namespace metal;

        struct v2f
        {
            float4 position [[position]];
            half3 color;
        };

        v2f vertex vertexMain( uint vertexId [[vertex_id]],
                               device const float3* positions [[buffer(0)]],
                               device const float3* colors [[buffer(1)]] )
        {
            v2f o;
            o.position = float4( positions[ vertexId ], 1.0 );
            o.color = half3 ( colors[ vertexId ] );
            return o;
        }

        half4 fragment fragmentMain( v2f in [[stage_in]] )
        {
            return half4( in.color, 1.0 );
        }
    )";

    NS::Error* pError = nullptr;
    MTL::Library* pLibrary = m_device->newLibrary( NS::String::string(shaderSrc, UTF8StringEncoding), nullptr, &pError );
    if ( !pLibrary )
    {
        __builtin_printf( "%s", pError->localizedDescription()->utf8String() );
        assert( false );
    }

    MTL::Function* pVertexFn = pLibrary->newFunction( NS::String::string("vertexMain", UTF8StringEncoding) );
    MTL::Function* pFragFn = pLibrary->newFunction( NS::String::string("fragmentMain", UTF8StringEncoding) );

    MTL::RenderPipelineDescriptor* pDesc = MTL::RenderPipelineDescriptor::alloc()->init();
    pDesc->setVertexFunction( pVertexFn );
    pDesc->setFragmentFunction( pFragFn );
    pDesc->colorAttachments()->object(0)->setPixelFormat( MTL::PixelFormat::PixelFormatBGRA8Unorm );

    m_pso = m_device->newRenderPipelineState( pDesc, &pError );
    if ( !m_pso )
    {
        __builtin_printf( "%s", pError->localizedDescription()->utf8String() );
        assert( false );
    }

    pVertexFn->release();
    pFragFn->release();
    pDesc->release();
    pLibrary->release();
}

void CGSH_Metal::ReleaseImpl()
{
    if (m_commandQueue){
        m_commandQueue->release();
        m_commandQueue = nullptr;
    }
    if (m_device){
        m_device->release();
        m_device = nullptr;
    }
    if (m_pso){
        m_pso->release();
    }
}

void CGSH_Metal::ResetImpl()
{

}

void CGSH_Metal::SetPresentationParams(const CGSHandler::PRESENTATION_PARAMS& presentationParams)
{
	CGSHandler::SetPresentationParams(presentationParams);
}

void CGSH_Metal::MarkNewFrame()
{
	CGSHandler::MarkNewFrame();
}

void CGSH_Metal::FlipImpl()
{
    FlushVertexBuffer();
    
	CGSHandler::FlipImpl();
}

void CGSH_Metal::FlushVertexBuffer()
{
    if(m_vertexBuffer.empty()) return;

    DoRenderPass();
    m_vertexBuffer.clear();
}

void CGSH_Metal::DoRenderPass()
{
    const auto numVertices = m_vertexBuffer.size();
    
    const size_t positionsDataSize = numVertices * sizeof( simd::float3 );
    const size_t colorDataSize = numVertices * sizeof( simd::float3 );

    MTL::Buffer* pVertexPositionsBuffer = m_device->newBuffer( positionsDataSize, MTL::ResourceStorageModeManaged );
    MTL::Buffer* pVertexColorsBuffer = m_device->newBuffer( colorDataSize, MTL::ResourceStorageModeManaged );


    simd::float3* positions = (simd::float3*)pVertexPositionsBuffer->contents();
    simd::float3* colours = (simd::float3*)pVertexColorsBuffer->contents();
    for (int vtxNo = 0; vtxNo < numVertices; ++vtxNo){
        const auto vertex = m_vertexBuffer[vtxNo];
        positions[vtxNo] = {(float)vertex.x/2048.0f, (float)vertex.y/2048.0f, (float)vertex.z/2048.0f};
        colours[vtxNo] = {1.0f, 1.0f, 0.0f};
    }

    pVertexPositionsBuffer->didModifyRange( NS::Range::Make( 0, pVertexPositionsBuffer->length() ) );
    pVertexColorsBuffer->didModifyRange( NS::Range::Make( 0, pVertexColorsBuffer->length() ) );
    
    NS::AutoreleasePool* pPool = NS::AutoreleasePool::alloc()->init();

    auto drawable = next_drawable(m_wId);
    
    auto dsize = drawable->layer()->drawableSize();
    
    MTL::CommandBuffer* pCmd = m_commandQueue->commandBuffer();

    MTL::RenderPassDescriptor* pRpd = MTL::RenderPassDescriptor::alloc()->init();
    
    auto attachment = pRpd->colorAttachments()->object(0);
    attachment->setClearColor(MTL::ClearColor(0.2, 0.0, 0.0, 1.0));
    attachment->setLoadAction(MTL::LoadActionClear);
    attachment->setTexture(drawable->texture());
    
    MTL::RenderCommandEncoder* pEnc = pCmd->renderCommandEncoder( pRpd );
    pEnc->setViewport(MTL::Viewport{0.0, 0.0, dsize.width, dsize.height, -1.0, 1.0});
    pEnc->setRenderPipelineState( m_pso );
    pEnc->setVertexBuffer( pVertexPositionsBuffer, 0, 0 );
    pEnc->setVertexBuffer( pVertexColorsBuffer, 0, 1 );
    pEnc->drawPrimitives( MTL::PrimitiveType::PrimitiveTypeTriangle, NS::UInteger(0), NS::UInteger(numVertices) );

    pEnc->endEncoding();
    pCmd->presentDrawable( drawable );
    pCmd->commit();

    pPool->release();
    
    pVertexPositionsBuffer->release();
    pVertexColorsBuffer->release();
}

void CGSH_Metal::VertexKick(uint8 registerId, uint64 data)
{
    if(m_vtxCount == 0) return;

    bool drawingKick = (registerId == GS_REG_XYZ2) || (registerId == GS_REG_XYZF2);
    bool fog = (registerId == GS_REG_XYZF2) || (registerId == GS_REG_XYZF3);

    if(!m_drawEnabled) drawingKick = false;

    if(fog)
    {
        m_vtxBuffer[m_vtxCount - 1].position = data & 0x00FFFFFFFFFFFFFFULL;
        m_vtxBuffer[m_vtxCount - 1].rgbaq = m_nReg[GS_REG_RGBAQ];
        m_vtxBuffer[m_vtxCount - 1].uv = m_nReg[GS_REG_UV];
        m_vtxBuffer[m_vtxCount - 1].st = m_nReg[GS_REG_ST];
        m_vtxBuffer[m_vtxCount - 1].fog = static_cast<uint8>(data >> 56);
    }
    else
    {
        m_vtxBuffer[m_vtxCount - 1].position = data;
        m_vtxBuffer[m_vtxCount - 1].rgbaq = m_nReg[GS_REG_RGBAQ];
        m_vtxBuffer[m_vtxCount - 1].uv = m_nReg[GS_REG_UV];
        m_vtxBuffer[m_vtxCount - 1].st = m_nReg[GS_REG_ST];
        m_vtxBuffer[m_vtxCount - 1].fog = static_cast<uint8>(m_nReg[GS_REG_FOG] >> 56);
    }

    m_vtxCount--;

    if(m_vtxCount == 0)
    {
        if((m_nReg[GS_REG_PRMODECONT] & 1) != 0)
        {
            m_primitiveMode <<= m_nReg[GS_REG_PRIM];
        }
        else
        {
            m_primitiveMode <<= m_nReg[GS_REG_PRMODE];
        }

        if(drawingKick)
        {
            SetRenderingContext(m_primitiveMode);
        }

        switch(m_primitiveType)
        {
        case PRIM_POINT:
            if(drawingKick) Prim_Point();
            m_vtxCount = 1;
            break;
        case PRIM_LINE:
            if(drawingKick) Prim_Line();
            m_vtxCount = 2;
            break;
        case PRIM_LINESTRIP:
            if(drawingKick) Prim_Line();
            memcpy(&m_vtxBuffer[1], &m_vtxBuffer[0], sizeof(VERTEX));
            m_vtxCount = 1;
            break;
        case PRIM_TRIANGLE:
            if(drawingKick) Prim_Triangle();
            m_vtxCount = 3;
            break;
        case PRIM_TRIANGLESTRIP:
            if(drawingKick) Prim_Triangle();
            memcpy(&m_vtxBuffer[2], &m_vtxBuffer[1], sizeof(VERTEX));
            memcpy(&m_vtxBuffer[1], &m_vtxBuffer[0], sizeof(VERTEX));
            m_vtxCount = 1;
            break;
        case PRIM_TRIANGLEFAN:
            if(drawingKick) Prim_Triangle();
            memcpy(&m_vtxBuffer[1], &m_vtxBuffer[0], sizeof(VERTEX));
            m_vtxCount = 1;
            break;
        case PRIM_SPRITE:
            if(drawingKick) Prim_Sprite();
            m_vtxCount = 2;
            break;
        }
    }
}

void CGSH_Metal::SetRenderingContext(uint64 primReg)
{
    auto prim = make_convertible<PRMODE>(primReg);

    unsigned int context = prim.nContext;

    auto offset = make_convertible<XYOFFSET>(m_nReg[GS_REG_XYOFFSET_1 + context]);
    auto frame = make_convertible<FRAME>(m_nReg[GS_REG_FRAME_1 + context]);
    auto zbuf = make_convertible<ZBUF>(m_nReg[GS_REG_ZBUF_1 + context]);
    auto tex0 = make_convertible<TEX0>(m_nReg[GS_REG_TEX0_1 + context]);
    auto tex1 = make_convertible<TEX1>(m_nReg[GS_REG_TEX1_1 + context]);
    auto clamp = make_convertible<CLAMP>(m_nReg[GS_REG_CLAMP_1 + context]);
    auto alpha = make_convertible<ALPHA>(m_nReg[GS_REG_ALPHA_1 + context]);
    auto scissor = make_convertible<SCISSOR>(m_nReg[GS_REG_SCISSOR_1 + context]);
    auto test = make_convertible<TEST>(m_nReg[GS_REG_TEST_1 + context]);
    auto texA = make_convertible<TEXA>(m_nReg[GS_REG_TEXA]);
    auto fogCol = make_convertible<FOGCOL>(m_nReg[GS_REG_FOGCOL]);
    auto scanMask = m_nReg[GS_REG_SCANMSK] & 3;
    auto colClamp = m_nReg[GS_REG_COLCLAMP] & 1;
    auto fba = m_nReg[GS_REG_FBA_1 + context] & 1;
    
    m_fbBasePtr = frame.GetBasePtr();

    m_primOfsX = offset.GetX();
    m_primOfsY = offset.GetY();

    m_texWidth = tex0.GetWidth();
    m_texHeight = tex0.GetHeight();
}

void CGSH_Metal::Prim_Point()
{
}

void CGSH_Metal::Prim_Line()
{
}

void CGSH_Metal::Prim_Triangle()
{
}

static uint32 MakeColor(uint8 r, uint8 g, uint8 b, uint8 a)
{
    return (a << 24) | (b << 16) | (g << 8) | (r);
}

void CGSH_Metal::Prim_Sprite()
{
    XYZ xyz[2];
    xyz[0] <<= m_vtxBuffer[1].position;
    xyz[1] <<= m_vtxBuffer[0].position;

    float nX1 = xyz[0].GetX();
    float nY1 = xyz[0].GetY();
    float nX2 = xyz[1].GetX();
    float nY2 = xyz[1].GetY();
    uint32 nZ = xyz[1].nZ;

    RGBAQ rgbaq[2];
    rgbaq[0] <<= m_vtxBuffer[1].rgbaq;
    rgbaq[1] <<= m_vtxBuffer[0].rgbaq;

    nX1 -= m_primOfsX;
    nX2 -= m_primOfsX;

    nY1 -= m_primOfsY;
    nY2 -= m_primOfsY;

    float nS[2] = {0, 0};
    float nT[2] = {0, 0};
/*
    if(m_PrimitiveMode.nTexture)
    {
        if(m_PrimitiveMode.nUseUV)
        {
            UV uv[2];
            uv[0] <<= m_vtxBuffer[1].uv;
            uv[1] <<= m_vtxBuffer[0].uv;

            nS[0] = uv[0].GetU() / static_cast<float>(m_nTexWidth);
            nS[1] = uv[1].GetU() / static_cast<float>(m_nTexWidth);

            nT[0] = uv[0].GetV() / static_cast<float>(m_nTexHeight);
            nT[1] = uv[1].GetV() / static_cast<float>(m_nTexHeight);
        }
        else
        {
            ST st[2];

            st[0] <<= m_VtxBuffer[1].st;
            st[1] <<= m_VtxBuffer[0].st;

            float nQ1 = rgbaq[1].nQ;
            float nQ2 = rgbaq[0].nQ;
            if(nQ1 == 0) nQ1 = 1;
            if(nQ2 == 0) nQ2 = 1;

            nS[0] = st[0].nS / nQ1;
            nS[1] = st[1].nS / nQ2;

            nT[0] = st[0].nT / nQ1;
            nT[1] = st[1].nT / nQ2;
        }
    }
*/
    auto color = MakeColor(
        rgbaq[1].nR, rgbaq[1].nG,
        rgbaq[1].nB, rgbaq[1].nA);

    // clang-format off
    PRIM_VERTEX vertices[] =
    {
        {nX1, nY1, nZ, color, nS[0], nT[0], 1, 0},
        {nX2, nY1, nZ, color, nS[1], nT[0], 1, 0},
        {nX1, nY2, nZ, color, nS[0], nT[1], 1, 0},

        {nX1, nY2, nZ, color, nS[0], nT[1], 1, 0},
        {nX2, nY1, nZ, color, nS[1], nT[0], 1, 0},
        {nX2, nY2, nZ, color, nS[1], nT[1], 1, 0},
    };
    // clang-format on

    m_vertexBuffer.insert(m_vertexBuffer.end(), std::begin(vertices), std::end(vertices));
}


/////////////////////////////////////////////////////////////
// Other Functions
/////////////////////////////////////////////////////////////

void CGSH_Metal::WriteRegisterImpl(uint8 registerId, uint64 data)
{
    CGSHandler::WriteRegisterImpl(registerId, data);

    switch(registerId)
    {
    case GS_REG_PRIM:
    {
        unsigned int newPrimitiveType = static_cast<unsigned int>(data & 0x07);
        if(newPrimitiveType != m_primitiveType)
        {
            FlushVertexBuffer();
        }
        m_primitiveType = newPrimitiveType;
        switch(m_primitiveType)
        {
        case PRIM_POINT:
            m_vtxCount = 1;
            break;
        case PRIM_LINE:
        case PRIM_LINESTRIP:
            m_vtxCount = 2;
            break;
        case PRIM_TRIANGLE:
        case PRIM_TRIANGLESTRIP:
        case PRIM_TRIANGLEFAN:
            m_vtxCount = 3;
            break;
        case PRIM_SPRITE:
            m_vtxCount = 2;
            break;
        }
    }
    break;

    case GS_REG_XYZ2:
    case GS_REG_XYZ3:
    case GS_REG_XYZF2:
    case GS_REG_XYZF3:
        VertexKick(registerId, data);
        break;
    }
}



void CGSH_Metal::ProcessLocalToHostTransfer()
{
	bool readsEnabled = CAppConfig::GetInstance().GetPreferenceBoolean(PREF_CGSHANDLER_GS_RAM_READS_ENABLED);
	if(readsEnabled)
	{
		//Make sure our local RAM copy is in sync with GPU
		SyncMemoryCache();
	}
}

void CGSH_Metal::ProcessLocalToLocalTransfer()
{

}

void CGSH_Metal::ProcessClutTransfer(uint32 csa, uint32)
{
}

void CGSH_Metal::BeginTransferWrite()
{
    // Called at the start of an image transfer. Clear the staging buffer. If it is not already clear, something has gone wrong.
    assert(m_xferBuffer.empty());
    m_xferBuffer.clear();
}

void CGSH_Metal::TransferWrite(const uint8* imageData, uint32 length)
{
    // Images are transfered in sections so this will be called a number of times.
    m_xferBuffer.insert(m_xferBuffer.end(), imageData, imageData + length);
}

void CGSH_Metal::ProcessHostToLocalTransfer()
{
    // called by CGSHandler::FeedImageDataImpl(const uint8* imageData, uint32 length) which is called by the GIF implementation when sending image data.
    // Called once the TransferWrite() calls are done.
    
    //Flush previous cached info
    /*
    memset(&m_clutStates, 0, sizeof(m_clutStates));
    m_draw->FlushRenderPass();

    auto bltBuf = make_convertible<BITBLTBUF>(m_nReg[GS_REG_BITBLTBUF]);
    auto trxReg = make_convertible<TRXREG>(m_nReg[GS_REG_TRXREG]);
    auto trxPos = make_convertible<TRXPOS>(m_nReg[GS_REG_TRXPOS]);

    m_transferHost->Params.bufAddress = bltBuf.GetDstPtr();
    m_transferHost->Params.bufWidth = bltBuf.GetDstWidth();
    m_transferHost->Params.rrw = trxReg.nRRW;
    m_transferHost->Params.dsax = trxPos.nDSAX;
    m_transferHost->Params.dsay = trxPos.nDSAY;

    auto pipelineCaps = make_convertible<CTransferHost::PIPELINE_CAPS>(0);
    pipelineCaps.dstFormat = bltBuf.nDstPsm;

    m_transferHost->SetPipelineCaps(pipelineCaps);
    m_transferHost->DoTransfer(m_xferBuffer);
*/
    m_xferBuffer.clear();
}

void CGSH_Metal::WriteBackMemoryCache()
{

}

void CGSH_Metal::SyncMemoryCache()
{

}

void CGSH_Metal::SyncCLUT(const TEX0& tex0)
{

}

Framework::CBitmap CGSH_Metal::GetScreenshot()
{
	return Framework::CBitmap();
}

bool CGSH_Metal::GetDepthTestingEnabled() const
{
	return m_depthTestingEnabled;
}

void CGSH_Metal::SetDepthTestingEnabled(bool depthTestingEnabled)
{
	m_depthTestingEnabled = depthTestingEnabled;
}

bool CGSH_Metal::GetAlphaBlendingEnabled() const
{
	return m_alphaBlendingEnabled;
}

void CGSH_Metal::SetAlphaBlendingEnabled(bool alphaBlendingEnabled)
{
	m_alphaBlendingEnabled = alphaBlendingEnabled;
}

bool CGSH_Metal::GetAlphaTestingEnabled() const
{
	return m_alphaTestingEnabled;
}

void CGSH_Metal::SetAlphaTestingEnabled(bool alphaTestingEnabled)
{
	m_alphaTestingEnabled = alphaTestingEnabled;
}

Framework::CBitmap CGSH_Metal::GetFramebuffer(uint64 frameReg)
{
	Framework::CBitmap result;
	SendGSCall([&]() { result = GetFramebufferImpl(frameReg); }, true);
	return result;
}

Framework::CBitmap CGSH_Metal::GetDepthbuffer(uint64 frameReg, uint64 zbufReg)
{
	Framework::CBitmap result;
	SendGSCall([&]() { result = GetDepthbufferImpl(frameReg, zbufReg); }, true);
	return result;
}

Framework::CBitmap CGSH_Metal::GetTexture(uint64 tex0Reg, uint32 maxMip, uint64 miptbp1Reg, uint64 miptbp2Reg, uint32 mipLevel)
{
	Framework::CBitmap result;
	SendGSCall([&]() { result = GetTextureImpl(tex0Reg, maxMip, miptbp1Reg, miptbp2Reg, mipLevel); }, true);
	return result;
}

int CGSH_Metal::GetFramebufferScale()
{
	return 1;
}

const CGSHandler::VERTEX* CGSH_Metal::GetInputVertices() const
{
	return m_vtxBuffer;
}

Framework::CBitmap CGSH_Metal::GetFramebufferImpl(uint64 frameReg)
{
	auto frame = make_convertible<FRAME>(frameReg);

	SyncMemoryCache();

	uint32 frameWidth = frame.GetWidth();
	uint32 frameHeight = 1024;
	Framework::CBitmap bitmap;

	switch(frame.nPsm)
	{
	case PSMCT32:
	{
		bitmap = ReadImage32<CGsPixelFormats::CPixelIndexorPSMCT32>(GetRam(), frame.GetBasePtr(),
		                                                            frame.nWidth, frameWidth, frameHeight);
	}
	break;
	case PSMCT24:
	{
		bitmap = ReadImage32<CGsPixelFormats::CPixelIndexorPSMCT32, 0x00FFFFFF>(GetRam(), frame.GetBasePtr(),
		                                                                        frame.nWidth, frameWidth, frameHeight);
	}
	break;
	case PSMCT16:
	{
		bitmap = ReadImage16<CGsPixelFormats::CPixelIndexorPSMCT16>(GetRam(), frame.GetBasePtr(),
		                                                            frame.nWidth, frameWidth, frameHeight);
	}
	break;
	case PSMCT16S:
	{
		bitmap = ReadImage16<CGsPixelFormats::CPixelIndexorPSMCT16S>(GetRam(), frame.GetBasePtr(),
		                                                             frame.nWidth, frameWidth, frameHeight);
	}
	break;
	default:
		assert(false);
		break;
	}
	return bitmap;
}

Framework::CBitmap CGSH_Metal::GetDepthbufferImpl(uint64 frameReg, uint64 zbufReg)
{
	auto frame = make_convertible<FRAME>(frameReg);
	auto zbuf = make_convertible<ZBUF>(zbufReg);

	SyncMemoryCache();

	uint32 frameWidth = frame.GetWidth();
	uint32 frameHeight = 1024;
	Framework::CBitmap bitmap;

	switch(zbuf.nPsm | 0x30)
	{
	case PSMZ32:
	{
		bitmap = ReadImage32<CGsPixelFormats::CPixelIndexorPSMZ32>(GetRam(), zbuf.GetBasePtr(),
		                                                           frame.nWidth, frameWidth, frameHeight);
	}
	break;
	case PSMZ24:
	{
		bitmap = ReadImage32<CGsPixelFormats::CPixelIndexorPSMZ32, 0x00FFFFFF>(GetRam(), zbuf.GetBasePtr(),
		                                                                       frame.nWidth, frameWidth, frameHeight);
	}
	break;
	default:
		assert(false);
		break;
	}

	return bitmap;
}

Framework::CBitmap CGSH_Metal::GetTextureImpl(uint64 tex0Reg, uint32 maxMip, uint64 miptbp1Reg, uint64 miptbp2Reg, uint32 mipLevel)
{
	auto tex0 = make_convertible<TEX0>(tex0Reg);
	auto width = std::max<uint32>(tex0.GetWidth() >> mipLevel, 1);
	auto height = std::max<uint32>(tex0.GetHeight() >> mipLevel, 1);

	SyncMemoryCache();

	Framework::CBitmap bitmap;

	switch(tex0.nPsm)
	{
	case PSMCT32:
	{
		bitmap = ReadImage32<CGsPixelFormats::CPixelIndexorPSMCT32>(GetRam(), tex0.GetBufPtr(),
		                                                            tex0.nBufWidth, tex0.GetWidth(), tex0.GetHeight());
	}
	break;
	case PSMT8:
	{
		bitmap = ReadImage8<CGsPixelFormats::CPixelIndexorPSMT8>(GetRam(), tex0.GetBufPtr(),
		                                                         tex0.nBufWidth, tex0.GetWidth(), tex0.GetHeight());
	}
	break;
	case PSMT4:
	{
		bitmap = ReadImage8<CGsPixelFormats::CPixelIndexorPSMT4>(GetRam(), tex0.GetBufPtr(),
		                                                         tex0.nBufWidth, tex0.GetWidth(), tex0.GetHeight());
	}
	break;
	}

	return bitmap;
}

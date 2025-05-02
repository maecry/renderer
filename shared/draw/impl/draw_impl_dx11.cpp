// used: [d3d]
#include <d3d11.h>
#include <d3dcompiler.h>
#ifdef _MSC_VER
#pragma comment(lib, "d3dcompiler")
#endif

#include "draw_impl_dx11.h"

// used: heapalloc
#include "../../utilities/memory.h"

struct DrawImplDX11Data_t
{
	ID3D11Device* pd3dDevice = nullptr;
	ID3D11DeviceContext* pd3dDeviceContext = nullptr;
	IDXGIFactory* pFactory = nullptr;
	ID3D11Buffer* pVB = nullptr;
	ID3D11Buffer* pIB = nullptr;
	ID3D11VertexShader* pVertexShader = nullptr;
	ID3D11InputLayout* pInputLayout = nullptr;
	ID3D11Buffer* pVertexConstantBuffer = nullptr;
	ID3D11PixelShader* pPixelShader = nullptr;
	ID3D11SamplerState* pFontSampler = nullptr;
	ID3D11ShaderResourceView* pFontTextureView = nullptr;
	ID3D11RasterizerState* pRasterizerState = nullptr;
	ID3D11BlendState* pBlendState = nullptr;
	ID3D11DepthStencilState* pDepthStencilState = nullptr;

	// @test: blur
	ID3D11Buffer* pBlurConstantBuffer = nullptr;
	ID3D11PixelShader* pBlurXPixelShader = nullptr;
	ID3D11PixelShader* pBlurYPixelShader = nullptr;
	ID3D11ShaderResourceView* pBlurTextureView = nullptr;
	ID3D11SamplerState* pBlurSampler = nullptr;

	int iVertexBufferSize = 5000, iIndexBufferSize = 10000;

	DrawImplDX11Data_t( )
	{
		CRT::MemorySet( this, 0, sizeof( *this ) );
		iVertexBufferSize = 5000;
		iIndexBufferSize = 10000;
	}
};

struct VERTEX_CONSTANT_BUFFER_DX11
{
	float mvp[ 4 ][ 4 ];
};

CDrawContextDX11::CDrawContextDX11( ID3D11Device* pDevice, ID3D11DeviceContext* pDeviceContext ) :
	CDrawContext::CDrawContext( )
{
	// @note: this never happened as it always setted to nullptr on CDrawContext::CDrawContext()
	CRT_ASSERTION( pBackendRendererUserData == nullptr && "already intilized a renderer backend" );

	DrawImplDX11Data_t* pData = MEM_NEW( DrawImplDX11Data_t )( );
	pBackendRendererUserData = pData;

	IDXGIDevice* pDXGIDevice = nullptr;
	IDXGIAdapter* pDXGIAdapter = nullptr;
	IDXGIFactory* pFactory = nullptr;

	if ( pDevice->QueryInterface( IID_PPV_ARGS( &pDXGIDevice ) ) == S_OK )
		if ( pDXGIDevice->GetParent( IID_PPV_ARGS( &pDXGIAdapter ) ) == S_OK )
			if ( pDXGIAdapter->GetParent( IID_PPV_ARGS( &pFactory ) ) == S_OK )
			{
				pData->pd3dDevice = pDevice;
				pData->pd3dDeviceContext = pDeviceContext;
				pData->pFactory = pFactory;
			}
	if ( pDXGIDevice )
		pDXGIDevice->Release( );
	if ( pDXGIAdapter )
		pDXGIAdapter->Release( );
	pData->pd3dDevice->AddRef( );
	pData->pd3dDeviceContext->AddRef( );
}

CDrawContextDX11::~CDrawContextDX11( )
{
	DrawImplDX11Data_t* pData = static_cast< DrawImplDX11Data_t* >( pBackendRendererUserData );
	CRT_ASSERTION( pData != nullptr && "no renderer backend to shutdown" );

	InvalidateDeviceObjects( );
	SafeRelease( pData->pFactory );
	SafeRelease( pData->pd3dDevice );
	SafeRelease( pData->pd3dDeviceContext );
	pBackendRendererUserData = nullptr;
	MEM_DELETE( pData );

	CDrawContext::~CDrawContext( );
}

void CDrawContextDX11::NewFrame( const DrawListFlags_t nDrawFlags )
{
	DrawImplDX11Data_t* pData = static_cast< DrawImplDX11Data_t* >( pBackendRendererUserData );
	CRT_ASSERTION( pData != nullptr && "no renderer backend to start new frame" );

	if ( pData->pFontSampler == nullptr )
		CreateDeviceObjects( );

	// reset our data every new frame
	CDrawContext::UpdateForNewFrame( );
}

void CDrawContextDX11::InvalidateDeviceObjects( )
{
	DrawImplDX11Data_t* pData = static_cast< DrawImplDX11Data_t* >( pBackendRendererUserData );
	if ( pData->pd3dDevice == nullptr )
		return;

	SafeRelease( pData->pFontSampler );
	SafeRelease( pData->pFontTextureView );
	SafeRelease( pData->pIB );
	SafeRelease( pData->pVB );
	SafeRelease( pData->pBlendState );
	SafeRelease( pData->pDepthStencilState );
	SafeRelease( pData->pRasterizerState );
	SafeRelease( pData->pPixelShader );
	SafeRelease( pData->pVertexConstantBuffer );
	SafeRelease( pData->pInputLayout );
	SafeRelease( pData->pVertexShader );
}

bool CDrawContextDX11::CreateDeviceObjects( )
{
	struct FontTexture_t
	{
		static void Create( DrawImplDX11Data_t* pData, CDrawContextDX11* pDrawContext )
		{
			unsigned char* pPixels;
			int iWidth, iHeight;
			pDrawContext->pFontAtlas->GetTexDataAsRGBA32( &pPixels, &iWidth, &iHeight );

			// upload texture to graphics system
			{
				D3D11_TEXTURE2D_DESC desc;
				ZeroMemory( &desc, sizeof( desc ) );
				desc.Width = iWidth;
				desc.Height = iHeight;
				desc.MipLevels = 1;
				desc.ArraySize = 1;
				desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
				desc.SampleDesc.Count = 1;
				desc.Usage = D3D11_USAGE_DEFAULT;
				desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
				desc.CPUAccessFlags = 0;

				ID3D11Texture2D* pTexture = nullptr;
				D3D11_SUBRESOURCE_DATA subResource;
				subResource.pSysMem = pPixels;
				subResource.SysMemPitch = desc.Width * 4;
				subResource.SysMemSlicePitch = 0;
				pData->pd3dDevice->CreateTexture2D( &desc, &subResource, &pTexture );
				CRT_ASSERTION( pTexture != nullptr );

				// Create texture view
				D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc;
				ZeroMemory( &srvDesc, sizeof( srvDesc ) );
				srvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
				srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
				srvDesc.Texture2D.MipLevels = desc.MipLevels;
				srvDesc.Texture2D.MostDetailedMip = 0;
				pData->pd3dDevice->CreateShaderResourceView( pTexture, &srvDesc, &pData->pFontTextureView );
				pTexture->Release( );
			}

			pDrawContext->pFontAtlas->pTexID = static_cast< TextureID_t >( pData->pFontTextureView );
			// create texture sampler
			{
				D3D11_SAMPLER_DESC desc;
				ZeroMemory( &desc, sizeof( desc ) );
				desc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
				desc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
				desc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
				desc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
				desc.MipLODBias = 0.f;
				desc.ComparisonFunc = D3D11_COMPARISON_ALWAYS;
				desc.MinLOD = 0.f;
				desc.MaxLOD = 0.f;
				pData->pd3dDevice->CreateSamplerState( &desc, &pData->pFontSampler );
			}
		}
	};

	DrawImplDX11Data_t* pData = static_cast< DrawImplDX11Data_t* >( pBackendRendererUserData );
	if ( pData->pd3dDevice == nullptr )
		return false;

	if ( pData->pFontSampler )
		InvalidateDeviceObjects( );

	// create the vertex shader and input layout
	{
		static constexpr const char* szVertexShader =
			"cbuffer vertexBuffer : register(b0) \
        {\
          float4x4 ProjectionMatrix; \
        };\
        struct VS_INPUT\
        {\
          float2 pos : POSITION;\
          float4 col : COLOR0;\
          float2 uv  : TEXCOORD0;\
        };\
        \
        struct PS_INPUT\
        {\
          float4 pos : SV_POSITION;\
          float4 col : COLOR0;\
          float2 uv  : TEXCOORD0;\
        };\
        \
        PS_INPUT main(VS_INPUT input)\
        {\
          PS_INPUT output;\
          output.pos = mul( ProjectionMatrix, float4(input.pos.xy, 0.f, 1.f));\
          output.col = input.col;\
          output.uv  = input.uv;\
          return output;\
		}";

		pData->pVertexShader = static_cast< ID3D11VertexShader* >( this->CreateVertexShader( szVertexShader ) );

		CRT_ASSERTION( pData->pVertexShader != nullptr && pData->pInputLayout != nullptr && pData->pVertexConstantBuffer != nullptr );
	}

	// create the pixel shader
	{
		static constexpr const char* szPixelShader =
			"struct PS_INPUT\
        {\
			float4 pos : SV_POSITION;\
			float4 col : COLOR0;\
			float2 uv  : TEXCOORD0;\
        };\
        sampler sampler0;\
        Texture2D texture0;\
        \
        float4 main(PS_INPUT input) : SV_Target\
        {\
			float4 out_col = input.col * texture0.Sample(sampler0, input.uv); \
			return out_col; \
        }";

		pData->pPixelShader = static_cast< ID3D11PixelShader* >( this->CreatePixelShader( szPixelShader ) );
		CRT_ASSERTION( pData->pPixelShader != nullptr );
	}

	{
		D3D11_BLEND_DESC desc;
		ZeroMemory( &desc, sizeof( desc ) );
		desc.AlphaToCoverageEnable = false;
		desc.RenderTarget[ 0 ].BlendEnable = true;
		desc.RenderTarget[ 0 ].SrcBlend = D3D11_BLEND_SRC_ALPHA;
		desc.RenderTarget[ 0 ].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
		desc.RenderTarget[ 0 ].BlendOp = D3D11_BLEND_OP_ADD;
		desc.RenderTarget[ 0 ].SrcBlendAlpha = D3D11_BLEND_ONE;
		desc.RenderTarget[ 0 ].DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA;
		desc.RenderTarget[ 0 ].BlendOpAlpha = D3D11_BLEND_OP_ADD;
		desc.RenderTarget[ 0 ].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
		pData->pd3dDevice->CreateBlendState( &desc, &pData->pBlendState );
	}

	// create the rasterizer state
	{
		D3D11_RASTERIZER_DESC desc;
		ZeroMemory( &desc, sizeof( desc ) );
		desc.FillMode = D3D11_FILL_SOLID;
		desc.CullMode = D3D11_CULL_NONE;
		desc.ScissorEnable = true;
		desc.DepthClipEnable = true;
		pData->pd3dDevice->CreateRasterizerState( &desc, &pData->pRasterizerState );
	}

	// create depth-stencil State
	{
		D3D11_DEPTH_STENCIL_DESC desc;
		ZeroMemory( &desc, sizeof( desc ) );
		desc.DepthEnable = false;
		desc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
		desc.DepthFunc = D3D11_COMPARISON_ALWAYS;
		desc.StencilEnable = false;
		desc.FrontFace.StencilFailOp = desc.FrontFace.StencilDepthFailOp = desc.FrontFace.StencilPassOp = D3D11_STENCIL_OP_KEEP;
		desc.FrontFace.StencilFunc = D3D11_COMPARISON_ALWAYS;
		desc.BackFace = desc.FrontFace;
		pData->pd3dDevice->CreateDepthStencilState( &desc, &pData->pDepthStencilState );
	}

	FontTexture_t::Create( pData, this );

	return true;
}

void CDrawContextDX11::Render( )
{
	struct RenderState_t
	{
		static void Setup( DrawData_t* pDrawData, DrawImplDX11Data_t* pData )
		{
			D3D11_VIEWPORT vp;
			CRT::MemorySet( &vp, 0, sizeof( D3D11_VIEWPORT ) );
			vp.Width = pDrawData->vecDisplaySize.x;
			vp.Height = pDrawData->vecDisplaySize.y;
			vp.MinDepth = 0.0f;
			vp.MaxDepth = 1.0f;
			vp.TopLeftX = vp.TopLeftY = 0;
			pData->pd3dDeviceContext->RSSetViewports( 1, &vp );

			unsigned int stride = sizeof( DrawVert_t );
			unsigned int offset = 0;

			pData->pd3dDeviceContext->IASetInputLayout( pData->pInputLayout );
			pData->pd3dDeviceContext->IASetVertexBuffers( 0, 1, &pData->pVB, &stride, &offset );
			pData->pd3dDeviceContext->IASetIndexBuffer( pData->pIB, sizeof( DrawIdx_t ) == 2 ? DXGI_FORMAT_R16_UINT : DXGI_FORMAT_R32_UINT, 0 );
			pData->pd3dDeviceContext->IASetPrimitiveTopology( pDrawData->bDebugWireframe ? D3D11_PRIMITIVE_TOPOLOGY_LINELIST : D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST );
			pData->pd3dDeviceContext->VSSetShader( pData->pVertexShader, nullptr, 0 );
			pData->pd3dDeviceContext->VSSetConstantBuffers( 0, 1, &pData->pVertexConstantBuffer );
			pData->pd3dDeviceContext->PSSetShader( pData->pPixelShader, nullptr, 0 );
			pData->pd3dDeviceContext->PSSetSamplers( 0, 1, &pData->pFontSampler );
			pData->pd3dDeviceContext->GSSetShader( nullptr, nullptr, 0 );
			pData->pd3dDeviceContext->HSSetShader( nullptr, nullptr, 0 ); // In theory we should backup and restore this as well.. very infrequently used..
			pData->pd3dDeviceContext->DSSetShader( nullptr, nullptr, 0 ); // In theory we should backup and restore this as well.. very infrequently used..
			pData->pd3dDeviceContext->CSSetShader( nullptr, nullptr, 0 ); // In theory we should backup and restore this as well.. very infrequently used..

			// Setup blend state
			const float blend_factor[ 4 ] = { 0.f, 0.f, 0.f, 0.f };
			pData->pd3dDeviceContext->OMSetBlendState( pData->pBlendState, blend_factor, 0xffffffff );
			pData->pd3dDeviceContext->OMSetDepthStencilState( pData->pDepthStencilState, 0 );
			pData->pd3dDeviceContext->RSSetState( pData->pRasterizerState );
		}
	};

	// build our draw data
	DrawData_t* pDrawData = CDrawContext::BuildDrawData( );
	if ( pDrawData->bValid == false )
		return;

	if ( pDrawData->vecDisplaySize.x <= 0.0f || pDrawData->vecDisplaySize.y <= 0.0f )
		return;

	DrawImplDX11Data_t* pData = static_cast< DrawImplDX11Data_t* >( pBackendRendererUserData );
	if ( pData->pd3dDevice == nullptr )
		return;

	ID3D11DeviceContext* ctx = pData->pd3dDeviceContext;

	// Create and grow vertex/index buffers if needed
	if ( pData->pVB == nullptr || pData->iVertexBufferSize < pDrawData->iTotalVtxCount )
	{
		SafeRelease( pData->pVB );
		pData->iVertexBufferSize = pDrawData->iTotalVtxCount + 5000;
		D3D11_BUFFER_DESC desc;
		CRT::MemorySet( &desc, 0, sizeof( D3D11_BUFFER_DESC ) );
		desc.Usage = D3D11_USAGE_DYNAMIC;
		desc.ByteWidth = pData->iVertexBufferSize * sizeof( DrawVert_t );
		desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
		desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
		desc.MiscFlags = 0;
		if ( pData->pd3dDevice->CreateBuffer( &desc, nullptr, &pData->pVB ) < 0 )
			return;
	}
	if ( pData->pIB == nullptr || pData->iIndexBufferSize < pDrawData->iTotalIdxCount )
	{
		SafeRelease( pData->pIB );
		pData->iIndexBufferSize = pDrawData->iTotalIdxCount + 10000;
		D3D11_BUFFER_DESC desc;
		CRT::MemorySet( &desc, 0, sizeof( D3D11_BUFFER_DESC ) );
		desc.Usage = D3D11_USAGE_DYNAMIC;
		desc.ByteWidth = pData->iIndexBufferSize * sizeof( DrawIdx_t );
		desc.BindFlags = D3D11_BIND_INDEX_BUFFER;
		desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
		if ( pData->pd3dDevice->CreateBuffer( &desc, nullptr, &pData->pIB ) < 0 )
			return;
	}

	// Upload vertex/index data into a single contiguous GPU buffer
	D3D11_MAPPED_SUBRESOURCE vtx_resource, idx_resource;
	if ( ctx->Map( pData->pVB, 0, D3D11_MAP_WRITE_DISCARD, 0, &vtx_resource ) != S_OK )
		return;
	if ( ctx->Map( pData->pIB, 0, D3D11_MAP_WRITE_DISCARD, 0, &idx_resource ) != S_OK )
		return;
	DrawVert_t* vtx_dst = ( DrawVert_t* )vtx_resource.pData;
	DrawIdx_t* idx_dst = ( DrawIdx_t* )idx_resource.pData;
	for ( int n = 0; n < pDrawData->iCmdListsCount; n++ )
	{
		const DrawList_t* cmd_list = pDrawData->ppCmdLists[ n ];
		CRT::MemoryCopy( vtx_dst, cmd_list->vecVtxBuffer.Data, cmd_list->vecVtxBuffer.Size * sizeof( DrawVert_t ) );
		CRT::MemoryCopy( idx_dst, cmd_list->vecIdxBuffer.Data, cmd_list->vecIdxBuffer.Size * sizeof( DrawIdx_t ) );
		vtx_dst += cmd_list->vecVtxBuffer.Size;
		idx_dst += cmd_list->vecIdxBuffer.Size;
	}
	ctx->Unmap( pData->pVB, 0 );
	ctx->Unmap( pData->pIB, 0 );

	// Setup orthographic projection matrix into our constant buffer
	// Our visible space lies from pDrawData->vecDisplayPos (top left) to pDrawData->vecDisplayPos + pDrawData->vecDisplaySize (bottom right). vecDisplayPos is (0,0) for single viewport apps.
	{
		D3D11_MAPPED_SUBRESOURCE mapped_resource;
		if ( ctx->Map( pData->pVertexConstantBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped_resource ) != S_OK )
			return;
		VERTEX_CONSTANT_BUFFER_DX11* constant_buffer = ( VERTEX_CONSTANT_BUFFER_DX11* )mapped_resource.pData;
		float L = pDrawData->vecDisplayPos.x;
		float R = pDrawData->vecDisplayPos.x + pDrawData->vecDisplaySize.x;
		float T = pDrawData->vecDisplayPos.y;
		float B = pDrawData->vecDisplayPos.y + pDrawData->vecDisplaySize.y;
		float mvp[ 4 ][ 4 ] = {
			{ 2.0f / ( R - L ), 0.0f, 0.0f, 0.0f },
			{ 0.0f, 2.0f / ( T - B ), 0.0f, 0.0f },
			{ 0.0f, 0.0f, 0.5f, 0.0f },
			{ ( R + L ) / ( L - R ), ( T + B ) / ( B - T ), 0.5f, 1.0f },
		};
		CRT::MemoryCopy( &constant_buffer->mvp, mvp, sizeof( mvp ) );
		ctx->Unmap( pData->pVertexConstantBuffer, 0 );
	}

	struct BACKUP_DX11_STATE
	{
		UINT ScissorRectsCount, ViewportsCount;
		D3D11_RECT ScissorRects[ D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE ];
		D3D11_VIEWPORT Viewports[ D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE ];
		ID3D11RasterizerState* RS;
		ID3D11BlendState* BlendState;
		FLOAT BlendFactor[ 4 ];
		UINT SampleMask;
		UINT StencilRef;
		ID3D11DepthStencilState* DepthStencilState;
		ID3D11ShaderResourceView* PSShaderResource;
		ID3D11SamplerState* PSSampler;
		ID3D11PixelShader* PS;
		ID3D11VertexShader* VS;
		ID3D11GeometryShader* GS;
		UINT PSInstancesCount, VSInstancesCount, GSInstancesCount;
		ID3D11ClassInstance* PSInstances[ 256 ], * VSInstances[ 256 ], * GSInstances[ 256 ]; // 256 is max according to PSSetShader documentation
		D3D11_PRIMITIVE_TOPOLOGY PrimitiveTopology;
		ID3D11Buffer* IndexBuffer, * VertexBuffer, * VSConstantBuffer;
		UINT IndexBufferOffset, VertexBufferStride, VertexBufferOffset;
		DXGI_FORMAT IndexBufferFormat;
		ID3D11InputLayout* InputLayout;
	};

	BACKUP_DX11_STATE old = {};
	old.ScissorRectsCount = old.ViewportsCount = D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE;
	ctx->RSGetScissorRects( &old.ScissorRectsCount, old.ScissorRects );
	ctx->RSGetViewports( &old.ViewportsCount, old.Viewports );
	ctx->RSGetState( &old.RS );
	ctx->OMGetBlendState( &old.BlendState, old.BlendFactor, &old.SampleMask );
	ctx->OMGetDepthStencilState( &old.DepthStencilState, &old.StencilRef );
	ctx->PSGetShaderResources( 0, 1, &old.PSShaderResource );
	ctx->PSGetSamplers( 0, 1, &old.PSSampler );
	old.PSInstancesCount = old.VSInstancesCount = old.GSInstancesCount = 256;
	ctx->PSGetShader( &old.PS, old.PSInstances, &old.PSInstancesCount );
	ctx->VSGetShader( &old.VS, old.VSInstances, &old.VSInstancesCount );
	ctx->VSGetConstantBuffers( 0, 1, &old.VSConstantBuffer );
	ctx->GSGetShader( &old.GS, old.GSInstances, &old.GSInstancesCount );

	ctx->IAGetPrimitiveTopology( &old.PrimitiveTopology );
	ctx->IAGetIndexBuffer( &old.IndexBuffer, &old.IndexBufferFormat, &old.IndexBufferOffset );
	ctx->IAGetVertexBuffers( 0, 1, &old.VertexBuffer, &old.VertexBufferStride, &old.VertexBufferOffset );
	ctx->IAGetInputLayout( &old.InputLayout );

	// Setup desired DX state
	RenderState_t::Setup( &drawData, pData );

	// Render command lists
	// (Because we merged all buffers into a single one, we maintain our own offset into them)
	int global_idx_offset = 0;
	int global_vtx_offset = 0;
	Vector2D_t clip_off = pDrawData->vecDisplayPos;
	for ( int n = 0; n < pDrawData->iCmdListsCount; n++ )
	{
		const DrawList_t* cmd_list = pDrawData->ppCmdLists[ n ];
		for ( size_t cmd_i = 0; cmd_i < cmd_list->vecCmdBuffer.Size; cmd_i++ )
		{
			const DrawCmd_t* pcmd = &cmd_list->vecCmdBuffer[ cmd_i ];
			if ( pcmd->fnUserCallback != nullptr )
			{
				// User callback, registered via ImDrawList::AddCallback()
				// (ImDrawCallback_ResetRenderState is a special callback value used by the user to request the renderer to reset render state.)
				if ( pcmd->fnUserCallback == DrawCallback_ResetRenderState )
					RenderState_t::Setup( &drawData, pData );
				else
					pcmd->fnUserCallback( cmd_list, pcmd );
			}
			else
			{
				// Project scissor/clipping rectangles into framebuffer space
				Vector2D_t clip_min( pcmd->vecClipRect.x - clip_off.x, pcmd->vecClipRect.y - clip_off.y );
				Vector2D_t clip_max( pcmd->vecClipRect.z - clip_off.x, pcmd->vecClipRect.w - clip_off.y );
				if ( clip_max.x <= clip_min.x || clip_max.y <= clip_min.y )
					continue;

				// Apply scissor/clipping rectangle
				const D3D11_RECT r = { ( LONG )clip_min.x, ( LONG )clip_min.y, ( LONG )clip_max.x, ( LONG )clip_max.y };
				ctx->RSSetScissorRects( 1, &r );

				// Bind texture, Draw
				ID3D11ShaderResourceView* texture_srv = ( ID3D11ShaderResourceView* )pcmd->pTextureId;
				ctx->PSSetShaderResources( 0, 1, &texture_srv );
				ctx->DrawIndexed( pcmd->nElemCount, pcmd->nIdxOffset + global_idx_offset, pcmd->nVtxOffset + global_vtx_offset );
			}
		}
		global_idx_offset += cmd_list->vecIdxBuffer.Size;
		global_vtx_offset += cmd_list->vecVtxBuffer.Size;
	}

	// Restore modified DX state
	ctx->RSSetScissorRects( old.ScissorRectsCount, old.ScissorRects );
	ctx->RSSetViewports( old.ViewportsCount, old.Viewports );
	ctx->RSSetState( old.RS );
	if ( old.RS )
		old.RS->Release( );
	ctx->OMSetBlendState( old.BlendState, old.BlendFactor, old.SampleMask );
	if ( old.BlendState )
		old.BlendState->Release( );
	ctx->OMSetDepthStencilState( old.DepthStencilState, old.StencilRef );
	if ( old.DepthStencilState )
		old.DepthStencilState->Release( );
	ctx->PSSetShaderResources( 0, 1, &old.PSShaderResource );
	if ( old.PSShaderResource )
		old.PSShaderResource->Release( );
	ctx->PSSetSamplers( 0, 1, &old.PSSampler );
	if ( old.PSSampler )
		old.PSSampler->Release( );
	ctx->PSSetShader( old.PS, old.PSInstances, old.PSInstancesCount );
	if ( old.PS )
		old.PS->Release( );
	for ( UINT i = 0; i < old.PSInstancesCount; i++ )
		if ( old.PSInstances[ i ] )
			old.PSInstances[ i ]->Release( );
	ctx->VSSetShader( old.VS, old.VSInstances, old.VSInstancesCount );
	if ( old.VS )
		old.VS->Release( );
	ctx->VSSetConstantBuffers( 0, 1, &old.VSConstantBuffer );
	if ( old.VSConstantBuffer )
		old.VSConstantBuffer->Release( );
	ctx->GSSetShader( old.GS, old.GSInstances, old.GSInstancesCount );
	if ( old.GS )
		old.GS->Release( );
	for ( UINT i = 0; i < old.VSInstancesCount; i++ )
		if ( old.VSInstances[ i ] )
			old.VSInstances[ i ]->Release( );
	ctx->IASetPrimitiveTopology( old.PrimitiveTopology );
	ctx->IASetIndexBuffer( old.IndexBuffer, old.IndexBufferFormat, old.IndexBufferOffset );
	if ( old.IndexBuffer )
		old.IndexBuffer->Release( );
	ctx->IASetVertexBuffers( 0, 1, &old.VertexBuffer, &old.VertexBufferStride, &old.VertexBufferOffset );
	if ( old.VertexBuffer )
		old.VertexBuffer->Release( );
	ctx->IASetInputLayout( old.InputLayout );
	if ( old.InputLayout )
		old.InputLayout->Release( );
}

void* CDrawContextDX11::CreateVertexShader( const char* szSource )
{
	DrawImplDX11Data_t* pData = static_cast< DrawImplDX11Data_t* >( pBackendRendererUserData );
	if ( pData->pd3dDevice == nullptr )
		return nullptr;

	ID3D11VertexShader* pVertexShader = nullptr;
	ID3DBlob* pVertexShaderBlob;
	ID3DBlob* pErrorBlob;
	if ( FAILED( D3DCompile( szSource, CRT::StringLength( szSource ), nullptr, nullptr, nullptr, "main", "vs_4_0", 0, 0, &pVertexShaderBlob, &pErrorBlob ) ) )
	{
		SafeRelease( pErrorBlob );
		SafeRelease( pVertexShaderBlob );
		return nullptr;
	}
	if ( pData->pd3dDevice->CreateVertexShader( pVertexShaderBlob->GetBufferPointer( ), pVertexShaderBlob->GetBufferSize( ), nullptr, &pVertexShader ) != S_OK )
	{
		SafeRelease( pErrorBlob );
		SafeRelease( pVertexShaderBlob );
		return nullptr;
	}

	// Create the input layout
	D3D11_INPUT_ELEMENT_DESC arrLayout[ ] = {
		{ "POSITION", 0, DXGI_FORMAT_R32G32_FLOAT, 0, ( UINT )CRT_OFFSETOF( DrawVert_t, vecPostion ), D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, ( UINT )CRT_OFFSETOF( DrawVert_t, vecUV ), D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "COLOR", 0, DXGI_FORMAT_R8G8B8A8_UNORM, 0, ( UINT )CRT_OFFSETOF( DrawVert_t, uCol ), D3D11_INPUT_PER_VERTEX_DATA, 0 },
	};
	if ( pData->pd3dDevice->CreateInputLayout( arrLayout, 3, pVertexShaderBlob->GetBufferPointer( ), pVertexShaderBlob->GetBufferSize( ), &pData->pInputLayout ) != S_OK )
	{
		SafeRelease( pErrorBlob );
		SafeRelease( pVertexShaderBlob );
		return nullptr;
	}
	SafeRelease( pErrorBlob );
	SafeRelease( pVertexShaderBlob );

	// Create the constant buffer
	{
		D3D11_BUFFER_DESC desc;
		desc.ByteWidth = sizeof( VERTEX_CONSTANT_BUFFER_DX11 );
		desc.Usage = D3D11_USAGE_DYNAMIC;
		desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
		desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
		desc.MiscFlags = 0;
		if ( pData->pd3dDevice->CreateBuffer( &desc, nullptr, &pData->pVertexConstantBuffer ) != S_OK )
		{
			return nullptr;
		}
	}

	return static_cast< void* >( pVertexShader );
}

void CDrawContextDX11::DestroyVertexShader( void* pShader )
{
	ID3D11VertexShader* pVertexShader = static_cast< ID3D11VertexShader* >( pShader );
	if ( pVertexShader == nullptr )
		return;

	pVertexShader->Release( );
	pVertexShader = nullptr;
}

void* CDrawContextDX11::CreatePixelShader( const char* szSource )
{
	DrawImplDX11Data_t* pData = static_cast< DrawImplDX11Data_t* >( pBackendRendererUserData );
	if ( pData->pd3dDevice == nullptr )
		return nullptr;

	ID3D11PixelShader* pPixelShader = nullptr;
	ID3DBlob* pPixelShaderBlob = nullptr;
	ID3DBlob* pErrorBlob = nullptr;
	if ( FAILED( D3DCompile( szSource, CRT::StringLength( szSource ), nullptr, nullptr, nullptr, "main", "ps_4_0", 0, 0, &pPixelShaderBlob, &pErrorBlob ) ) )
	{
		SafeRelease( pErrorBlob );
		SafeRelease( pPixelShaderBlob );
		return nullptr;
	}
	if ( pData->pd3dDevice->CreatePixelShader( pPixelShaderBlob->GetBufferPointer( ), pPixelShaderBlob->GetBufferSize( ), nullptr, &pPixelShader ) != S_OK )
	{
		SafeRelease( pErrorBlob );
		SafeRelease( pPixelShaderBlob );
		return nullptr;
	}
	SafeRelease( pErrorBlob );
	SafeRelease( pPixelShaderBlob );

	return static_cast< void* >( pPixelShader );
}

void CDrawContextDX11::DestroyPixelShader( void* pShader )
{
	ID3D11PixelShader* pPixelShader = static_cast< ID3D11PixelShader* >( pShader );
	if ( pPixelShader == nullptr )
		return;

	pPixelShader->Release( );
	pPixelShader = nullptr;
}
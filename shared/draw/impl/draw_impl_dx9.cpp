// used: [d3d]
#include <d3d9.h>

#include "../draw.h"
#include "draw_impl_dx9.h"

// used: heapalloc
#include "../../utilities/memory.h"

// DirectX data
struct DrawImplDX9Data_t
{
	LPDIRECT3DDEVICE9 pd3dDevice = NULL;
	LPDIRECT3DVERTEXBUFFER9 pVB = NULL;
	LPDIRECT3DINDEXBUFFER9 pIB = NULL;
	LPDIRECT3DTEXTURE9 pFontTexture = NULL;
	int iVertexBufferSize = 5000, iIndexBufferSize = 10000;

	DrawImplDX9Data_t( )
	{
		CRT::MemorySet( this, 0, sizeof( *this ) );
		iVertexBufferSize = 5000;
		iIndexBufferSize = 10000;
	}
};

struct CUSTOMVERTEX
{
	float vecPostion[ 3 ];
	D3DCOLOR uCol;
	float vecUV[ 2 ];
};

#define D3DFVF_CUSTOMVERTEX (D3DFVF_XYZ | D3DFVF_DIFFUSE | D3DFVF_TEX1)

CDrawContextDX9::CDrawContextDX9( IDirect3DDevice9* device ) :
	CDrawContext::CDrawContext( )
{
	// @note: this never happened as it always setted to nullptr on CDrawContext::CDrawContext()
	CRT_ASSERTION( pBackendRendererUserData == nullptr && "already intilized a renderer backend" );

	DrawImplDX9Data_t* pData = MEM_NEW( DrawImplDX9Data_t )( );
	pBackendRendererUserData = pData;

	pData->pd3dDevice = device;
	pData->pd3dDevice->AddRef( );
}

CDrawContextDX9::~CDrawContextDX9( )
{
	DrawImplDX9Data_t* pData = static_cast< DrawImplDX9Data_t* >( pBackendRendererUserData );
	CRT_ASSERTION( pData != nullptr && "no renderer backend to shutdown" );

	InvalidateDeviceObjects( );
	SafeRelease( pData->pd3dDevice );
	CDrawContext::~CDrawContext( );
}

void CDrawContextDX9::NewFrame( const DrawListFlags_t nDrawFlags )
{
	if ( pBackendRendererUserData == NULL )
		return;

	DrawImplDX9Data_t* pData = static_cast< DrawImplDX9Data_t* >( pBackendRendererUserData );

	if ( pData->pFontTexture == NULL )
		CreateDeviceObjects( );

	// reset draw every new frame
	CDrawContext::UpdateForNewFrame( );
}

bool CDrawContextDX9::CreateDeviceObjects( )
{
	struct FontTexture_t
	{
		static bool Create( CDrawContext* ctx )
		{
			if ( ctx->pBackendRendererUserData == NULL )
				return false;

			DrawImplDX9Data_t* pData = static_cast< DrawImplDX9Data_t* >( ctx->pBackendRendererUserData );


			// Build texture atlas
			unsigned char* pixels;
			int width, height, bytes_per_pixel;
			ctx->pFontAtlas->GetTexDataAsRGBA32( &pixels, &width, &height, &bytes_per_pixel );

			// Upload texture to graphics system
			pData->pFontTexture = NULL;
			if ( pData->pd3dDevice->CreateTexture( width, height, 1, D3DUSAGE_DYNAMIC, D3DFMT_A8R8G8B8, D3DPOOL_DEFAULT, &pData->pFontTexture, NULL ) < 0 )
				return false;
			D3DLOCKED_RECT tex_locked_rect;
			if ( pData->pFontTexture->LockRect( 0, &tex_locked_rect, NULL, 0 ) != D3D_OK )
				return false;
			for ( int y = 0; y < height; y++ )
				CRT::MemoryCopy( ( unsigned char* )tex_locked_rect.pBits + tex_locked_rect.Pitch * y, pixels + ( width * bytes_per_pixel ) * y, ( width * bytes_per_pixel ) );
			pData->pFontTexture->UnlockRect( 0 );

			// Store our identifier
			ctx->pFontAtlas->pTexID = static_cast< TextureID_t >( pData->pFontTexture );

			return true;
		}
	};

	if ( pBackendRendererUserData == NULL )
		return false;

	DrawImplDX9Data_t* pData = static_cast< DrawImplDX9Data_t* >( pBackendRendererUserData );

	if ( pData->pd3dDevice == NULL )
		return false;
	if ( !FontTexture_t::Create( this ) )
		return false;
	return true;
}

void CDrawContextDX9::InvalidateDeviceObjects( )
{
	if ( pBackendRendererUserData == NULL )
		return;

	DrawImplDX9Data_t* pData = static_cast< DrawImplDX9Data_t* >( pBackendRendererUserData );

	if ( pData->pd3dDevice == nullptr )
		return;

	SafeRelease( pData->pVB );
	SafeRelease( pData->pIB );
	SafeRelease( pData->pFontTexture );
	pFontAtlas->pTexID = nullptr;
}

void* CDrawContextDX9::CreateVertexShader( const char* szSource )
{
	CRT_ASSERTION( false && "not implemented" );
	return nullptr;
}

void CDrawContextDX9::DestroyVertexShader( void* pShader )
{
	CRT_ASSERTION( false && "not implemented" );
}

void* CDrawContextDX9::CreatePixelShader( const char* szSource )
{
	CRT_ASSERTION( false && "not implemented" );
	return nullptr;
}

void CDrawContextDX9::DestroyPixelShader( void* pShader )
{
	CRT_ASSERTION( false && "not implemented" );
}

void CDrawContextDX9::Render( )
{
	struct RenderState_t
	{
		static void Setup( DrawImplDX9Data_t* pData, DrawData_t* pDrawData )
		{
			// Setup viewport
			D3DVIEWPORT9 vp;
			vp.X = vp.Y = 0;
			vp.Width = ( DWORD )pDrawData->vecDisplaySize.x;
			vp.Height = ( DWORD )pDrawData->vecDisplaySize.y;
			vp.MinZ = 0.0f;
			vp.MaxZ = 1.0f;
			pData->pd3dDevice->SetViewport( &vp );

			// Setup render state: fixed-pipeline, alpha-blending, no face culling, no depth testing, shade mode (for gradient)
			pData->pd3dDevice->SetPixelShader( NULL );
			pData->pd3dDevice->SetVertexShader( NULL );
			pData->pd3dDevice->SetRenderState( D3DRS_CULLMODE, D3DCULL_NONE );
			pData->pd3dDevice->SetRenderState( D3DRS_LIGHTING, false );
			pData->pd3dDevice->SetRenderState( D3DRS_ZENABLE, false );
			pData->pd3dDevice->SetRenderState( D3DRS_ZWRITEENABLE, false );
			pData->pd3dDevice->SetRenderState( D3DRS_ALPHABLENDENABLE, true );
			pData->pd3dDevice->SetRenderState( D3DRS_ALPHATESTENABLE, false );
			pData->pd3dDevice->SetRenderState( D3DRS_BLENDOP, D3DBLENDOP_ADD );
			pData->pd3dDevice->SetRenderState( D3DRS_SRCBLEND, D3DBLEND_SRCALPHA );
			pData->pd3dDevice->SetRenderState( D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA );
			pData->pd3dDevice->SetRenderState( D3DRS_SCISSORTESTENABLE, true );
			pData->pd3dDevice->SetRenderState( D3DRS_SHADEMODE, D3DSHADE_GOURAUD );
			pData->pd3dDevice->SetRenderState( D3DRS_FOGENABLE, false );
			pData->pd3dDevice->SetTextureStageState( 0, D3DTSS_COLOROP, D3DTOP_MODULATE );
			pData->pd3dDevice->SetTextureStageState( 0, D3DTSS_COLORARG1, D3DTA_TEXTURE );
			pData->pd3dDevice->SetTextureStageState( 0, D3DTSS_COLORARG2, D3DTA_DIFFUSE );
			pData->pd3dDevice->SetTextureStageState( 0, D3DTSS_ALPHAOP, D3DTOP_MODULATE );
			pData->pd3dDevice->SetTextureStageState( 0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE );
			pData->pd3dDevice->SetTextureStageState( 0, D3DTSS_ALPHAARG2, D3DTA_DIFFUSE );
			pData->pd3dDevice->SetSamplerState( 0, D3DSAMP_MINFILTER, D3DTEXF_LINEAR );
			pData->pd3dDevice->SetSamplerState( 0, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR );
			if ( pDrawData->bDebugWireframe )
				pData->pd3dDevice->SetRenderState( D3DRS_FILLMODE, D3DFILL_WIREFRAME );

			// Setup orthographic projection matrix
			// Our visible imgui space lies from pDrawData->vecDisplayPos (top left) to pDrawData->vecDisplayPos+data_data->vecDisplaySize (bottom right). vecDisplayPos is (0,0) for single viewport apps.
			// Being agnostic of whether <d3dx9.h> or <DirectXMath.h> can be used, we aren't relying on D3DXMatrixIdentity()/D3DXMatrixOrthoOffCenterLH() or DirectX::XMMatrixIdentity()/DirectX::XMMatrixOrthographicOffCenterLH()
			{
				float L = pDrawData->vecDisplayPos.x + 0.5f;
				float R = pDrawData->vecDisplayPos.x + pDrawData->vecDisplaySize.x + 0.5f;
				float T = pDrawData->vecDisplayPos.y + 0.5f;
				float B = pDrawData->vecDisplayPos.y + pDrawData->vecDisplaySize.y + 0.5f;
				D3DMATRIX mat_identity = { { { 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f } } };
				D3DMATRIX mat_projection = { { { 2.0f / ( R - L ), 0.0f, 0.0f, 0.0f,
				0.0f, 2.0f / ( T - B ), 0.0f, 0.0f,
				0.0f, 0.0f, 0.5f, 0.0f,
				( L + R ) / ( L - R ), ( T + B ) / ( B - T ), 0.5f, 1.0f } } };
				pData->pd3dDevice->SetTransform( D3DTS_WORLD, &mat_identity );
				pData->pd3dDevice->SetTransform( D3DTS_VIEW, &mat_identity );
				pData->pd3dDevice->SetTransform( D3DTS_PROJECTION, &mat_projection );
			}
		}
	};

	if ( pBackendRendererUserData == NULL )
		return;

	DrawImplDX9Data_t* pData = static_cast< DrawImplDX9Data_t* >( pBackendRendererUserData );

	// build our draw data
	DrawData_t* pDrawData = CDrawContext::BuildDrawData( );
	if ( pDrawData->bValid == false )
		return;

	// Avoid rendering when minimized
	if ( pDrawData->vecDisplaySize.x <= 0.0f || pDrawData->vecDisplaySize.y <= 0.0f )
		return;

	// Create and grow buffers if needed
	if ( pData->pVB == nullptr || pData->iVertexBufferSize < pDrawData->iTotalVtxCount )
	{
		SafeRelease( pData->pVB );
		pData->iVertexBufferSize = pDrawData->iTotalVtxCount + 5000;
		if ( pData->pd3dDevice->CreateVertexBuffer( pData->iVertexBufferSize * sizeof( CUSTOMVERTEX ), D3DUSAGE_DYNAMIC | D3DUSAGE_WRITEONLY, D3DFVF_CUSTOMVERTEX, D3DPOOL_DEFAULT, &pData->pVB, NULL ) < 0 )
			return;
	}
	if ( pData->pIB == nullptr || pData->iIndexBufferSize < pDrawData->iTotalIdxCount )
	{
		SafeRelease( pData->pIB );
		pData->iIndexBufferSize = pDrawData->iTotalIdxCount + 10000;
		if ( pData->pd3dDevice->CreateIndexBuffer( pData->iIndexBufferSize * sizeof( DrawIdx_t ), D3DUSAGE_DYNAMIC | D3DUSAGE_WRITEONLY, sizeof( DrawIdx_t ) == 2 ? D3DFMT_INDEX16 : D3DFMT_INDEX32, D3DPOOL_DEFAULT, &pData->pIB, NULL ) < 0 )
			return;
	}

	// Backup the DX9 state
	// used D3DSBT_PIXELSTATE instead D3DSBT_ALL to fix game material render artifacts
	IDirect3DStateBlock9* d3d9_state_block = NULL;
	if ( pData->pd3dDevice->CreateStateBlock( D3DSBT_PIXELSTATE, &d3d9_state_block ) != D3D_OK )
		return;

	// @credits: T0b1
	if ( d3d9_state_block->Capture( ) != D3D_OK )
		return;

	// backup vertex states to compensate stateblock changes
	IDirect3DVertexDeclaration9* vertex_declaration;
	IDirect3DVertexShader9* vertex_shader;
	pData->pd3dDevice->GetVertexDeclaration( &vertex_declaration );
	pData->pd3dDevice->GetVertexShader( &vertex_shader );

	// Backup the DX9 transform (DX9 documentation suggests that it is included in the StateBlock but it doesn't appear to)
	D3DMATRIX last_world, last_view, last_projection;
	pData->pd3dDevice->GetTransform( D3DTS_WORLD, &last_world );
	pData->pd3dDevice->GetTransform( D3DTS_VIEW, &last_view );
	pData->pd3dDevice->GetTransform( D3DTS_PROJECTION, &last_projection );

	// Copy and convert all vertices into a single contiguous buffer, convert colors to DX9 default format.
	// FIXME-OPT: This is a waste of resource, the ideal is to use imconfig.h and
	//  1) to avoid repacking colors:   #define DRAW_USE_BGRA_PACKED_COLOR
	//  2) to avoid repacking vertices: #define DRAW_OVERRIDE_DRAWVERT_STRUCT_LAYOUT struct ImDrawVert { ImVec2 vecPostion; float z; ImU32 uCol; ImVec2 vecUV; }
	CUSTOMVERTEX* vtx_dst;
	DrawIdx_t* idx_dst;
	if ( pData->pVB->Lock( 0, ( UINT )( pDrawData->iTotalVtxCount * sizeof( CUSTOMVERTEX ) ), ( void** )&vtx_dst, D3DLOCK_DISCARD ) < 0 )
		return;
	if ( pData->pIB->Lock( 0, ( UINT )( pDrawData->iTotalIdxCount * sizeof( DrawIdx_t ) ), ( void** )&idx_dst, D3DLOCK_DISCARD ) < 0 )
		return;
	for ( int n = 0; n < pDrawData->iCmdListsCount; n++ )
	{
		const DrawList_t* cmd_list = pDrawData->ppCmdLists[ n ];
		const DrawVert_t* vtx_src = cmd_list->vecVtxBuffer.Data;
		for ( size_t i = 0; i < cmd_list->vecVtxBuffer.Size; i++ )
		{
			vtx_dst->vecPostion[ 0 ] = vtx_src->vecPostion.x;
			vtx_dst->vecPostion[ 1 ] = vtx_src->vecPostion.y;
			vtx_dst->vecPostion[ 2 ] = 0.0f;
			vtx_dst->uCol = ( vtx_src->uCol & 0xFF00FF00 ) | ( ( vtx_src->uCol & 0xFF0000 ) >> 16 ) | ( ( vtx_src->uCol & 0xFF ) << 16 ); // RGBA --> ARGB for DirectX9
			vtx_dst->vecUV[ 0 ] = vtx_src->vecUV.x;
			vtx_dst->vecUV[ 1 ] = vtx_src->vecUV.y;
			vtx_dst++;
			vtx_src++;
		}
		CRT::MemoryCopy( idx_dst, cmd_list->vecIdxBuffer.Data, cmd_list->vecIdxBuffer.Size * sizeof( DrawIdx_t ) );
		idx_dst += cmd_list->vecIdxBuffer.Size;
	}
	pData->pVB->Unlock( );
	pData->pIB->Unlock( );
	pData->pd3dDevice->SetStreamSource( 0, pData->pVB, 0, sizeof( CUSTOMVERTEX ) );
	pData->pd3dDevice->SetIndices( pData->pIB );
	pData->pd3dDevice->SetFVF( D3DFVF_CUSTOMVERTEX );

	// Setup desired DX state
	RenderState_t::Setup( pData, &drawData );

	// Render command lists
	// (Because we merged all buffers into a single one, we maintain our own offset into them)
	int global_vtx_offset = 0;
	int global_idx_offset = 0;
	Vector2D_t clip_off = pDrawData->vecDisplayPos;
	for ( int n = 0; n < pDrawData->iCmdListsCount; n++ )
	{
		const DrawList_t* cmd_list = pDrawData->ppCmdLists[ n ];
		for ( size_t cmd_i = 0; cmd_i < cmd_list->vecCmdBuffer.Size; cmd_i++ )
		{
			const DrawCmd_t* pcmd = &cmd_list->vecCmdBuffer[ cmd_i ];
			if ( pcmd->fnUserCallback != NULL )
			{
				// User fnCallback, registered via DrawList_t::AddCallback()
				// (DrawCallback_ResetRenderState is a special fnCallback value used by the user to request the renderer to reset render state.)
				if ( pcmd->fnUserCallback == DrawCallback_ResetRenderState )
					RenderState_t::Setup( pData, &drawData );
				else
					pcmd->fnUserCallback( cmd_list, pcmd );
			}
			else /*if (pcmd->nElemCount > 0)*/ // @note: unnecessary as we already did this in DrawDataBuilder_t::AddDrawList
			{
				const RECT r = { ( LONG )( pcmd->vecClipRect.x - clip_off.x ), ( LONG )( pcmd->vecClipRect.y - clip_off.y ), ( LONG )( pcmd->vecClipRect.z - clip_off.x ), ( LONG )( pcmd->vecClipRect.w - clip_off.y ) };
				pData->pd3dDevice->SetTexture( 0, static_cast< LPDIRECT3DTEXTURE9 >( pcmd->pTextureId ) );
				pData->pd3dDevice->SetScissorRect( &r );

				pData->pd3dDevice->DrawIndexedPrimitive( D3DPT_TRIANGLELIST, pcmd->nVtxOffset + global_vtx_offset, 0, ( UINT )cmd_list->vecVtxBuffer.Size, pcmd->nIdxOffset + global_idx_offset, pcmd->nElemCount / 3 );
			}
		}
		global_idx_offset += cmd_list->vecIdxBuffer.Size;
		global_vtx_offset += cmd_list->vecVtxBuffer.Size;
	}

	// Restore the DX9 transform
	pData->pd3dDevice->SetTransform( D3DTS_WORLD, &last_world );
	pData->pd3dDevice->SetTransform( D3DTS_VIEW, &last_view );
	pData->pd3dDevice->SetTransform( D3DTS_PROJECTION, &last_projection );

	// modified by qo0
	// Restore the DX9 vertex states
	pData->pd3dDevice->SetVertexDeclaration( vertex_declaration );
	pData->pd3dDevice->SetVertexShader( vertex_shader );

	// Restore the DX9 state
	d3d9_state_block->Apply( );
	d3d9_state_block->Release( );
}
#pragma once

#include "../draw.h"

struct ID3D11Device;
struct ID3D11DeviceContext;

class CDrawContextDX11 final : public CDrawContext
{
public:
	DRAW_IMPL_API CDrawContextDX11( ID3D11Device* pDevice, ID3D11DeviceContext* pDeviceContext );
	~CDrawContextDX11( );

	DRAW_IMPL_API void NewFrame( const DrawListFlags_t nDrawFlags = 0 ) override;
	DRAW_IMPL_API void Render( ) override;

	DRAW_IMPL_API bool CreateDeviceObjects( ) override;
	DRAW_IMPL_API void InvalidateDeviceObjects( ) override;

	DRAW_IMPL_API void* CreateVertexShader( const char* szSource ) override;
	DRAW_IMPL_API void DestroyVertexShader( void* pShader ) override;
	DRAW_IMPL_API void* CreatePixelShader( const char* szSource ) override;
	DRAW_IMPL_API void DestroyPixelShader( void* pShader ) override;
};
#pragma once

#include "../draw.h"

struct IDirect3DDevice9;

class CDrawContextDX9 final : public CDrawContext
{
public:
	DRAW_IMPL_API CDrawContextDX9( IDirect3DDevice9* device );
	~CDrawContextDX9( );

	DRAW_IMPL_API void NewFrame( const DrawListFlags_t nDrawFlags = 0 ) override;
	DRAW_IMPL_API void Render( ) override;

	DRAW_IMPL_API bool CreateDeviceObjects( ) override;
	DRAW_IMPL_API void InvalidateDeviceObjects( ) override;

	DRAW_IMPL_API void* CreateVertexShader( const char* szSource ) override;
	DRAW_IMPL_API void DestroyVertexShader( void* pShader ) override;
	DRAW_IMPL_API void* CreatePixelShader( const char* szSource ) override;
	DRAW_IMPL_API void DestroyPixelShader( void* pShader ) override;
};
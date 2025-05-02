#include <d3d11.h>
#include <tchar.h>
#include <memory>

#pragma comment(lib, "d3d11.lib")

#include "../shared/draw/draw.h"
#include "../shared/draw/impl/draw_impl_dx11.h"

#include "../shared/inputsystem/inputsystem.h"
#include "../shared/inputsystem/imp/inputsystem_impl_win32.h"

#include "../shared/utilities/logger.h"

#include "../shared/utilities/crt.h"
#include "../shared/utilities/assert.h"

// Data
static ID3D11Device* g_pd3dDevice = nullptr;
static ID3D11DeviceContext* g_pd3dDeviceContext = nullptr;
static IDXGISwapChain* g_pSwapChain = nullptr;
static UINT g_ResizeWidth = 0, g_ResizeHeight = 0;
static ID3D11RenderTargetView* g_mainRenderTargetView = nullptr;
static INT64 g_Time = 0;
static INT64 g_TicksPerSecond;

// Forward declarations of helper functions
bool CreateDeviceD3D( HWND hWnd );
void CleanupDeviceD3D( );
void CreateRenderTarget( );
void CleanupRenderTarget( );
LRESULT WINAPI WndProc( HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam );

// Main code
int main( int, char** )
{
	WNDCLASSEXW wc = { sizeof( wc ), CS_CLASSDC, WndProc, 0L, 0L, GetModuleHandle( nullptr ), nullptr, nullptr, nullptr, nullptr, L"sandbox-app", nullptr };
	::RegisterClassExW( &wc );
	HWND hwnd = ::CreateWindowW( wc.lpszClassName, L"sandbox", WS_OVERLAPPEDWINDOW, 100, 100, 1280, 800, nullptr, nullptr, wc.hInstance, nullptr );

	if ( g_Time == 0 )
		if ( !::QueryPerformanceFrequency( ( LARGE_INTEGER* )&g_Time ) )
			return 1;

	if ( g_TicksPerSecond == 0 )
		if ( !::QueryPerformanceFrequency( ( LARGE_INTEGER* )&g_TicksPerSecond ) )
			return 1;

	// Initialize Direct3D
	if ( !CreateDeviceD3D( hwnd ) )
	{
		CleanupDeviceD3D( );
		::UnregisterClassW( wc.lpszClassName, wc.hInstance );
		return 1;
	}

	// Show the window
	::ShowWindow( hwnd, SW_SHOWDEFAULT );
	::UpdateWindow( hwnd );

	std::unique_ptr<CDrawContextDX11> pDrawCtx = std::make_unique<CDrawContextDX11>( g_pd3dDevice, g_pd3dDeviceContext );
	g_pDrawContext = pDrawCtx.get( );

	std::unique_ptr<CInputSystemWin32> pInputSystem = std::make_unique<CInputSystemWin32>( );
	pInputSystem->AttachToWindow( hwnd );
	g_pInputSystem = pInputSystem.get( );

	// Our state
	bool show_demo_window = true;
	bool show_another_window = false;
	Vector4D_t clear_color = Vector4D_t( 0.45f, 0.55f, 0.60f, 1.00f );

	size_t nId = pDrawCtx->AddDrawList( 0 );
	auto draw = pDrawCtx->GetDrawList( nId );

	// Main loop
	bool done = false;
	while ( !done )
	{
		// Poll and handle messages (inputs, window resize, etc.)
		// See the WndProc() function below for our to dispatch events to the Win32 backend.
		MSG msg;
		while ( ::PeekMessage( &msg, nullptr, 0U, 0U, PM_REMOVE ) )
		{
			::TranslateMessage( &msg );
			::DispatchMessage( &msg );
			if ( msg.message == WM_QUIT )
				done = true;
		}
		if ( done )
			break;

		// Handle window resize (we don't resize directly in the WM_SIZE handler)
		if ( g_ResizeWidth != 0 && g_ResizeHeight != 0 )
		{
			CleanupRenderTarget( );
			g_pSwapChain->ResizeBuffers( 0, g_ResizeWidth, g_ResizeHeight, DXGI_FORMAT_UNKNOWN, 0 );
			g_ResizeWidth = g_ResizeHeight = 0;
			CreateRenderTarget( );
		}

		pInputSystem->Update( pDrawCtx->flDeltaTime );
		pDrawCtx->NewFrame( );

		// main draw code
		{
			draw->AddLine( Vector2D_t( 200, 200 ), Vector2D_t( 300, 200 ), DRAW_COL32_WHITE, 1.0f );
			draw->AddShadowRect( Vector2D_t( 400, 400 ), Vector2D_t( 600, 600 ), 320.f, Vector2D_t( ), pInputSystem->IsMouseHoveringRect( Vector2D_t( 400, 400 ), Vector2D_t( 600, 600 ) ) ? DRAW_COL32( 255, 255, 255, 255 ) : DRAW_COL32( 255, 255, 0, 255 ), 4.f, kDrawFlags_All );
			draw->AddRectFilledMultiColor( Vector2D_t( 400, 400 ), Vector2D_t( 600, 600 ), DRAW_COL32( 255, 0, 0, 255 ), DRAW_COL32( 0, 255, 0, 255 ), DRAW_COL32( 0, 0, 255, 255 ), DRAW_COL32( 255, 255, 255, 255 ), 8.0f );
			//draw->AddRect(Vector2D_t(400, 400), Vector2D_t(600, 600), DRAW_COL32_WHITE, 4.f, kDrawCornerFlags_All, 1.0f, kDrawRectFlags_Inline | kDrawRectFlags_Outline);

			draw->AddText( Vector2D_t( 200, 300 ), DRAW_COL32_WHITE, "HELLO WORLD" );
			draw->AddImage( pDrawCtx->pFontAtlas->pTexID, Vector2D_t( 20, 20 ), Vector2D_t( 20, 20 ) + Vector2D_t( pDrawCtx->pFontAtlas->iTexWidth, pDrawCtx->pFontAtlas->iTexHeight ) );

			char szFpsBuffer[ CRT::IntegerToString_t<int, 10U>::MaxCount( ) ] = {};
			const char* szFps = CRT::IntegerToString( static_cast< int >( M_ROUND( pDrawCtx->flFramerate ) ), szFpsBuffer, sizeof( szFpsBuffer ) );
			draw->AddText( Vector2D_t( 200, 350 ), DRAW_COL32_WHITE, szFps );

			static int i = 12;
			// pg_up
			if ( pInputSystem->IsKeyPressed( VK_PRIOR ) )
				i++;
			// pg_down
			if ( pInputSystem->IsKeyPressed( VK_NEXT ) )
				i--;

			i = CRT::Clamp( i, -1, 360 );

			char a[ CRT::IntegerToString_t<int, 10U>::MaxCount( ) ] = {};
			const char* b = CRT::IntegerToString( i, a, sizeof( a ) );
			draw->AddText( Vector2D_t( 200, 400 ), DRAW_COL32_WHITE, b );

			draw->AddTriangle( Vector2D_t( 300, 150 ), Vector2D_t( 300, 200 ), Vector2D_t( 250, 300 ), DRAW_COL32_WHITE, 1.0f, kDrawFlags_FillConvex );
			draw->AddCircleMultiColor( Vector2D_t( 800, 600 ), 64.f, DRAW_COL32( 255, 255, 255, 155 ), DRAW_COL32_BLACK, i );

			pDrawCtx->foregroundDrawList.AddCircle( Vector2D_t( 500, 500 ), 64.f * 2, DRAW_COL32( 255, 0, 0, 255 ), i );
		}

		const float clear_color_with_alpha[ 4 ] = { clear_color.x * clear_color.w, clear_color.y * clear_color.w, clear_color.z * clear_color.w, clear_color.w };
		g_pd3dDeviceContext->OMSetRenderTargets( 1, &g_mainRenderTargetView, nullptr );
		g_pd3dDeviceContext->ClearRenderTargetView( g_mainRenderTargetView, clear_color_with_alpha );

		if ( pInputSystem->IsKeyPressed( VK_F1 ) )
			pDrawCtx->drawData.bDebugWireframe = !pDrawCtx->drawData.bDebugWireframe;

		pDrawCtx->Render( );
		g_pSwapChain->Present( 1U, 0 ); // Present with vsync
	}

	g_pDrawContext = nullptr;
	g_pInputSystem = nullptr;

	CleanupDeviceD3D( );
	::DestroyWindow( hwnd );
	::UnregisterClassW( wc.lpszClassName, wc.hInstance );

	return 0;
}

// Helper functions

bool CreateDeviceD3D( HWND hWnd )
{
	// Setup swap chain
	DXGI_SWAP_CHAIN_DESC sd;
	ZeroMemory( &sd, sizeof( sd ) );
	sd.BufferCount = 2;
	sd.BufferDesc.Width = 0;
	sd.BufferDesc.Height = 0;
	sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	sd.BufferDesc.RefreshRate.Numerator = 60;
	sd.BufferDesc.RefreshRate.Denominator = 1;
	sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
	sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	sd.OutputWindow = hWnd;
	sd.SampleDesc.Count = 1;
	sd.SampleDesc.Quality = 0;
	sd.Windowed = TRUE;
	sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

	UINT createDeviceFlags = 0;
	//createDeviceFlags |= D3D11_CREATE_DEVICE_DEBUG;
	D3D_FEATURE_LEVEL featureLevel;
	const D3D_FEATURE_LEVEL featureLevelArray[ 2 ] = {
		D3D_FEATURE_LEVEL_11_0,
		D3D_FEATURE_LEVEL_10_0,
	};
	HRESULT res = D3D11CreateDeviceAndSwapChain( nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, createDeviceFlags, featureLevelArray, 2, D3D11_SDK_VERSION, &sd, &g_pSwapChain, &g_pd3dDevice, &featureLevel, &g_pd3dDeviceContext );
	if ( res == DXGI_ERROR_UNSUPPORTED ) // Try high-performance WARP software driver if hardware is not available.
		res = D3D11CreateDeviceAndSwapChain( nullptr, D3D_DRIVER_TYPE_WARP, nullptr, createDeviceFlags, featureLevelArray, 2, D3D11_SDK_VERSION, &sd, &g_pSwapChain, &g_pd3dDevice, &featureLevel, &g_pd3dDeviceContext );
	if ( res != S_OK )
		return false;

	CreateRenderTarget( );
	return true;
}

void CleanupDeviceD3D( )
{
	CleanupRenderTarget( );
	if ( g_pSwapChain )
	{
		g_pSwapChain->Release( );
		g_pSwapChain = nullptr;
	}
	if ( g_pd3dDeviceContext )
	{
		g_pd3dDeviceContext->Release( );
		g_pd3dDeviceContext = nullptr;
	}
	if ( g_pd3dDevice )
	{
		g_pd3dDevice->Release( );
		g_pd3dDevice = nullptr;
	}
}

void CreateRenderTarget( )
{
	ID3D11Texture2D* pBackBuffer;
	g_pSwapChain->GetBuffer( 0, IID_PPV_ARGS( &pBackBuffer ) );
	g_pd3dDevice->CreateRenderTargetView( pBackBuffer, nullptr, &g_mainRenderTargetView );
	pBackBuffer->Release( );
}

void CleanupRenderTarget( )
{
	if ( g_mainRenderTargetView )
	{
		g_mainRenderTargetView->Release( );
		g_mainRenderTargetView = nullptr;
	}
}

LRESULT WINAPI WndProc( HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam )
{
	if ( g_pInputSystem == nullptr )
		return ::DefWindowProcW( hWnd, uMsg, wParam, lParam );

	CInputSystemWin32* pInputSystem = static_cast< CInputSystemWin32* >( g_pInputSystem );
	pInputSystem->OnWndProc( uMsg, wParam, lParam );

	switch ( uMsg )
	{
		case WM_SIZE:
			if ( wParam == SIZE_MINIMIZED )
				return 0;
			g_ResizeWidth = ( UINT )LOWORD( lParam ); // Queue resize
			g_ResizeHeight = ( UINT )HIWORD( lParam );
			return 0;
		case WM_SYSCOMMAND:
			if ( ( wParam & 0xfff0 ) == SC_KEYMENU ) // Disable ALT application menu
				return 0;
			break;
		case WM_DESTROY:
			::PostQuitMessage( 0 );
			return 0;
	}
	return ::DefWindowProcW( hWnd, uMsg, wParam, lParam );
}
#include "inputsystem_impl_win32.h"

#include "../../draw/draw.h"

static LRESULT WndProc( HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam );

CInputSystemWin32::CInputSystemWin32( ) :
	CBaseInputSystem::CBaseInputSystem( ),
	hWindow( nullptr ),
	nTime( 0 ),
	nTicksPerSecond( 0 )
{ }

CInputSystemWin32::~CInputSystemWin32( )
{
	DetachFromWindow( );
}

bool CInputSystemWin32::AttachToWindow( HWND hWnd )
{
	if ( hWindow != nullptr )
		CRT_ASSERTION( false && "input system already attached to a window" );

	// save it for maybe later use
	hWindow = hWnd;

	if ( nTime == 0 )
		if ( !::QueryPerformanceFrequency( ( LARGE_INTEGER* )&nTime ) )
			return false;

	if ( nTicksPerSecond == 0 )
		if ( !::QueryPerformanceFrequency( ( LARGE_INTEGER* )&nTicksPerSecond ) )
			return false;

	pBackendPlatformUserData = hWindow;

	return true;
}

void CInputSystemWin32::DetachFromWindow( )
{
	if ( hWindow == nullptr )
		CRT_ASSERTION( false && "Input system not attached to a window" );

	hWindow = nullptr;
}

LRESULT CInputSystemWin32::OnWndProc( UINT uMsg, WPARAM wParam, LPARAM lParam )
{
	switch ( uMsg )
	{
		case WM_LBUTTONDOWN:
		case WM_LBUTTONDBLCLK:
		case WM_RBUTTONDOWN:
		case WM_RBUTTONDBLCLK:
		case WM_MBUTTONDOWN:
		case WM_MBUTTONDBLCLK:
		case WM_XBUTTONDOWN:
		case WM_XBUTTONDBLCLK:
		{
			int button = 0;
			if ( uMsg == WM_LBUTTONDOWN || uMsg == WM_LBUTTONDBLCLK )
			{
				button = 0;
			}
			if ( uMsg == WM_RBUTTONDOWN || uMsg == WM_RBUTTONDBLCLK )
			{
				button = 1;
			}
			if ( uMsg == WM_MBUTTONDOWN || uMsg == WM_MBUTTONDBLCLK )
			{
				button = 2;
			}
			if ( uMsg == WM_XBUTTONDOWN || uMsg == WM_XBUTTONDBLCLK )
			{
				button = ( GET_XBUTTON_WPARAM( wParam ) == XBUTTON1 ) ? 3 : 4;
			}
			if ( !IsAnyMouseDown( ) && ::GetCapture( ) == NULL )
				::SetCapture( hWindow );
			arrMouseDown[ button ] = true;
			return 0;
		}
		case WM_LBUTTONUP:
		case WM_RBUTTONUP:
		case WM_MBUTTONUP:
		case WM_XBUTTONUP:
		{
			int button = 0;
			if ( uMsg == WM_LBUTTONUP )
			{
				button = 0;
			}
			if ( uMsg == WM_RBUTTONUP )
			{
				button = 1;
			}
			if ( uMsg == WM_MBUTTONUP )
			{
				button = 2;
			}
			if ( uMsg == WM_XBUTTONUP )
			{
				button = ( GET_XBUTTON_WPARAM( wParam ) == XBUTTON1 ) ? 3 : 4;
			}
			arrMouseDown[ button ] = false;
			if ( !IsAnyMouseDown( ) && ::GetCapture( ) == hWindow )
				::ReleaseCapture( );
			return 0;
		}
		case WM_MOUSEWHEEL:
			flMouseWheel += ( float )GET_WHEEL_DELTA_WPARAM( wParam ) / ( float )WHEEL_DELTA;
			return 0;
		case WM_MOUSEHWHEEL:
			flMouseWheelH += ( float )GET_WHEEL_DELTA_WPARAM( wParam ) / ( float )WHEEL_DELTA;
			return 0;
		case WM_KEYDOWN:
		case WM_SYSKEYDOWN:
			if ( wParam < 256 )
				arrKeysDown[ wParam ] = 1;
			return 0;
		case WM_KEYUP:
		case WM_SYSKEYUP:
			if ( wParam < 256 )
				arrKeysDown[ wParam ] = 0;
			return 0;
			//case WM_CHAR:
			//	// You can also use ToAscii()+GetKeyboardState() to retrieve characters.
			//	AddInputCharacter((unsigned int)wParam);
			//	return 0;
			//case WM_SETCURSOR:
			//	if (LOWORD(lParam) == HTCLIENT && ImGui_ImplWin32_UpdateMouseCursor())
			//		return 1;
			//	return 0;
			//case WM_DEVICECHANGE:
			//	if ((UINT)wParam == DBT_DEVNODES_CHANGED)
			//		g_WantUpdateHasGamepad = true;
			//	return 0;
	}
	//L_PRINT(LOG_INFO) << "uMsg: " << uMsg << " wParam: " << wParam << " lParam: " << lParam;

	return 0;
}

void CInputSystemWin32::Update( const float flDeltaTime )
{
	if ( g_pDrawContext == nullptr )
		return;

	RECT rect;
	::GetClientRect( hWindow, &rect );
	g_pDrawContext->SetDisplaySize( Vector2D_t( ( float )( rect.right - rect.left ), ( float )( rect.bottom - rect.top ) ) );

	INT64 nCurrentTime = 0;
	if ( !::QueryPerformanceCounter( ( LARGE_INTEGER* )&nCurrentTime ) )
		return;
	g_pDrawContext->SetDeltaTime( static_cast< float >( nCurrentTime - nTime ) / static_cast< float >( nTicksPerSecond ) );
	nTime = nCurrentTime;

	bKeyControl = ( ::GetKeyState( VK_CONTROL ) & 0x8000 ) != 0;
	bKeyShift = ( ::GetKeyState( VK_SHIFT ) & 0x8000 ) != 0;
	bKeyAlt = ( ::GetKeyState( VK_MENU ) & 0x8000 ) != 0;
	bKeySuper = false;

	// update mouse pos
	{
		vecMousePos = Vector2D_t{ -FLT_MAX, -FLT_MAX };
		POINT pt;
		if ( HWND hActive = ::GetActiveWindow( ) )
			if ( hActive == hWindow || ::IsChild( hActive, hWindow ) )
			{
				if ( ::GetCursorPos( &pt ) && ::ScreenToClient( hWindow, &pt ) )
					vecMousePos = Vector2D_t{ static_cast< float >( pt.x ), static_cast< float >( pt.y ) };
			}
	}

	CBaseInputSystem::Update( flDeltaTime );
}
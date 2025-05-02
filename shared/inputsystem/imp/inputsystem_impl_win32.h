#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>

#include "../inputsystem.h"

class CInputSystemWin32 final : public CBaseInputSystem
{
public:
	CInputSystemWin32( );
	~CInputSystemWin32( );

	bool AttachToWindow( HWND hWnd );
	void DetachFromWindow( );

	LRESULT OnWndProc( UINT uMsg, WPARAM wParam, LPARAM lParam );
	void Update( const float flDeltaTime ) override;
private:
	HWND hWindow = nullptr;
	WNDPROC pfnOldWndProc = nullptr;

	INT64 nTime = 0;
	INT64 nTicksPerSecond = 0;
};
#pragma once

#include "../datatypes/vector.h"

class CBaseInputSystem
{
	enum
	{
		kMaxMouseButtons = 5,
		kMaxKeys = 512
	};

public:
	CBaseInputSystem( );
	virtual ~CBaseInputSystem( ) { };

	// we need to update the input system every frame
	virtual void Update( const float flDeltaTime );

	int CalcTypematicRepeatAmount( float flPreviousTime, float flCurrentTime, float flRepeatDelay, float flRepeatRate ) const;
	int GetKeyPressedAmount( int nKey, float flRepeatDelay, float flRate ) const;

	bool IsKeyDown( int nKey ) const;
	bool IsKeyPressed( int nKey, bool bRepeat = false ) const;
	bool IsKeyReleased( int nKey ) const;

	bool IsMouseDown( int nButton ) const;
	bool IsMouseClicked( int nButton, bool bRepeat = false ) const;
	bool IsMouseReleased( int nButton ) const;
	bool IsMouseDoubleClicked( int nButton ) const;
	bool IsMouseHoveringRect( const Vector2D_t& vecRectMin, const Vector2D_t& vecRectMax ) const;
	bool IsMousePosValid( const Vector2D_t* pvecPos = nullptr ) const;
	bool IsAnyMouseDown( );
	Vector2D_t GetMousePos( ) const;

	bool IsMouseDragPastThreshold( int nButton, float flLockThreshold = -1.0f ) const;
	bool IsMouseDragging( int nButton, float flLockThreshold = -1.0f ) const;
	Vector2D_t GetMouseDragDelta( int nButton, float flLockThreshold = -1.0f ) const;
	void ResetMouseDragDelta( int nButton );

	void* GetBackendPlatformUserData( ) const
	{
		return pBackendPlatformUserData;
	}

private:
	void UpdateMouseInputs( );
	void UpdateMouseWheel( );

protected:
	//bool bMouseDrawCursor;
	void* pBackendPlatformUserData = nullptr;

	Vector2D_t vecMousePos = {};
	bool arrMouseDown[ kMaxMouseButtons ] = {};
	float flMouseWheel = 0.0f;
	float flMouseWheelH = 0.0f;
	bool bKeyControl = false;
	bool bKeyShift = false;
	bool bKeyAlt = false;
	bool bKeySuper = false;
	bool arrKeysDown[ kMaxKeys ] = {};

	//bool bWantCaptureMouse;
	//bool bWantCaptureKeyboard;

	Vector2D_t vecMousePosPrev = {};
	Vector2D_t arrMouseClickedPos[ kMaxMouseButtons ] = {};
	double arrMouseClickedTime[ kMaxMouseButtons ] = {};
	bool arrMouseClicked[ kMaxMouseButtons ] = {};
	bool arrMouseDoubleClicked[ kMaxMouseButtons ] = {};

	bool arrMouseReleased[ kMaxMouseButtons ] = {};
	bool arrMouseDownOwned[ kMaxMouseButtons ] = {};
	bool arrMouseDownWasDoubleClick[ kMaxMouseButtons ] = {};
	float arrMouseDownDuration[ kMaxMouseButtons ] = { -1.0f };
	float arrMouseDownDurationPrev[ kMaxMouseButtons ] = { -1.0f };
	Vector2D_t arrMouseDragMaxDistanceAbs[ kMaxMouseButtons ] = {};
	float arrMouseDragMaxDistanceSqr[ kMaxMouseButtons ] = { -1.0f };
	float arrKeysDownDuration[ kMaxKeys ] = { -1.0f };
	float arrKeysDownDurationPrev[ kMaxKeys ] = { -1.0f };

	Vector2D_t vecMouseDelta;
};

inline CBaseInputSystem* g_pInputSystem = nullptr;
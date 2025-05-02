#include "inputsystem.h"

#include "../utilities/crt.h"

#include "../draw/draw.h"

CBaseInputSystem::CBaseInputSystem( )
{
	vecMousePos = Vector2D_t( -FLT_MAX, -FLT_MAX );
	vecMousePosPrev = Vector2D_t( -FLT_MAX, -FLT_MAX );

	for ( int i = 0; i < CRT_ARRAYSIZE( arrMouseDownDuration ); i++ )
		arrMouseDownDuration[ i ] = arrMouseDownDurationPrev[ i ] = -1.0f;
	for ( int i = 0; i < CRT_ARRAYSIZE( arrKeysDownDuration ); i++ )
		arrKeysDownDuration[ i ] = arrKeysDownDurationPrev[ i ] = -1.0f;
}

void CBaseInputSystem::Update( const float flDeltaTime )
{
	CRT::MemoryCopy( arrKeysDownDurationPrev, arrKeysDownDuration, sizeof( arrKeysDownDuration ) );
	for ( int i = 0; i < CRT_ARRAYSIZE( arrKeysDown ); i++ )
		arrKeysDownDuration[ i ] = arrKeysDown[ i ] ? ( arrKeysDownDuration[ i ] < 0.0f ? 0.0f : arrKeysDownDuration[ i ] + flDeltaTime ) : -1.0f;

	UpdateMouseInputs( );
}

bool CBaseInputSystem::IsKeyDown( int nKey ) const
{
	if ( nKey <= 0 )
		return false;

	CRT_ASSERTION( nKey >= 0 && nKey < kMaxKeys );
	return arrKeysDown[ nKey ];
}

bool CBaseInputSystem::IsKeyPressed( int nKey, bool bRepeat ) const
{
	if ( nKey <= 0 )
		return false;

	CRT_ASSERTION( nKey >= 0 && nKey < kMaxKeys );
	const float t = arrKeysDownDuration[ nKey ];
	if ( std::fpclassify( t ) == FP_ZERO )
		return true;

	if ( bRepeat && t > 0.250f )
		return GetKeyPressedAmount( nKey, 0.250f, 0.050f ) > 0;

	return false;
}

bool CBaseInputSystem::IsKeyReleased( int nKey ) const
{
	if ( nKey <= 0 )
		return false;

	CRT_ASSERTION( nKey >= 0 && nKey < kMaxKeys );
	return arrKeysDownDurationPrev[ nKey ] >= 0.0f && !arrKeysDown[ nKey ];
}

int CBaseInputSystem::CalcTypematicRepeatAmount( float flPreviousTime, float flCurrentTime, float flRepeatDelay, float flRepeatRate ) const
{
	if ( std::fpclassify( flPreviousTime ) == FP_ZERO )
		return 1;
	if ( flPreviousTime > flCurrentTime )
		return 0;
	if ( std::fpclassify( flRepeatRate ) == FP_ZERO )
		return ( flPreviousTime < flRepeatDelay ) && ( flCurrentTime >= flRepeatDelay );

	const int count_t0 = ( flPreviousTime < flRepeatDelay ) ? -1 : ( int )( ( flPreviousTime - flRepeatDelay ) / flRepeatRate );
	const int count_t1 = ( flCurrentTime < flRepeatDelay ) ? -1 : ( int )( ( flCurrentTime - flRepeatDelay ) / flRepeatRate );
	const int count = count_t1 - count_t0;
	return count;
}

int CBaseInputSystem::GetKeyPressedAmount( int nKey, float flRepeatDelay, float flRate ) const
{
	if ( g_pDrawContext == nullptr )
		return 0;

	if ( nKey <= 0 )
		return 0;

	CRT_ASSERTION( nKey >= 0 && nKey < kMaxKeys );
	const float t = arrKeysDownDuration[ nKey ];
	return CalcTypematicRepeatAmount( t - g_pDrawContext->flDeltaTime, t, flRepeatDelay, flRate );
}

bool CBaseInputSystem::IsMouseDown( int nButton ) const
{
	if ( nButton <= 0 )
		return false;

	CRT_ASSERTION( nButton >= 0 && nButton < kMaxMouseButtons );
	return arrMouseDown[ nButton ];
}

bool CBaseInputSystem::IsMouseClicked( int nButton, bool bRepeat ) const
{
	if ( g_pDrawContext == nullptr )
		return false;

	if ( nButton <= 0 )
		return false;

	const float t = arrMouseDownDuration[ nButton ];
	if ( std::fpclassify( t ) == FP_ZERO )
		return true;

	if ( bRepeat && t > 0.250f )
	{
		int iAmount = CalcTypematicRepeatAmount( t - g_pDrawContext->flDeltaTime, t, 0.250f, 0.050f * 0.50f );
		return iAmount > 0;
	}

	return false;
}

bool CBaseInputSystem::IsMouseReleased( int nButton ) const
{
	if ( nButton <= 0 )
		return false;

	CRT_ASSERTION( nButton >= 0 && nButton < kMaxMouseButtons );
	return arrMouseReleased[ nButton ];
}

bool CBaseInputSystem::IsMouseDoubleClicked( int nButton ) const
{
	if ( nButton <= 0 )
		return false;

	CRT_ASSERTION( nButton >= 0 && nButton < kMaxMouseButtons );
	return arrMouseDoubleClicked[ nButton ];
}

bool CBaseInputSystem::IsMouseHoveringRect( const Vector2D_t& vecRectMin, const Vector2D_t& vecRectMax ) const
{
	return vecMousePos.x >= vecRectMin.x && vecMousePos.x <= vecRectMax.x && vecMousePos.y >= vecRectMin.y && vecMousePos.y <= vecRectMax.y;
}

bool CBaseInputSystem::IsMousePosValid( const Vector2D_t* pvecPos ) const
{
	const float MOUSE_INVALID = -256000.0f;
	Vector2D_t p = pvecPos != nullptr ? *pvecPos : GetMousePos( );
	return p.x >= MOUSE_INVALID && p.y >= MOUSE_INVALID;
}

bool CBaseInputSystem::IsAnyMouseDown( )
{
	for ( int n = 0; n < CRT_ARRAYSIZE( arrMouseDown ); n++ )
		if ( arrMouseDown[ n ] )
			return true;
	return false;
}

Vector2D_t CBaseInputSystem::GetMousePos( ) const
{
	return vecMousePos;
}

bool CBaseInputSystem::IsMouseDragPastThreshold( int nButton, float flLockThreshold ) const
{
	if ( nButton <= 0 )
		return false;

	CRT_ASSERTION( nButton >= 0 && nButton < kMaxMouseButtons );
	if ( flLockThreshold < 0.0f )
		flLockThreshold = 6.0f;
	return arrMouseDragMaxDistanceSqr[ nButton ] >= flLockThreshold * flLockThreshold;
}

bool CBaseInputSystem::IsMouseDragging( int nButton, float flLockThreshold ) const
{
	if ( nButton <= 0 )
		return false;

	CRT_ASSERTION( nButton >= 0 && nButton < kMaxMouseButtons );
	if ( !arrMouseDown[ nButton ] )
		return false;

	return IsMouseDragPastThreshold( nButton, flLockThreshold );
}

Vector2D_t CBaseInputSystem::GetMouseDragDelta( int nButton, float flLockThreshold ) const
{
	CRT_ASSERTION( nButton >= 0 && nButton < kMaxMouseButtons );
	if ( flLockThreshold < 0.0f )
		flLockThreshold = 6.0f;

	if ( arrMouseDown[ nButton ] || arrMouseReleased[ nButton ] )
		if ( arrMouseDragMaxDistanceSqr[ nButton ] >= flLockThreshold * flLockThreshold )
			if ( IsMousePosValid( &vecMousePos ) && IsMousePosValid( &arrMouseClickedPos[ nButton ] ) )
				return vecMousePos - arrMouseClickedPos[ nButton ];

	return Vector2D_t( 0.0f, 0.0f );
}

void CBaseInputSystem::ResetMouseDragDelta( int nButton )
{
	CRT_ASSERTION( nButton >= 0 && nButton < kMaxMouseButtons );
	arrMouseClickedPos[ nButton ] = vecMousePos;
}

void CBaseInputSystem::UpdateMouseInputs( )
{
	if ( g_pDrawContext == nullptr )
		return;

	if ( IsMousePosValid( &vecMousePos ) )
		vecMousePos = VectorFloor( vecMousePos );

	// If mouse just appeared or disappeared (usually denoted by -FLT_MAX components) we cancel out movement in MouseDelta
	if ( IsMousePosValid( &vecMousePos ) && IsMousePosValid( &vecMousePosPrev ) )
		vecMouseDelta = vecMousePos - vecMousePosPrev;
	else
		vecMouseDelta = Vector2D_t( 0.0f, 0.0f );

	vecMousePosPrev = vecMousePos;
	for ( int i = 0; i < CRT_ARRAYSIZE( arrMouseDown ); i++ )
	{
		arrMouseClicked[ i ] = arrMouseDown[ i ] && arrMouseDownDuration[ i ] < 0.0f;
		arrMouseReleased[ i ] = !arrMouseDown[ i ] && arrMouseDownDuration[ i ] >= 0.0f;
		arrMouseDownDurationPrev[ i ] = arrMouseDownDuration[ i ];
		arrMouseDownDuration[ i ] = arrMouseDown[ i ] ? ( arrMouseDownDuration[ i ] < 0.0f ? 0.0f : arrMouseDownDuration[ i ] + g_pDrawContext->flDeltaTime ) : -1.0f;
		arrMouseDoubleClicked[ i ] = false;
		if ( arrMouseClicked[ i ] )
		{
			if ( ( float )( g_pDrawContext->dbTime - arrMouseClickedTime[ i ] ) < 0.30f )
			{
				Vector2D_t delta_from_click_pos = IsMousePosValid( &vecMousePos ) ? ( vecMousePos - arrMouseClickedPos[ i ] ) : Vector2D_t( 0.0f, 0.0f );
				if ( VectorLengthSqr( delta_from_click_pos ) < 6.0f * 6.0f )
					arrMouseDoubleClicked[ i ] = true;
				arrMouseClickedTime[ i ] = -FLT_MAX; // so the third click isn't turned into a double-click
			}
			else
			{
				arrMouseClickedTime[ i ] = g_pDrawContext->dbTime;
			}
			arrMouseClickedPos[ i ] = vecMousePos;
			arrMouseDownWasDoubleClick[ i ] = arrMouseDoubleClicked[ i ];
			arrMouseDragMaxDistanceAbs[ i ] = Vector2D_t( 0.0f, 0.0f );
			arrMouseDragMaxDistanceSqr[ i ] = 0.0f;
		}
		else if ( arrMouseDown[ i ] )
		{
			// Maintain the maximum distance we reaching from the initial click position, which is used with dragging threshold
			Vector2D_t delta_from_click_pos = IsMousePosValid( &vecMousePos ) ? ( vecMousePos - arrMouseClickedPos[ i ] ) : Vector2D_t( 0.0f, 0.0f );
			arrMouseDragMaxDistanceSqr[ i ] = CRT::Max( arrMouseDragMaxDistanceSqr[ i ], VectorLengthSqr( delta_from_click_pos ) );
			arrMouseDragMaxDistanceAbs[ i ].x = CRT::Max( arrMouseDragMaxDistanceAbs[ i ].x, delta_from_click_pos.x < 0.0f ? -delta_from_click_pos.x : delta_from_click_pos.x );
			arrMouseDragMaxDistanceAbs[ i ].y = CRT::Max( arrMouseDragMaxDistanceAbs[ i ].y, delta_from_click_pos.y < 0.0f ? -delta_from_click_pos.y : delta_from_click_pos.y );
		}
		if ( !arrMouseDown[ i ] && !arrMouseReleased[ i ] )
			arrMouseDownWasDoubleClick[ i ] = false;
	}
}

void CBaseInputSystem::UpdateMouseWheel( )
{
	if ( g_pDrawContext == nullptr )
		return;

	if ( flMouseWheel == 0.0f && flMouseWheelH == 0.0f )
		return;

	// nothing to do with this now
}

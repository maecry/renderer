#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include "draw.h"

// used: memoryset
#include "../utilities/crt.h"
// used: _pi
#include "../utilities/math.h"
// used: heapalloc, heapfree
#include "../utilities/memory.h"

#pragma region draw_main

CDrawContext::CDrawContext( ) :
	backgroundDrawList( &drawListSharedData ), foregroundDrawList( &drawListSharedData )
{
	bInitialized = true;
	pFont = NULL;
	flFontSize = FontBaseSize = 0.0f;

	pFontAtlas = MEM_NEW( FontAtlas_t )( );
	vecDisplaySize = Vector2D_t( -1.0f, -1.0f );
	flDeltaTime = 1.0f / 60.0f;
	vecDisplayFramebufferScale = Vector2D_t( 1.0f, 1.0f );

	pBackendRendererUserData = NULL;

	dbTime = 0.0;
	iFrameCount = 0;
	iFrameCountEnded = 0;
	iFrameCountRendered = 0;
	bWithinFrameScope = false;

	CRT::MemorySet( arrFramerateSecPerFrame, 0, sizeof( arrFramerateSecPerFrame ) );
	iFramerateSecPerFrameIdx = iFramerateSecPerFrameCount = 0;
	flFramerateSecPerFrameAccum = 0.0f;
}

CDrawContext::~CDrawContext( )
{
	if ( pFontAtlas != nullptr )
	{
		pFontAtlas->bLocked = false;
		MEM_DELETE( pFontAtlas );
	}
	pFontAtlas = NULL;

	if ( !bInitialized )
		return;

	drawDataBuilder.ClearFreeMemory( );
	backgroundDrawList.ClearFreeMemory( );
	foregroundDrawList.ClearFreeMemory( );

	for ( PiorityDrawList_t& draw : vecPiorityDrawList )
	{
		draw.drawList.ClearFreeMemory( );
	}

	bInitialized = false;
}


void CDrawContext::SortPiorityDrawList( )
{
	CRT_ASSERTION( bInitialized );

	struct SortPiorityDrawList_t
	{
		static int __cdecl Run( const void* lhs, const void* rhs )
		{
			const PiorityDrawList_t* pDrawListLhs = *( PiorityDrawList_t** )lhs;
			const PiorityDrawList_t* pDrawListRhs = *( PiorityDrawList_t** )rhs;
			return pDrawListLhs->nPiority < pDrawListRhs->nPiority;
		}

	};

	vecPiorityDrawList.sort( &SortPiorityDrawList_t::Run );
}

void CDrawContext::UpdateForNewFrame( const DrawListFlags_t nDrawFlags )
{
	CRT_ASSERTION( ( flDeltaTime > 0.0f || iFrameCount == 0 ) && "Need a positive flDeltaTime!" );
	CRT_ASSERTION( ( iFrameCount == 0 || iFrameCountEnded == iFrameCount ) && "Forgot to call Render() or EndFrame() at the end of the previous frame?" );
	CRT_ASSERTION( vecDisplaySize.x >= 0.0f && vecDisplaySize.y >= 0.0f && "Invalid vecDisplaySize value!" );
	CRT_ASSERTION( pFontAtlas->vecFonts.Size > 0 && "pFont Atlas not built. Did you call io.pFontAtlas->GetTexDataAsRGBA32() / GetTexDataAsAlpha8() ?" );
	CRT_ASSERTION( pFontAtlas->vecFonts[ 0 ]->IsLoaded( ) && "pFont Atlas not built. Did you call io.pFontAtlas->GetTexDataAsRGBA32() / GetTexDataAsAlpha8() ?" );

	dbTime += flDeltaTime;
	bWithinFrameScope = true;
	iFrameCount += 1;

	pFontAtlas->bLocked = true;
	SetCurrentFont( pFontAtlas->vecFonts[ 0 ] );

	drawListSharedData.vecClipRectFullscreen = Vector4D_t( 0.0f, 0.0f, vecDisplaySize.x, vecDisplaySize.y );
	drawListSharedData.flCurveTessellationTol = 1.25f;
	drawListSharedData.SetCircleSegmentMaxError( 1.60f );
	drawListSharedData.nInitialFlags = nDrawFlags != 0 ? nDrawFlags : kDrawListFlags_AntiAliasedLines | kDrawListFlags_AntiAliasedFill;

	backgroundDrawList.Clear( );
	backgroundDrawList.PushTextureID( pFontAtlas->pTexID );
	backgroundDrawList.PushClipRectFullScreen( );

	if ( !vecPiorityDrawList.empty( ) )
	{
		SortPiorityDrawList( );
		for ( PiorityDrawList_t& draw : vecPiorityDrawList )
		{
			draw.drawList.Clear( );
			draw.drawList.PushTextureID( pFontAtlas->pTexID );
			draw.drawList.PushClipRectFullScreen( );
		}
	}

	foregroundDrawList.Clear( );
	foregroundDrawList.PushTextureID( pFontAtlas->pTexID );
	foregroundDrawList.PushClipRectFullScreen( );

	drawData.Clear( );

	flFramerateSecPerFrameAccum += flDeltaTime - arrFramerateSecPerFrame[ iFramerateSecPerFrameIdx ];
	arrFramerateSecPerFrame[ iFramerateSecPerFrameIdx ] = flDeltaTime;
	iFramerateSecPerFrameIdx = ( iFramerateSecPerFrameIdx + 1 ) % CRT_ARRAYSIZE( arrFramerateSecPerFrame );
	iFramerateSecPerFrameCount = CRT::Min( ( size_t )iFramerateSecPerFrameCount + 1, CRT_ARRAYSIZE( arrFramerateSecPerFrame ) );
	flFramerate = ( flFramerateSecPerFrameAccum > 0.0f ) ? ( 1.0f / ( flFramerateSecPerFrameAccum / ( float )iFramerateSecPerFrameCount ) ) : FLT_MAX;
}

void CDrawContext::EndFrame( )
{
	CRT_ASSERTION( bInitialized );

	if ( iFrameCountEnded == iFrameCount ) // Don't process EndFrame() multiple times.
		return;

	bWithinFrameScope = false;
	iFrameCountEnded = iFrameCount;
}

void CDrawContext::SetCurrentFont( Font_t* pFont )
{
	if ( pFont == nullptr || !pFont->IsLoaded( ) )
		return;

	CRT_ASSERTION( pFont->flScale > 0.0f );
	pFont = pFont;
	FontBaseSize = CRT::Max( 1.0f, flFontGlobalScale * pFont->flFontSize * pFont->flScale );
	flFontSize = pFont->flFontSize;

	auto* atlas = pFont->pContainerAtlas;
	drawListSharedData.vecTexUvWhitePixel = atlas->vecTexUvWhitePixel;
	drawListSharedData.pFont = pFont;
	drawListSharedData.flFontSize = flFontSize;

	drawListSharedData.ShadowRectId = atlas->iShadowRectId;
	drawListSharedData.ShadowRectUvs = &atlas->ShadowRectUvs[ 0 ];
}

DrawData_t* CDrawContext::BuildDrawData( )
{
	CRT_ASSERTION( bInitialized );

	if ( iFrameCountEnded != iFrameCount )
		EndFrame( );

	iFrameCountRendered = iFrameCount;
	drawDataBuilder.Clear( );

	// add background DrawList_t
	if ( !backgroundDrawList.vecVtxBuffer.empty( ) )
		drawDataBuilder.AddDrawList( 0, &backgroundDrawList );

	for ( PiorityDrawList_t& draw : vecPiorityDrawList )
	{
		if ( !draw.drawList.vecVtxBuffer.empty( ) )
			drawDataBuilder.AddDrawList( draw.nPiority, &draw.drawList );
	}

	drawDataBuilder.FlattenIntoSingleLayer( );

	// add foreground DrawList_t
	if ( !foregroundDrawList.vecVtxBuffer.empty( ) )
		drawDataBuilder.AddDrawList( 0, &foregroundDrawList );

	SetupDrawData( &drawDataBuilder.vecLayers[ 0 ] );

	iMetricsRenderVertices = drawData.iTotalVtxCount;
	iMetricsRenderIndices = drawData.iTotalIdxCount;

	return &drawData;
}

void CDrawContext::SetupDrawData( CRT::Vector<DrawList_t*>* draw_lists )
{
	drawData.bValid = true;
	drawData.ppCmdLists = ( draw_lists->Size > 0 ) ? draw_lists->Data : NULL;
	drawData.iCmdListsCount = draw_lists->Size;
	drawData.iTotalVtxCount = drawData.iTotalIdxCount = 0;
	drawData.vecDisplayPos = Vector2D_t( 0.0f, 0.0f );
	drawData.vecDisplaySize = vecDisplaySize;
	drawData.vecFramebufferScale = vecDisplayFramebufferScale;
	for ( size_t n = 0; n < draw_lists->Size; n++ )
	{
		drawData.iTotalVtxCount += draw_lists->Data[ n ]->vecVtxBuffer.Size;
		drawData.iTotalIdxCount += draw_lists->Data[ n ]->vecIdxBuffer.Size;
	}
}

#pragma endregion

#pragma region draw_list_shared_data

DrawListSharedData_t::DrawListSharedData_t( )
{
	pFont = NULL;
	flFontSize = 0.0f;
	flCurveTessellationTol = 0.0f;
	flCircleSegmentMaxError = 0.0f;
	vecClipRectFullscreen = Vector4D_t( -8192.0f, -8192.0f, +8192.0f, +8192.0f );
	nInitialFlags = 0;

	// Lookup tables
	for ( int i = 0; i < CRT_ARRAYSIZE( arrCircleVtx12 ); i++ )
	{
		const float a = ( ( float )i * 2 * M::_PI ) / ( float )CRT_ARRAYSIZE( arrCircleVtx12 );
		arrCircleVtx12[ i ] = Vector2D_t( M_COS( a ), M_SIN( a ) );
	}
	CRT::MemorySet( arrCircleSegmentCounts, 0, sizeof( arrCircleSegmentCounts ) ); // This will be set by SetCircleSegmentMaxError()
}

void DrawListSharedData_t::SetCircleSegmentMaxError( float max_error )
{
	if ( flCircleSegmentMaxError == max_error )
		return;
	flCircleSegmentMaxError = max_error;
	for ( int i = 0; i < CRT_ARRAYSIZE( arrCircleSegmentCounts ); i++ )
	{
		const float flRadius = i + 1.0f;
		const int segment_count = DRAW_DRAWLIST_CIRCLE_AUTO_SEGMENT_CALC( flRadius, flCircleSegmentMaxError );
		arrCircleSegmentCounts[ i ] = ( std::uint8_t )CRT::Min( segment_count, 255 );
	}
}

#pragma endregion

#pragma region draw_data

void DrawDataBuilder_t::AddDrawList( const size_t nLayer, DrawList_t* pDrawList )
{
	CRT_ASSERTION( nLayer >= 0 && nLayer < CRT_ARRAYSIZE( vecLayers ) );

	// if the draw list is empty, we don't need to add it
	if ( pDrawList->vecCmdBuffer.empty( ) )
		return;

	// Remove trailing command if unused
	DrawCmd_t& last_cmd = pDrawList->vecCmdBuffer.back( );
	if ( last_cmd.nElemCount == 0 && last_cmd.fnUserCallback == NULL )
	{
		pDrawList->vecCmdBuffer.pop_back( );
		if ( pDrawList->vecCmdBuffer.empty( ) )
			return;
	}

	// Draw list sanity check. Detect mismatch between PrimReserve() calls and incrementing _nVtxCurrentIdx, _pVtxWritePtr etc.
	// May trigger for you if you are using PrimXXX functions incorrectly.
	CRT_ASSERTION( pDrawList->vecVtxBuffer.Size == 0 || pDrawList->_pVtxWritePtr == pDrawList->vecVtxBuffer.Data + pDrawList->vecVtxBuffer.Size );
	CRT_ASSERTION( pDrawList->vecIdxBuffer.Size == 0 || pDrawList->_pIdxWritePtr == pDrawList->vecIdxBuffer.Data + pDrawList->vecIdxBuffer.Size );
	if ( !( pDrawList->nFlags & kDrawListFlags_AllowVtxOffset ) )
		CRT_ASSERTION( ( int )pDrawList->_nVtxCurrentIdx == pDrawList->vecVtxBuffer.Size );

	if ( sizeof( DrawIdx_t ) == 2 )
		CRT_ASSERTION( pDrawList->_nVtxCurrentIdx < ( 1 << 16 ) );

	vecLayers[ nLayer ].push_back( pDrawList );
}

void DrawDataBuilder_t::FlattenIntoSingleLayer( )
{
	int n = vecLayers[ 0 ].Size;
	int size = n;
	for ( int i = 1; i < CRT_ARRAYSIZE( vecLayers ); i++ )
		size += vecLayers[ i ].Size;
	vecLayers[ 0 ].resize( size );
	for ( int layer_n = 1; layer_n < CRT_ARRAYSIZE( vecLayers ); layer_n++ )
	{
		CRT::Vector<DrawList_t*>& layer = vecLayers[ layer_n ];
		if ( layer.empty( ) )
			continue;
		CRT::MemoryCopy( &vecLayers[ 0 ][ n ], &layer[ 0 ], layer.Size * sizeof( DrawList_t* ) );
		n += layer.Size;
		layer.resize( 0 );
	}
}

void DrawData_t::DeIndexAllBuffers( )
{
	CRT::Vector<DrawVert_t> new_vtx_buffer;
	iTotalVtxCount = iTotalIdxCount = 0;
	for ( int i = 0; i < iCmdListsCount; i++ )
	{
		DrawList_t* cmd_list = ppCmdLists[ i ];
		if ( cmd_list->vecIdxBuffer.empty( ) )
			continue;
		new_vtx_buffer.resize( cmd_list->vecIdxBuffer.Size );
		for ( size_t j = 0; j < cmd_list->vecIdxBuffer.Size; j++ )
			new_vtx_buffer[ j ] = cmd_list->vecVtxBuffer[ cmd_list->vecIdxBuffer[ j ] ];
		cmd_list->vecVtxBuffer.swap( new_vtx_buffer );
		cmd_list->vecIdxBuffer.resize( 0 );
		iTotalVtxCount += cmd_list->vecVtxBuffer.Size;
	}
}

void DrawData_t::ScaleClipRects( const Vector2D_t& fb_scale )
{
	for ( int i = 0; i < iCmdListsCount; i++ )
	{
		DrawList_t* cmd_list = ppCmdLists[ i ];
		for ( size_t cmd_i = 0; cmd_i < cmd_list->vecCmdBuffer.Size; cmd_i++ )
		{
			DrawCmd_t* cmd = &cmd_list->vecCmdBuffer[ cmd_i ];
			cmd->vecClipRect = Vector4D_t( cmd->vecClipRect.x * fb_scale.x, cmd->vecClipRect.y * fb_scale.y, cmd->vecClipRect.z * fb_scale.x, cmd->vecClipRect.w * fb_scale.y );
		}
	}
}

#pragma endregion

#pragma region draw_list

void DrawList_t::PushClipRect( Vector2D_t cr_min, Vector2D_t cr_max, bool intersect_with_current_clip_rect )
{
	Vector4D_t cr( cr_min.x, cr_min.y, cr_max.x, cr_max.y );
	if ( intersect_with_current_clip_rect && _vecClipRectStack.Size )
	{
		Vector4D_t current = _vecClipRectStack.Data[ _vecClipRectStack.Size - 1 ];
		if ( cr.x < current.x )
			cr.x = current.x;
		if ( cr.y < current.y )
			cr.y = current.y;
		if ( cr.z > current.z )
			cr.z = current.z;
		if ( cr.w > current.w )
			cr.w = current.w;
	}
	cr.z = CRT::Max( cr.x, cr.z );
	cr.w = CRT::Max( cr.y, cr.w );

	_vecClipRectStack.push_back( cr );
	UpdateClipRect( );
}

void DrawList_t::PushClipRectFullScreen( )
{
	PushClipRect( Vector2D_t( _pData->vecClipRectFullscreen.x, _pData->vecClipRectFullscreen.y ), Vector2D_t( _pData->vecClipRectFullscreen.z, _pData->vecClipRectFullscreen.w ) );
}

void DrawList_t::PopClipRect( )
{
	CRT_ASSERTION( _vecClipRectStack.Size > 0 );
	_vecClipRectStack.pop_back( );
	UpdateClipRect( );
}

void DrawList_t::PushTextureID( TextureID_t texture_id )
{
	_vecTextureIdStack.push_back( texture_id );
	UpdateTextureID( );
}

void DrawList_t::PopTextureID( )
{
	CRT_ASSERTION( _vecTextureIdStack.Size > 0 );
	_vecTextureIdStack.pop_back( );
	UpdateTextureID( );
}

TextureID_t DrawList_t::GetCurrentTextureId( ) const
{
	return _vecTextureIdStack.Size ? _vecTextureIdStack.Data[ _vecTextureIdStack.Size - 1 ] : nullptr;
}

const Vector4D_t& DrawList_t::GetCurrentClipRect( ) const
{
	return _vecClipRectStack.Size ? _vecClipRectStack.Data[ _vecClipRectStack.Size - 1 ] : _pData->vecClipRectFullscreen;
}


#define DRAW_NORMALIZE2F_OVER_ZERO(VX,VY)     { float d2 = VX*VX + VY*VY; if (d2 > 0.0f) { float inv_len = 1.0f / M_SQRT(d2); VX *= inv_len; VY *= inv_len; } }
#define DRAW_FIXNORMAL2F(VX,VY)               { float d2 = VX*VX + VY*VY; if (d2 < 0.5f) d2 = 0.5f; float inv_lensq = 1.0f / d2; VX *= inv_lensq; VY *= inv_lensq; }

void DrawList_t::AddLine( const Vector2D_t& vecFirst, const Vector2D_t& vecSecond, const ColorPacked_t uCol, float flThickness )
{
	if ( ( uCol & DRAW_COL32_A_MASK ) == 0 )
		return;
	PathLineTo( vecFirst + Vector2D_t( 0.5f, 0.5f ) );
	PathLineTo( vecSecond + Vector2D_t( 0.5f, 0.5f ) );
	PathStroke( uCol, false, flThickness );
}

void DrawList_t::AddRect( const Vector2D_t& vecMin, const Vector2D_t& vecMax, const ColorPacked_t uCol, float flRounding, DrawFlags_t nDrawFlags, float flThickness )
{
	if ( ( uCol & DRAW_COL32_A_MASK ) == 0 )
		return;

	if ( nDrawFlags & kDrawListFlags_AntiAliasedLines )
		PathRect( vecMin + Vector2D_t( 0.50f, 0.50f ), vecMax - Vector2D_t( 0.50f, 0.50f ), flRounding, nDrawFlags );
	else
		PathRect( vecMin + Vector2D_t( 0.50f, 0.50f ), vecMax - Vector2D_t( 0.49f, 0.49f ), flRounding, nDrawFlags ); // Better looking lower-right corner and rounded non-AA shapes.

	if ( nDrawFlags & kDrawFlags_FillConvex )
	{
		CRT_UNUNSED( flThickness );
		PathFillConvex( uCol );
	}
	else
		PathStroke( uCol, true, flThickness );
}

void DrawList_t::AddRectFilledMultiColor( const Vector2D_t& vecMin, const Vector2D_t& vecMax, const ColorPacked_t uColUpLeft, const ColorPacked_t uColUpRight, const ColorPacked_t uColBotRight, const ColorPacked_t uColBotLeft, float flRounding, DrawFlags_t nDrawFlags )
{
	if ( ( ( uColUpLeft | uColUpRight | uColBotRight | uColBotLeft ) & DRAW_COL32_A_MASK ) == 0 )
		return;

	flRounding = CRT::Min( flRounding, M_FABS( vecMax.x - vecMin.x ) * ( ( ( nDrawFlags & kDrawFlags_Top ) == kDrawFlags_Top ) || ( ( nDrawFlags & kDrawFlags_Bot ) == kDrawFlags_Bot ) ? 0.5f : 1.0f ) - 1.0f );
	flRounding = CRT::Min( flRounding, M_FABS( vecMax.y - vecMin.y ) * ( ( ( nDrawFlags & kDrawFlags_Left ) == kDrawFlags_Left ) || ( ( nDrawFlags & kDrawFlags_Right ) == kDrawFlags_Right ) ? 0.5f : 1.0f ) - 1.0f );
	if ( flRounding > 0.0f )
	{
		static const auto ConvertPackedToVec4 = [ ]( const ColorPacked_t uColIn )
			{
				float s = 1.0f / 255.0f;
				return Vector4D_t(
					( ( uColIn >> DRAW_COL32_R_SHIFT ) & 0xFF ) * s,
					( ( uColIn >> DRAW_COL32_G_SHIFT ) & 0xFF ) * s,
					( ( uColIn >> DRAW_COL32_B_SHIFT ) & 0xFF ) * s,
					( ( uColIn >> DRAW_COL32_A_SHIFT ) & 0xFF ) * s );
			};

		const int size_before = vecVtxBuffer.Size;
		AddRect( vecMin, vecMax, DRAW_COL32_WHITE, flRounding, kDrawFlags_FillConvex );
		const int size_after = vecVtxBuffer.Size;

		for ( int i = size_before; i < size_after; i++ )
		{
			DrawVert_t* vert = &vecVtxBuffer.Data[ i ];

			Vector4D_t upr_left = ConvertPackedToVec4( uColUpLeft );
			Vector4D_t bot_left = ConvertPackedToVec4( uColBotLeft );
			Vector4D_t up_right = ConvertPackedToVec4( uColUpRight );
			Vector4D_t bot_right = ConvertPackedToVec4( uColBotRight );

			float X = CRT::Clamp( ( vert->vecPostion.x - vecMin.x ) / ( vecMax.x - vecMin.x ), 0.0f, 1.0f );

			// 4 colors - 8 deltas

			float r1 = upr_left.x + ( up_right.x - upr_left.x ) * X;
			float r2 = bot_left.x + ( bot_right.x - bot_left.x ) * X;

			float g1 = upr_left.y + ( up_right.y - upr_left.y ) * X;
			float g2 = bot_left.y + ( bot_right.y - bot_left.y ) * X;

			float b1 = upr_left.z + ( up_right.z - upr_left.z ) * X;
			float b2 = bot_left.z + ( bot_right.z - bot_left.z ) * X;

			float a1 = upr_left.w + ( up_right.w - upr_left.w ) * X;
			float a2 = bot_left.w + ( bot_right.w - bot_left.w ) * X;

			float Y = CRT::Clamp( ( vert->vecPostion.y - vecMin.y ) / ( vecMax.y - vecMin.y ), 0.0f, 1.0f );
			float r = r1 + ( r2 - r1 ) * Y;
			float g = g1 + ( g2 - g1 ) * Y;
			float b = b1 + ( b2 - b1 ) * Y;
			float a = a1 + ( a2 - a1 ) * Y;

			vert->uCol = Color_t( r, g, b, a ).GetU32( );
		}

		// dont' do default non rounding method
		return;
	}

	const Vector2D_t vecUV = _pData->vecTexUvWhitePixel;
	PrimReserve( 6, 4 );
	PrimWriteIdx( ( DrawIdx_t )( _nVtxCurrentIdx ) );
	PrimWriteIdx( ( DrawIdx_t )( _nVtxCurrentIdx + 1 ) );
	PrimWriteIdx( ( DrawIdx_t )( _nVtxCurrentIdx + 2 ) );
	PrimWriteIdx( ( DrawIdx_t )( _nVtxCurrentIdx ) );
	PrimWriteIdx( ( DrawIdx_t )( _nVtxCurrentIdx + 2 ) );
	PrimWriteIdx( ( DrawIdx_t )( _nVtxCurrentIdx + 3 ) );
	PrimWriteVtx( vecMin, vecUV, uColUpLeft );
	PrimWriteVtx( Vector2D_t( vecMax.x, vecMin.y ), vecUV, uColUpRight );
	PrimWriteVtx( vecMax, vecUV, uColBotRight );
	PrimWriteVtx( Vector2D_t( vecMin.x, vecMax.y ), vecUV, uColBotLeft );
}

void DrawList_t::AddQuad( const Vector2D_t& vecFirst, const Vector2D_t& vecSecond, const Vector2D_t& vecThird, const Vector2D_t& vecFourth, const ColorPacked_t uCol, float flThickness, const DrawFlags_t nDrawFlags )
{
	if ( ( uCol & DRAW_COL32_A_MASK ) == 0 )
		return;

	PathLineTo( vecFirst );
	PathLineTo( vecSecond );
	PathLineTo( vecThird );
	PathLineTo( vecFourth );

	if ( nDrawFlags & kDrawFlags_FillConvex )
	{
		CRT_UNUNSED( flThickness );
		PathFillConvex( uCol );
	}
	else
		PathStroke( uCol, false, flThickness );
}

void DrawList_t::AddTriangle( const Vector2D_t& vecFirst, const Vector2D_t& vecSecond, const Vector2D_t& vecThird, const ColorPacked_t uCol, float flThickness, const DrawFlags_t nDrawFlags )
{
	if ( ( uCol & DRAW_COL32_A_MASK ) == 0 )
		return;

	PathLineTo( vecFirst );
	PathLineTo( vecSecond );
	PathLineTo( vecThird );

	if ( nDrawFlags & kDrawFlags_FillConvex )
	{
		CRT_UNUNSED( flThickness );
		PathFillConvex( uCol );
	}
	else
		PathStroke( uCol, true, flThickness );
}

void DrawList_t::AddCircle( const Vector2D_t& vecCenter, float flRadius, const ColorPacked_t uCol, int iSegments, float flThickness, const DrawFlags_t nDrawFlags )
{
	if ( ( uCol & DRAW_COL32_A_MASK ) == 0 || flRadius <= 0.5f )
		return;

	// Obtain segment count
	if ( iSegments <= 2 )
	{
		// Automatic segment count
		const int radius_idx = ( int )flRadius - 1;
		if ( radius_idx < CRT_ARRAYSIZE( _pData->arrCircleSegmentCounts ) )
			iSegments = _pData->arrCircleSegmentCounts[ radius_idx ]; // Use cached value
		else
			iSegments = DRAW_DRAWLIST_CIRCLE_AUTO_SEGMENT_CALC( flRadius, _pData->flCircleSegmentMaxError );
	}
	else
	{
		// Explicit segment count (still clamp to avoid drawing insanely tessellated shapes)
		iSegments = CRT::Clamp( iSegments, 3, DRAW_DRAWLIST_CIRCLE_AUTO_SEGMENT_MAX );
	}

	// Because we are filling a closed shape we remove 1 from the count of segments/pPoints
	const float flArcMax = M::_2PI * ( static_cast< float >( iSegments ) - 1.0f ) / static_cast< float >( iSegments );
	if ( iSegments == 12 )
		PathArcToFast( vecCenter, flRadius - 0.5f, 0, 12 );
	else
		PathArcTo( vecCenter, flRadius - 0.5f, 0.0f, flArcMax, iSegments - 1 );

	if ( nDrawFlags & kDrawFlags_FillConvex )
	{
		CRT_UNUNSED( flThickness );
		PathFillConvex( uCol );
	}
	else
		PathStroke( uCol, true, flThickness );
}

void DrawList_t::AddCircleMultiColor( const Vector2D_t& vecCenter, float flRadius, const ColorPacked_t uColCenter, const ColorPacked_t uColOuter, int iSegments )
{
	if ( ( uColCenter & DRAW_COL32_A_MASK ) == 0 || ( uColOuter & DRAW_COL32_A_MASK ) == 0 || flRadius <= 0.5f )
		return;

	if ( iSegments <= 3 )
	{
		// Automatic segment count
		const int radius_idx = ( int )flRadius - 1;
		if ( radius_idx < CRT_ARRAYSIZE( _pData->arrCircleSegmentCounts ) )
			iSegments = _pData->arrCircleSegmentCounts[ radius_idx ]; // Use cached value
		else
			iSegments = DRAW_DRAWLIST_CIRCLE_AUTO_SEGMENT_CALC( flRadius, _pData->flCircleSegmentMaxError );
	}
	else
	{
		// Explicit segment count (still clamp to avoid drawing insanely tessellated shapes)
		iSegments = CRT::Clamp( iSegments, 3, DRAW_DRAWLIST_CIRCLE_AUTO_SEGMENT_MAX );
	}

	// Use arc with automatic segment count
	const float flArcMax = M::_2PI * ( static_cast< float >( iSegments ) ) / static_cast< float >( iSegments );
	if ( iSegments == 12 )
		PathArcToFast( vecCenter, flRadius, 0, 12 );
	else
		PathArcTo( vecCenter, flRadius, 0.0f, flArcMax, iSegments - 1 );

	const int count = _vecPath.Size - 1;

	unsigned int vtx_base = _nVtxCurrentIdx;
	PrimReserve( count * 3, count + 1 );

	// Submit vertices
	const Vector2D_t uv = _pData->vecTexUvWhitePixel;
	PrimWriteVtx( vecCenter, uv, uColCenter );
	for ( int n = 0; n < count; n++ )
		PrimWriteVtx( _vecPath[ n ], uv, uColOuter );

	// Submit a fan of triangles
	for ( int n = 0; n < count; n++ )
	{
		PrimWriteIdx( ( DrawIdx_t )( vtx_base ) );
		PrimWriteIdx( ( DrawIdx_t )( vtx_base + 1 + n ) );
		PrimWriteIdx( ( DrawIdx_t )( vtx_base + 1 + ( ( n + 1 ) % count ) ) );
	}
	_vecPath.Size = 0;
}

void DrawList_t::AddNgon( const Vector2D_t& vecCenter, float flRadius, const ColorPacked_t uCol, int iSegments, float flThickness, const DrawFlags_t nDrawFlags )
{
	if ( ( uCol & DRAW_COL32_A_MASK ) == 0 || iSegments <= 2 )
		return;

	// Because we are filling a bClosed shape we remove 1 from the count of segments/pPoints
	const float flArcMax = M::_2PI * ( static_cast< float >( iSegments ) ) / static_cast< float >( iSegments );
	PathArcTo( vecCenter, flRadius - 0.5f, 0.0f, flArcMax, iSegments - 1 );

	if ( nDrawFlags & kDrawFlags_FillConvex )
	{
		CRT_UNUNSED( flThickness );
		PathFillConvex( uCol );
	}
	else
		PathStroke( uCol, true, flThickness );
}

void DrawList_t::AddText( const Vector2D_t& vecPostion, const ColorPacked_t uCol, const char* szTextBegin, const char* szTextEnd )
{
	AddText( nullptr, 0.0f, vecPostion, uCol, szTextBegin, szTextEnd );
}

void DrawList_t::AddText( const Font_t* pFont, float font_size, const Vector2D_t& vecPostion, const ColorPacked_t uCol, const char* szTextBegin, const char* szTextEnd, float flWrapWidth, const Vector4D_t* vecCpuFineClipRect )
{
	if ( ( uCol & DRAW_COL32_A_MASK ) == 0 )
		return;

	if ( szTextEnd == NULL )
		szTextEnd = szTextBegin + CRT::StringLength( szTextBegin );
	if ( szTextBegin == szTextEnd )
		return;

	// Pull default pFont/size from the shared ImDrawListSharedData instance
	if ( pFont == NULL )
		pFont = _pData->pFont;
	if ( font_size == 0.0f )
		font_size = _pData->flFontSize;

	// use DrawList_t::PushTextureId() to change pFont.
	CRT_ASSERTION( pFont->pContainerAtlas->pTexID == _vecTextureIdStack.back( ) );

	Vector4D_t clip_rect = _vecClipRectStack.back( );
	if ( vecCpuFineClipRect )
	{
		clip_rect.x = CRT::Max( clip_rect.x, vecCpuFineClipRect->x );
		clip_rect.y = CRT::Max( clip_rect.y, vecCpuFineClipRect->y );
		clip_rect.z = CRT::Min( clip_rect.z, vecCpuFineClipRect->z );
		clip_rect.w = CRT::Min( clip_rect.w, vecCpuFineClipRect->w );
	}

	pFont->RenderText( this, font_size, vecPostion, uCol, clip_rect, szTextBegin, szTextEnd, flWrapWidth, vecCpuFineClipRect != NULL );
}

void DrawList_t::AddPolyline( const Vector2D_t* pPoints, size_t points_count, const ColorPacked_t uCol, bool bClosed, float flThickness )
{
	if ( points_count < 2 )
		return;

	const Vector2D_t vecUV = _pData->vecTexUvWhitePixel;

	int count = points_count;
	if ( !bClosed )
		count = points_count - 1;

	const bool thick_line = flThickness > 1.0f;
	if ( nFlags & kDrawListFlags_AntiAliasedLines )
	{
		// Anti-aliased stroke
		const float AA_SIZE = 1.0f;
		const ColorPacked_t col_trans = uCol & ~DRAW_COL32_A_MASK;

		const int nIdxCount = thick_line ? count * 18 : count * 12;
		const int nVtxCount = thick_line ? points_count * 4 : points_count * 3;
		PrimReserve( nIdxCount, nVtxCount );

		// Temporary buffer
		Vector2D_t* temp_normals = MEM_STACKALLOC( Vector2D_t*, points_count * ( thick_line ? 5 : 3 ) * sizeof( Vector2D_t ) ); //-V630
		Vector2D_t* temp_points = temp_normals + points_count;

		for ( int i1 = 0; i1 < count; i1++ )
		{
			const int i2 = ( i1 + 1 ) == points_count ? 0 : i1 + 1;
			float dx = pPoints[ i2 ].x - pPoints[ i1 ].x;
			float dy = pPoints[ i2 ].y - pPoints[ i1 ].y;
			DRAW_NORMALIZE2F_OVER_ZERO( dx, dy );
			temp_normals[ i1 ].x = dy;
			temp_normals[ i1 ].y = -dx;
		}
		if ( !bClosed )
			temp_normals[ points_count - 1 ] = temp_normals[ points_count - 2 ];

		if ( !thick_line )
		{
			if ( !bClosed )
			{
				temp_points[ 0 ] = pPoints[ 0 ] + temp_normals[ 0 ] * AA_SIZE;
				temp_points[ 1 ] = pPoints[ 0 ] - temp_normals[ 0 ] * AA_SIZE;
				temp_points[ ( points_count - 1 ) * 2 + 0 ] = pPoints[ points_count - 1 ] + temp_normals[ points_count - 1 ] * AA_SIZE;
				temp_points[ ( points_count - 1 ) * 2 + 1 ] = pPoints[ points_count - 1 ] - temp_normals[ points_count - 1 ] * AA_SIZE;
			}

			// FIXME-OPT: Merge the different loops, possibly remove the temporary buffer.
			std::uint32_t idx1 = _nVtxCurrentIdx;
			for ( int i1 = 0; i1 < count; i1++ )
			{
				const int i2 = ( i1 + 1 ) == points_count ? 0 : i1 + 1;
				std::uint32_t idx2 = ( i1 + 1 ) == points_count ? _nVtxCurrentIdx : idx1 + 3;

				// Average normals
				float dm_x = ( temp_normals[ i1 ].x + temp_normals[ i2 ].x ) * 0.5f;
				float dm_y = ( temp_normals[ i1 ].y + temp_normals[ i2 ].y ) * 0.5f;
				DRAW_FIXNORMAL2F( dm_x, dm_y )
					dm_x *= AA_SIZE;
				dm_y *= AA_SIZE;

				// Add temporary vertexes
				Vector2D_t* out_vtx = &temp_points[ i2 * 2 ];
				out_vtx[ 0 ].x = pPoints[ i2 ].x + dm_x;
				out_vtx[ 0 ].y = pPoints[ i2 ].y + dm_y;
				out_vtx[ 1 ].x = pPoints[ i2 ].x - dm_x;
				out_vtx[ 1 ].y = pPoints[ i2 ].y - dm_y;

				// Add indexes
				_pIdxWritePtr[ 0 ] = ( DrawIdx_t )( idx2 + 0 );
				_pIdxWritePtr[ 1 ] = ( DrawIdx_t )( idx1 + 0 );
				_pIdxWritePtr[ 2 ] = ( DrawIdx_t )( idx1 + 2 );
				_pIdxWritePtr[ 3 ] = ( DrawIdx_t )( idx1 + 2 );
				_pIdxWritePtr[ 4 ] = ( DrawIdx_t )( idx2 + 2 );
				_pIdxWritePtr[ 5 ] = ( DrawIdx_t )( idx2 + 0 );
				_pIdxWritePtr[ 6 ] = ( DrawIdx_t )( idx2 + 1 );
				_pIdxWritePtr[ 7 ] = ( DrawIdx_t )( idx1 + 1 );
				_pIdxWritePtr[ 8 ] = ( DrawIdx_t )( idx1 + 0 );
				_pIdxWritePtr[ 9 ] = ( DrawIdx_t )( idx1 + 0 );
				_pIdxWritePtr[ 10 ] = ( DrawIdx_t )( idx2 + 0 );
				_pIdxWritePtr[ 11 ] = ( DrawIdx_t )( idx2 + 1 );
				_pIdxWritePtr += 12;

				idx1 = idx2;
			}

			// Add vertexes
			for ( int i = 0; i < points_count; i++ )
			{
				_pVtxWritePtr[ 0 ].vecPostion = pPoints[ i ];
				_pVtxWritePtr[ 0 ].vecUV = vecUV;
				_pVtxWritePtr[ 0 ].uCol = uCol;
				_pVtxWritePtr[ 1 ].vecPostion = temp_points[ i * 2 + 0 ];
				_pVtxWritePtr[ 1 ].vecUV = vecUV;
				_pVtxWritePtr[ 1 ].uCol = col_trans;
				_pVtxWritePtr[ 2 ].vecPostion = temp_points[ i * 2 + 1 ];
				_pVtxWritePtr[ 2 ].vecUV = vecUV;
				_pVtxWritePtr[ 2 ].uCol = col_trans;
				_pVtxWritePtr += 3;
			}
		}
		else
		{
			const float half_inner_thickness = ( flThickness - AA_SIZE ) * 0.5f;
			if ( !bClosed )
			{
				temp_points[ 0 ] = pPoints[ 0 ] + temp_normals[ 0 ] * ( half_inner_thickness + AA_SIZE );
				temp_points[ 1 ] = pPoints[ 0 ] + temp_normals[ 0 ] * ( half_inner_thickness );
				temp_points[ 2 ] = pPoints[ 0 ] - temp_normals[ 0 ] * ( half_inner_thickness );
				temp_points[ 3 ] = pPoints[ 0 ] - temp_normals[ 0 ] * ( half_inner_thickness + AA_SIZE );
				temp_points[ ( points_count - 1 ) * 4 + 0 ] = pPoints[ points_count - 1 ] + temp_normals[ points_count - 1 ] * ( half_inner_thickness + AA_SIZE );
				temp_points[ ( points_count - 1 ) * 4 + 1 ] = pPoints[ points_count - 1 ] + temp_normals[ points_count - 1 ] * ( half_inner_thickness );
				temp_points[ ( points_count - 1 ) * 4 + 2 ] = pPoints[ points_count - 1 ] - temp_normals[ points_count - 1 ] * ( half_inner_thickness );
				temp_points[ ( points_count - 1 ) * 4 + 3 ] = pPoints[ points_count - 1 ] - temp_normals[ points_count - 1 ] * ( half_inner_thickness + AA_SIZE );
			}

			// FIXME-OPT: Merge the different loops, possibly remove the temporary buffer.
			std::uint32_t idx1 = _nVtxCurrentIdx;
			for ( int i1 = 0; i1 < count; i1++ )
			{
				const int i2 = ( i1 + 1 ) == points_count ? 0 : i1 + 1;
				std::uint32_t idx2 = ( i1 + 1 ) == points_count ? _nVtxCurrentIdx : idx1 + 4;

				// Average normals
				float dm_x = ( temp_normals[ i1 ].x + temp_normals[ i2 ].x ) * 0.5f;
				float dm_y = ( temp_normals[ i1 ].y + temp_normals[ i2 ].y ) * 0.5f;
				DRAW_FIXNORMAL2F( dm_x, dm_y );
				float dm_out_x = dm_x * ( half_inner_thickness + AA_SIZE );
				float dm_out_y = dm_y * ( half_inner_thickness + AA_SIZE );
				float dm_in_x = dm_x * half_inner_thickness;
				float dm_in_y = dm_y * half_inner_thickness;

				// Add temporary vertexes
				Vector2D_t* out_vtx = &temp_points[ i2 * 4 ];
				out_vtx[ 0 ].x = pPoints[ i2 ].x + dm_out_x;
				out_vtx[ 0 ].y = pPoints[ i2 ].y + dm_out_y;
				out_vtx[ 1 ].x = pPoints[ i2 ].x + dm_in_x;
				out_vtx[ 1 ].y = pPoints[ i2 ].y + dm_in_y;
				out_vtx[ 2 ].x = pPoints[ i2 ].x - dm_in_x;
				out_vtx[ 2 ].y = pPoints[ i2 ].y - dm_in_y;
				out_vtx[ 3 ].x = pPoints[ i2 ].x - dm_out_x;
				out_vtx[ 3 ].y = pPoints[ i2 ].y - dm_out_y;

				// Add indexes
				_pIdxWritePtr[ 0 ] = ( DrawIdx_t )( idx2 + 1 );
				_pIdxWritePtr[ 1 ] = ( DrawIdx_t )( idx1 + 1 );
				_pIdxWritePtr[ 2 ] = ( DrawIdx_t )( idx1 + 2 );
				_pIdxWritePtr[ 3 ] = ( DrawIdx_t )( idx1 + 2 );
				_pIdxWritePtr[ 4 ] = ( DrawIdx_t )( idx2 + 2 );
				_pIdxWritePtr[ 5 ] = ( DrawIdx_t )( idx2 + 1 );
				_pIdxWritePtr[ 6 ] = ( DrawIdx_t )( idx2 + 1 );
				_pIdxWritePtr[ 7 ] = ( DrawIdx_t )( idx1 + 1 );
				_pIdxWritePtr[ 8 ] = ( DrawIdx_t )( idx1 + 0 );
				_pIdxWritePtr[ 9 ] = ( DrawIdx_t )( idx1 + 0 );
				_pIdxWritePtr[ 10 ] = ( DrawIdx_t )( idx2 + 0 );
				_pIdxWritePtr[ 11 ] = ( DrawIdx_t )( idx2 + 1 );
				_pIdxWritePtr[ 12 ] = ( DrawIdx_t )( idx2 + 2 );
				_pIdxWritePtr[ 13 ] = ( DrawIdx_t )( idx1 + 2 );
				_pIdxWritePtr[ 14 ] = ( DrawIdx_t )( idx1 + 3 );
				_pIdxWritePtr[ 15 ] = ( DrawIdx_t )( idx1 + 3 );
				_pIdxWritePtr[ 16 ] = ( DrawIdx_t )( idx2 + 3 );
				_pIdxWritePtr[ 17 ] = ( DrawIdx_t )( idx2 + 2 );
				_pIdxWritePtr += 18;

				idx1 = idx2;
			}

			// Add vertexes
			for ( int i = 0; i < points_count; i++ )
			{
				_pVtxWritePtr[ 0 ].vecPostion = temp_points[ i * 4 + 0 ];
				_pVtxWritePtr[ 0 ].vecUV = vecUV;
				_pVtxWritePtr[ 0 ].uCol = col_trans;
				_pVtxWritePtr[ 1 ].vecPostion = temp_points[ i * 4 + 1 ];
				_pVtxWritePtr[ 1 ].vecUV = vecUV;
				_pVtxWritePtr[ 1 ].uCol = uCol;
				_pVtxWritePtr[ 2 ].vecPostion = temp_points[ i * 4 + 2 ];
				_pVtxWritePtr[ 2 ].vecUV = vecUV;
				_pVtxWritePtr[ 2 ].uCol = uCol;
				_pVtxWritePtr[ 3 ].vecPostion = temp_points[ i * 4 + 3 ];
				_pVtxWritePtr[ 3 ].vecUV = vecUV;
				_pVtxWritePtr[ 3 ].uCol = col_trans;
				_pVtxWritePtr += 4;
			}
		}
		_nVtxCurrentIdx += ( DrawIdx_t )nVtxCount;
	}
	else
	{
		// Non Anti-aliased Stroke
		const int nIdxCount = count * 6;
		const int nVtxCount = count * 4; // FIXME-OPT: Not sharing edges
		PrimReserve( nIdxCount, nVtxCount );

		for ( int i1 = 0; i1 < count; i1++ )
		{
			const int i2 = ( i1 + 1 ) == points_count ? 0 : i1 + 1;
			const Vector2D_t& vecFirst = pPoints[ i1 ];
			const Vector2D_t& vecSecond = pPoints[ i2 ];

			float dx = vecSecond.x - vecFirst.x;
			float dy = vecSecond.y - vecFirst.y;
			DRAW_NORMALIZE2F_OVER_ZERO( dx, dy );
			dx *= ( flThickness * 0.5f );
			dy *= ( flThickness * 0.5f );

			_pVtxWritePtr[ 0 ].vecPostion.x = vecFirst.x + dy;
			_pVtxWritePtr[ 0 ].vecPostion.y = vecFirst.y - dx;
			_pVtxWritePtr[ 0 ].vecUV = vecUV;
			_pVtxWritePtr[ 0 ].uCol = uCol;
			_pVtxWritePtr[ 1 ].vecPostion.x = vecSecond.x + dy;
			_pVtxWritePtr[ 1 ].vecPostion.y = vecSecond.y - dx;
			_pVtxWritePtr[ 1 ].vecUV = vecUV;
			_pVtxWritePtr[ 1 ].uCol = uCol;
			_pVtxWritePtr[ 2 ].vecPostion.x = vecSecond.x - dy;
			_pVtxWritePtr[ 2 ].vecPostion.y = vecSecond.y + dx;
			_pVtxWritePtr[ 2 ].vecUV = vecUV;
			_pVtxWritePtr[ 2 ].uCol = uCol;
			_pVtxWritePtr[ 3 ].vecPostion.x = vecFirst.x - dy;
			_pVtxWritePtr[ 3 ].vecPostion.y = vecFirst.y + dx;
			_pVtxWritePtr[ 3 ].vecUV = vecUV;
			_pVtxWritePtr[ 3 ].uCol = uCol;
			_pVtxWritePtr += 4;

			_pIdxWritePtr[ 0 ] = ( DrawIdx_t )( _nVtxCurrentIdx );
			_pIdxWritePtr[ 1 ] = ( DrawIdx_t )( _nVtxCurrentIdx + 1 );
			_pIdxWritePtr[ 2 ] = ( DrawIdx_t )( _nVtxCurrentIdx + 2 );
			_pIdxWritePtr[ 3 ] = ( DrawIdx_t )( _nVtxCurrentIdx );
			_pIdxWritePtr[ 4 ] = ( DrawIdx_t )( _nVtxCurrentIdx + 2 );
			_pIdxWritePtr[ 5 ] = ( DrawIdx_t )( _nVtxCurrentIdx + 3 );
			_pIdxWritePtr += 6;
			_nVtxCurrentIdx += 4;
		}
	}
}

void DrawList_t::AddConvexPolyFilled( const Vector2D_t* pPoints, size_t points_count, const ColorPacked_t uCol )
{
	if ( points_count < 3 )
		return;

	const Vector2D_t vecUV = _pData->vecTexUvWhitePixel;

	if ( nFlags & kDrawListFlags_AntiAliasedFill )
	{
		// Anti-aliased Fill
		const float AA_SIZE = 1.0f;
		const ColorPacked_t col_trans = uCol & ~DRAW_COL32_A_MASK;
		const int nIdxCount = ( points_count - 2 ) * 3 + points_count * 6;
		const int nVtxCount = ( points_count * 2 );
		PrimReserve( nIdxCount, nVtxCount );

		// Add indexes for fill
		std::uint32_t vtx_inner_idx = _nVtxCurrentIdx;
		std::uint32_t vtx_outer_idx = _nVtxCurrentIdx + 1;
		for ( int i = 2; i < points_count; i++ )
		{
			_pIdxWritePtr[ 0 ] = ( DrawIdx_t )( vtx_inner_idx );
			_pIdxWritePtr[ 1 ] = ( DrawIdx_t )( vtx_inner_idx + ( ( i - 1 ) << 1 ) );
			_pIdxWritePtr[ 2 ] = ( DrawIdx_t )( vtx_inner_idx + ( i << 1 ) );
			_pIdxWritePtr += 3;
		}

		// Compute normals
		Vector2D_t* temp_normals = MEM_STACKALLOC( Vector2D_t*, points_count * sizeof( Vector2D_t ) ); //-V630
		for ( int i0 = points_count - 1, i1 = 0; i1 < points_count; i0 = i1++ )
		{
			const Vector2D_t& p0 = pPoints[ i0 ];
			const Vector2D_t& vecFirst = pPoints[ i1 ];
			float dx = vecFirst.x - p0.x;
			float dy = vecFirst.y - p0.y;
			DRAW_NORMALIZE2F_OVER_ZERO( dx, dy );
			temp_normals[ i0 ].x = dy;
			temp_normals[ i0 ].y = -dx;
		}

		for ( int i0 = points_count - 1, i1 = 0; i1 < points_count; i0 = i1++ )
		{
			// Average normals
			const Vector2D_t& n0 = temp_normals[ i0 ];
			const Vector2D_t& n1 = temp_normals[ i1 ];
			float dm_x = ( n0.x + n1.x ) * 0.5f;
			float dm_y = ( n0.y + n1.y ) * 0.5f;
			DRAW_FIXNORMAL2F( dm_x, dm_y );
			dm_x *= AA_SIZE * 0.5f;
			dm_y *= AA_SIZE * 0.5f;

			// Add vertices
			_pVtxWritePtr[ 0 ].vecPostion.x = ( pPoints[ i1 ].x - dm_x );
			_pVtxWritePtr[ 0 ].vecPostion.y = ( pPoints[ i1 ].y - dm_y );
			_pVtxWritePtr[ 0 ].vecUV = vecUV;
			_pVtxWritePtr[ 0 ].uCol = uCol; // Inner
			_pVtxWritePtr[ 1 ].vecPostion.x = ( pPoints[ i1 ].x + dm_x );
			_pVtxWritePtr[ 1 ].vecPostion.y = ( pPoints[ i1 ].y + dm_y );
			_pVtxWritePtr[ 1 ].vecUV = vecUV;
			_pVtxWritePtr[ 1 ].uCol = col_trans; // Outer
			_pVtxWritePtr += 2;

			// Add indexes for fringes
			_pIdxWritePtr[ 0 ] = ( DrawIdx_t )( vtx_inner_idx + ( i1 << 1 ) );
			_pIdxWritePtr[ 1 ] = ( DrawIdx_t )( vtx_inner_idx + ( i0 << 1 ) );
			_pIdxWritePtr[ 2 ] = ( DrawIdx_t )( vtx_outer_idx + ( i0 << 1 ) );
			_pIdxWritePtr[ 3 ] = ( DrawIdx_t )( vtx_outer_idx + ( i0 << 1 ) );
			_pIdxWritePtr[ 4 ] = ( DrawIdx_t )( vtx_outer_idx + ( i1 << 1 ) );
			_pIdxWritePtr[ 5 ] = ( DrawIdx_t )( vtx_inner_idx + ( i1 << 1 ) );
			_pIdxWritePtr += 6;
		}
		_nVtxCurrentIdx += ( DrawIdx_t )nVtxCount;
	}
	else
	{
		// Non Anti-aliased Fill
		const int nIdxCount = ( points_count - 2 ) * 3;
		const int nVtxCount = points_count;
		PrimReserve( nIdxCount, nVtxCount );
		for ( int i = 0; i < nVtxCount; i++ )
		{
			_pVtxWritePtr[ 0 ].vecPostion = pPoints[ i ];
			_pVtxWritePtr[ 0 ].vecUV = vecUV;
			_pVtxWritePtr[ 0 ].uCol = uCol;
			_pVtxWritePtr++;
		}
		for ( int i = 2; i < points_count; i++ )
		{
			_pIdxWritePtr[ 0 ] = ( DrawIdx_t )( _nVtxCurrentIdx );
			_pIdxWritePtr[ 1 ] = ( DrawIdx_t )( _nVtxCurrentIdx + i - 1 );
			_pIdxWritePtr[ 2 ] = ( DrawIdx_t )( _nVtxCurrentIdx + i );
			_pIdxWritePtr += 3;
		}
		_nVtxCurrentIdx += ( DrawIdx_t )nVtxCount;
	}
}

void DrawList_t::AddImage( TextureID_t pTextureID, const Vector2D_t& vecMin, const Vector2D_t& vecMax, const Vector2D_t& vecUVMin, const Vector2D_t& vecUVMax, const ColorPacked_t uCol )
{
	if ( ( uCol & DRAW_COL32_A_MASK ) == 0 )
		return;

	const bool push_texture_id = _vecTextureIdStack.empty( ) || pTextureID != _vecTextureIdStack.back( );
	if ( push_texture_id )
		PushTextureID( pTextureID );

	PrimReserve( 6, 4 );
	PrimRectUV( vecMin, vecMax, vecUVMin, vecUVMax, uCol );

	if ( push_texture_id )
		PopTextureID( );
}

void DrawList_t::AddImageQuad( TextureID_t pTextureID, const Vector2D_t& vecFirst, const Vector2D_t& vecSecond, const Vector2D_t& vecThird, const Vector2D_t& vecFourth, const Vector2D_t& vecUVFirst, const Vector2D_t& vecUVSecond, const Vector2D_t& vecUVThird, const Vector2D_t& vecUVFourth, const ColorPacked_t uCol )
{
	if ( ( uCol & DRAW_COL32_A_MASK ) == 0 )
		return;

	const bool push_texture_id = _vecTextureIdStack.empty( ) || pTextureID != _vecTextureIdStack.back( );
	if ( push_texture_id )
		PushTextureID( pTextureID );

	PrimReserve( 6, 4 );
	PrimQuadUV( vecFirst, vecSecond, vecThird, vecFourth, vecUVFirst, vecUVSecond, vecUVThird, vecUVFourth, uCol );

	if ( push_texture_id )
		PopTextureID( );
}

void DrawList_t::AddImageRounded( TextureID_t pTextureID, const Vector2D_t& vecMin, const Vector2D_t& vecMax, const Vector2D_t& vecUVMin, const Vector2D_t& vecUVMax, const ColorPacked_t uCol, float flRounding, DrawFlags_t nDrawFlags )
{
	if ( ( uCol & DRAW_COL32_A_MASK ) == 0 )
		return;

	if ( flRounding <= 0.0f || ( nDrawFlags & kDrawFlags_All ) == 0 )
	{
		AddImage( pTextureID, vecMin, vecMax, vecUVMin, vecUVMax, uCol );
		return;
	}

	const bool push_texture_id = _vecTextureIdStack.empty( ) || pTextureID != _vecTextureIdStack.back( );
	if ( push_texture_id )
		PushTextureID( pTextureID );

	int iVtxStart = vecVtxBuffer.Size;
	PathRect( vecMin, vecMax, flRounding, nDrawFlags );
	PathFillConvex( uCol );
	int iVtxEnd = vecVtxBuffer.Size;
	ShadeVertsLinearUV( iVtxStart, iVtxEnd, vecMin, vecMax, vecUVMin, vecUVMax, true );

	if ( push_texture_id )
		PopTextureID( );
}

struct ShadowHelper_t
{
	static void AddSubtractedRect( DrawList_t* draw_list, const Vector2D_t& a_min, const Vector2D_t& a_max, const Vector2D_t& a_min_uv, const Vector2D_t& a_max_uv, Vector2D_t b_min, Vector2D_t b_max, const ColorPacked_t uCol )
	{
		// Early out without drawing anything if A is zero-size
		if ( ( a_min.x >= a_max.x ) || ( a_min.y >= a_max.y ) )
			return;

		// Early out without drawing anything if B covers A entirely
		if ( ( a_min.x >= b_min.x ) && ( a_max.x <= b_max.x ) && ( a_min.y >= b_min.y ) && ( a_max.y <= b_max.y ) )
			return;

		// First clip the extents of B to A
		b_min = VectorMax( b_min, a_min );
		b_max = VectorMin( b_max, a_max );

		if ( ( b_min.x >= b_max.x ) || ( b_min.y >= b_max.y ) )
		{
			// B is entirely outside A, so just draw A as-is
			draw_list->PrimReserve( 6, 4 );
			draw_list->PrimRectUV( a_min, a_max, a_min_uv, a_max_uv, uCol );
			return;
		}

		// Otherwise we need to emit (up to) four quads to cover the visible area...
		//
		// Our layout looks like this (numbers are vertex indices, letters are quads):
		//
		// 0---8------9-----1
		// |   |  B   |     |
		// +   4------5     +
		// | A |xxxxxx|  C  |
		// |   |xxxxxx|     |
		// +   7------6     +
		// |   |  D   |     |
		// 3---11-----10----2

		const int max_verts = 12;
		const int max_indices = 6 * 4; // At most four quads
		draw_list->PrimReserve( max_indices, max_verts );

		DrawIdx_t* idx_write = draw_list->_pIdxWritePtr;
		DrawVert_t* vtx_write = draw_list->_pVtxWritePtr;
		DrawIdx_t idx = ( DrawIdx_t )draw_list->_nVtxCurrentIdx;

		// Write vertices
		vtx_write[ 0 ].vecPostion = Vector2D_t( a_min.x, a_min.y );
		vtx_write[ 0 ].vecUV = Vector2D_t( a_min_uv.x, a_min_uv.y );
		vtx_write[ 0 ].uCol = uCol;
		vtx_write[ 1 ].vecPostion = Vector2D_t( a_max.x, a_min.y );
		vtx_write[ 1 ].vecUV = Vector2D_t( a_max_uv.x, a_min_uv.y );
		vtx_write[ 1 ].uCol = uCol;
		vtx_write[ 2 ].vecPostion = Vector2D_t( a_max.x, a_max.y );
		vtx_write[ 2 ].vecUV = Vector2D_t( a_max_uv.x, a_max_uv.y );
		vtx_write[ 2 ].uCol = uCol;
		vtx_write[ 3 ].vecPostion = Vector2D_t( a_min.x, a_max.y );
		vtx_write[ 3 ].vecUV = Vector2D_t( a_min_uv.x, a_max_uv.y );
		vtx_write[ 3 ].uCol = uCol;

		const Vector2D_t pos_to_uv_scale = ( a_max_uv - a_min_uv ) / ( a_max - a_min ); // Guaranteed never to be a /0 because we check for zero-size A above
		const Vector2D_t pos_to_uv_offset = ( a_min_uv / pos_to_uv_scale ) - a_min;

		// Helper that generates an interpolated UV based on position
	#define LERP_UV(x_pos, y_pos) (Vector2D_t(((x_pos) + pos_to_uv_offset.x) * pos_to_uv_scale.x, ((y_pos) + pos_to_uv_offset.y) * pos_to_uv_scale.y))
		vtx_write[ 4 ].vecPostion = Vector2D_t( b_min.x, b_min.y );
		vtx_write[ 4 ].vecUV = LERP_UV( b_min.x, b_min.y );
		vtx_write[ 4 ].uCol = uCol;
		vtx_write[ 5 ].vecPostion = Vector2D_t( b_max.x, b_min.y );
		vtx_write[ 5 ].vecUV = LERP_UV( b_max.x, b_min.y );
		vtx_write[ 5 ].uCol = uCol;
		vtx_write[ 6 ].vecPostion = Vector2D_t( b_max.x, b_max.y );
		vtx_write[ 6 ].vecUV = LERP_UV( b_max.x, b_max.y );
		vtx_write[ 6 ].uCol = uCol;
		vtx_write[ 7 ].vecPostion = Vector2D_t( b_min.x, b_max.y );
		vtx_write[ 7 ].vecUV = LERP_UV( b_min.x, b_max.y );
		vtx_write[ 7 ].uCol = uCol;
		vtx_write[ 8 ].vecPostion = Vector2D_t( b_min.x, a_min.y );
		vtx_write[ 8 ].vecUV = LERP_UV( b_min.x, a_min.y );
		vtx_write[ 8 ].uCol = uCol;
		vtx_write[ 9 ].vecPostion = Vector2D_t( b_max.x, a_min.y );
		vtx_write[ 9 ].vecUV = LERP_UV( b_max.x, a_min.y );
		vtx_write[ 9 ].uCol = uCol;
		vtx_write[ 10 ].vecPostion = Vector2D_t( b_max.x, a_max.y );
		vtx_write[ 10 ].vecUV = LERP_UV( b_max.x, a_max.y );
		vtx_write[ 10 ].uCol = uCol;
		vtx_write[ 11 ].vecPostion = Vector2D_t( b_min.x, a_max.y );
		vtx_write[ 11 ].vecUV = LERP_UV( b_min.x, a_max.y );
		vtx_write[ 11 ].uCol = uCol;
	#undef LERP_UV
		draw_list->_pVtxWritePtr += 12;
		draw_list->_nVtxCurrentIdx += 12;

		// Write indices for each quad (if it is visible)
		if ( b_min.x > a_min.x ) // A
		{
			idx_write[ 0 ] = ( DrawIdx_t )( idx + 0 );
			idx_write[ 1 ] = ( DrawIdx_t )( idx + 8 );
			idx_write[ 2 ] = ( DrawIdx_t )( idx + 11 );
			idx_write[ 3 ] = ( DrawIdx_t )( idx + 0 );
			idx_write[ 4 ] = ( DrawIdx_t )( idx + 11 );
			idx_write[ 5 ] = ( DrawIdx_t )( idx + 3 );
			idx_write += 6;
		}
		if ( b_min.y > a_min.y ) // B
		{
			idx_write[ 0 ] = ( DrawIdx_t )( idx + 8 );
			idx_write[ 1 ] = ( DrawIdx_t )( idx + 9 );
			idx_write[ 2 ] = ( DrawIdx_t )( idx + 5 );
			idx_write[ 3 ] = ( DrawIdx_t )( idx + 8 );
			idx_write[ 4 ] = ( DrawIdx_t )( idx + 5 );
			idx_write[ 5 ] = ( DrawIdx_t )( idx + 4 );
			idx_write += 6;
		}
		if ( a_max.x > b_max.x ) // C
		{
			idx_write[ 0 ] = ( DrawIdx_t )( idx + 9 );
			idx_write[ 1 ] = ( DrawIdx_t )( idx + 1 );
			idx_write[ 2 ] = ( DrawIdx_t )( idx + 2 );
			idx_write[ 3 ] = ( DrawIdx_t )( idx + 9 );
			idx_write[ 4 ] = ( DrawIdx_t )( idx + 2 );
			idx_write[ 5 ] = ( DrawIdx_t )( idx + 10 );
			idx_write += 6;
		}
		if ( a_max.y > b_max.y ) // D
		{
			idx_write[ 0 ] = ( DrawIdx_t )( idx + 7 );
			idx_write[ 1 ] = ( DrawIdx_t )( idx + 6 );
			idx_write[ 2 ] = ( DrawIdx_t )( idx + 10 );
			idx_write[ 3 ] = ( DrawIdx_t )( idx + 7 );
			idx_write[ 4 ] = ( DrawIdx_t )( idx + 10 );
			idx_write[ 5 ] = ( DrawIdx_t )( idx + 11 );
			idx_write += 6;
		}

		const int used_indices = ( int )( idx_write - draw_list->_pIdxWritePtr );
		draw_list->_pIdxWritePtr = idx_write;
		draw_list->PrimUnreserve( max_indices - used_indices, 0 );
	}

	// clip a polygonal shape to a rectangle, writing the results into dest_points. The number of points emitted is returned (may be zero if the polygon was entirely outside the rectangle, or the source polygon was not valid). dest_points may still be written to even if zero was returned.
	// allocated_dest_points should contain the number of allocated points in dest_points - in general this should be the number of source points + 4 to accommodate the worst case. If this is exceeded data will be truncated and -1 returned. Stack space work area is allocated based on this value so it shouldn't be too large.
	static int ClipPolygonShape( Vector2D_t* src_points, int num_src_points, Vector2D_t* dest_points, int allocated_dest_points, Vector2D_t clip_rect_min, Vector2D_t clip_rect_max )
	{
		// Early-out with an empty result if clipping region is zero-sized
		if ( ( clip_rect_max.x <= clip_rect_min.x ) || ( clip_rect_max.y <= clip_rect_min.y ) )
			return 0;

		// Early-out if there is no source geometry
		if ( num_src_points < 3 )
			return 0;

		// The four clip planes here are indexed as:
		// 0 = X-, 1 = X+, 2 = Y-, 3 = Y+
		std::uint8_t* outflags[ 2 ]; // Double-buffered flags for each vertex indicating which of the four clip planes it is outside of
		outflags[ 0 ] = MEM_STACKALLOC( std::uint8_t*, 2 * allocated_dest_points * sizeof( std::uint8_t ) );
		outflags[ 1 ] = outflags[ 0 ] + allocated_dest_points;

		// Calculate initial outflags
		std::uint8_t outflags_anded = 0xFF;
		std::uint8_t outflags_ored = 0;
		for ( int point_idx = 0; point_idx < num_src_points; point_idx++ )
		{
			const Vector2D_t vecPostion = src_points[ point_idx ];
			const std::uint8_t point_outflags = ( vecPostion.x < clip_rect_min.x ? 1 : 0 ) | ( vecPostion.x > clip_rect_max.x ? 2 : 0 ) | ( vecPostion.y < clip_rect_min.y ? 4 : 0 ) | ( vecPostion.y > clip_rect_max.y ? 8 : 0 );
			outflags[ 0 ][ point_idx ] = point_outflags; // Writing to buffer 0
			outflags_anded &= point_outflags;
			outflags_ored |= point_outflags;
		}
		if ( outflags_anded != 0 ) // Entirely clipped by any one plane, so nothing remains
			return 0;

		if ( outflags_ored == 0 ) // Entirely within bounds, so trivial accept
		{
			if ( allocated_dest_points < num_src_points )
				return -1; // Not sure what the caller was thinking if this happens, but we should handle it gracefully

			CRT::MemoryCopy( dest_points, src_points, num_src_points * sizeof( Vector2D_t ) );
			return num_src_points;
		}

		// Shape needs clipping
		Vector2D_t* clip_buf[ 2 ]; // Double-buffered work area
		clip_buf[ 0 ] = MEM_STACKALLOC( Vector2D_t*, 2 * allocated_dest_points * sizeof( Vector2D_t ) ); //-V630
		clip_buf[ 1 ] = clip_buf[ 0 ] + allocated_dest_points;

		CRT::MemoryCopy( clip_buf[ 0 ], src_points, num_src_points * sizeof( Vector2D_t ) );
		int clip_buf_size = num_src_points; // Number of vertices currently in the clip buffer

		int read_buffer_idx = 0; // The index of the clip buffer/out-flags we are reading (0 or 1)

		for ( int clip_plane = 0; clip_plane < 4; clip_plane++ ) // 0 = X-, 1 = X+, 2 = Y-, 3 = Y+
		{
			const int clip_plane_bit = 1 << clip_plane; // Bit mask for our current plane in out-flags
			if ( ( outflags_ored & clip_plane_bit ) == 0 )
				continue; // All vertices are inside this plane, so no need to clip

			Vector2D_t* read_vert = &clip_buf[ read_buffer_idx ][ 0 ]; // Clip buffer vertex we are currently reading
			Vector2D_t* write_vert = &clip_buf[ 1 - read_buffer_idx ][ 0 ]; // Clip buffer vertex we are currently writing
			Vector2D_t* write_vert_end = write_vert + allocated_dest_points; // End of the write buffer
			std::uint8_t* read_outflags = &outflags[ read_buffer_idx ][ 0 ]; // Out-flag we are currently reading
			std::uint8_t* write_outflags = &outflags[ 1 - read_buffer_idx ][ 0 ]; // Out-flag we are currently writing

			// Keep track of the last vertex visited, initially the last in the list
			Vector2D_t* last_vert = &read_vert[ clip_buf_size - 1 ];
			std::uint8_t last_outflags = read_outflags[ clip_buf_size - 1 ];

			for ( int vert = 0; vert < clip_buf_size; vert++ )
			{
				std::uint8_t current_outflags = *( read_outflags++ );
				bool out = ( current_outflags & clip_plane_bit ) != 0;
				if ( ( ( current_outflags ^ last_outflags ) & clip_plane_bit ) == 0 ) // We haven't crossed the clip plane
				{
					if ( !out )
					{
						// Emit vertex as-is
						if ( write_vert >= write_vert_end )
							return -1; // Ran out of buffer space, so abort
						*( write_vert++ ) = *read_vert;
						*( write_outflags++ ) = current_outflags;
					}
				}
				else
				{
					// Emit a vertex at the intersection point
					float t = 0.0f;
					Vector2D_t pos0 = *last_vert;
					Vector2D_t pos1 = *read_vert;
					Vector2D_t intersect_pos;
					switch ( clip_plane )
					{
						case 0:
							t = ( clip_rect_min.x - pos0.x ) / ( pos1.x - pos0.x );
							intersect_pos = Vector2D_t( clip_rect_min.x, pos0.y + ( ( pos1.y - pos0.y ) * t ) );
							break; // X-
						case 1:
							t = ( clip_rect_max.x - pos0.x ) / ( pos1.x - pos0.x );
							intersect_pos = Vector2D_t( clip_rect_max.x, pos0.y + ( ( pos1.y - pos0.y ) * t ) );
							break; // X+
						case 2:
							t = ( clip_rect_min.y - pos0.y ) / ( pos1.y - pos0.y );
							intersect_pos = Vector2D_t( pos0.x + ( ( pos1.x - pos0.x ) * t ), clip_rect_min.y );
							break; // Y-
						case 3:
							t = ( clip_rect_max.y - pos0.y ) / ( pos1.y - pos0.y );
							intersect_pos = Vector2D_t( pos0.x + ( ( pos1.x - pos0.x ) * t ), clip_rect_max.y );
							break; // Y+
					}

					if ( write_vert >= write_vert_end )
						return -1; // Ran out of buffer space, so abort

					// Write new out-flags for the vertex we just emitted
					*( write_vert++ ) = intersect_pos;
					*( write_outflags++ ) = ( intersect_pos.x < clip_rect_min.x ? 1 : 0 ) | ( intersect_pos.x > clip_rect_max.x ? 2 : 0 ) | ( intersect_pos.y < clip_rect_min.y ? 4 : 0 ) | ( intersect_pos.y > clip_rect_max.y ? 8 : 0 );

					if ( !out )
					{
						// When coming back in, also emit the actual vertex
						if ( write_vert >= write_vert_end )
							return -1; // Ran out of buffer space, so abort
						*( write_vert++ ) = *read_vert;
						*( write_outflags++ ) = current_outflags;
					}

					last_outflags = current_outflags;
				}

				last_vert = read_vert;
				read_vert++; // Advance to next vertex
			}

			clip_buf_size = ( int )( write_vert - &clip_buf[ 1 - read_buffer_idx ][ 0 ] ); // Update buffer size
			read_buffer_idx = 1 - read_buffer_idx; // Swap buffers
		}

		if ( clip_buf_size < 3 )
			return 0; // Nothing to return

		// Copy results to output buffer, removing any redundant vertices
		int num_out_verts = 0;
		Vector2D_t last_vert = clip_buf[ read_buffer_idx ][ clip_buf_size - 1 ];
		for ( int i = 0; i < clip_buf_size; i++ )
		{
			Vector2D_t vert = clip_buf[ read_buffer_idx ][ i ];
			if ( VectorLengthSqr( vert - last_vert ) > 0.00001f )
			{
				dest_points[ num_out_verts++ ] = vert;
				last_vert = vert;
			}
		}

		// Return size (IF this is still a valid shape)
		return ( num_out_verts > 2 ) ? num_out_verts : 0;
	}

	// Adds a rectangle (A) with a convex shape (B) subtracted from it (i.e. the portion of A covered by B is not drawn).
	static void AddSubtractedRect( DrawList_t* draw_list, const Vector2D_t& a_min, const Vector2D_t& a_max, const Vector2D_t& a_min_uv, const Vector2D_t& a_max_uv, Vector2D_t* b_points, int num_b_points, const ColorPacked_t uCol )
	{
		// Early out without drawing anything if A is zero-size
		if ( ( a_min.x >= a_max.x ) || ( a_min.y >= a_max.y ) )
			return;

		// First clip B to A
		const int max_clipped_points = num_b_points + 4;
		Vector2D_t* clipped_b_points = MEM_STACKALLOC( Vector2D_t*, max_clipped_points * sizeof( Vector2D_t ) ); //-V630
		const int num_clipped_points = ClipPolygonShape( b_points, num_b_points, clipped_b_points, max_clipped_points, a_min, a_max );
		CRT_ASSERTION( num_clipped_points >= 0 ); // -1 would indicate max_clipped_points was too small, which shouldn't happen

		b_points = clipped_b_points;
		num_b_points = num_clipped_points;

		if ( num_clipped_points == 0 )
		{
			// B is entirely outside A, so just draw A as-is
			draw_list->PrimReserve( 6, 4 );
			draw_list->PrimRectUV( a_min, a_max, a_min_uv, a_max_uv, uCol );
		}
		else
		{
			// We need to generate clipped geometry
			// To do this we walk the inner polygon and connect each edge to one of the four corners of our rectangle based on the quadrant their normal points at
			const int max_verts = num_b_points + 4; // Inner points plus the four corners
			const int max_indices = ( num_b_points * 3 ) + ( 4 * 3 ); // Worst case is one triangle per inner edge and then four filler triangles
			draw_list->PrimReserve( max_indices, max_verts );

			DrawIdx_t* idx_write = draw_list->_pIdxWritePtr;
			DrawVert_t* vtx_write = draw_list->_pVtxWritePtr;
			DrawIdx_t inner_idx = ( DrawIdx_t )draw_list->_nVtxCurrentIdx; // Starting index for inner vertices

			// Write inner vertices
			const Vector2D_t pos_to_uv_scale = ( a_max_uv - a_min_uv ) / ( a_max - a_min ); // Guaranteed never to be a /0 because we check for zero-size A above
			const Vector2D_t pos_to_uv_offset = ( a_min_uv / pos_to_uv_scale ) - a_min;

			// Helper that generates an interpolated UV based on position
		#define LERP_UV(x_pos, y_pos) (Vector2D_t(((x_pos) + pos_to_uv_offset.x) * pos_to_uv_scale.x, ((y_pos) + pos_to_uv_offset.y) * pos_to_uv_scale.y))
			for ( int i = 0; i < num_b_points; i++ )
			{
				vtx_write[ i ].vecPostion = b_points[ i ];
				vtx_write[ i ].vecUV = LERP_UV( b_points[ i ].x, b_points[ i ].y );
				vtx_write[ i ].uCol = uCol;
			}
		#undef LERP_UV

			vtx_write += num_b_points;

			// Write outer vertices
			DrawIdx_t outer_idx = ( DrawIdx_t )( inner_idx + num_b_points ); // Starting index for outer vertices

			Vector2D_t outer_verts[ 4 ];
			outer_verts[ 0 ] = Vector2D_t( a_min.x, a_min.y ); // X- Y- (quadrant 0, top left)
			outer_verts[ 1 ] = Vector2D_t( a_max.x, a_min.y ); // X+ Y- (quadrant 1, top right)
			outer_verts[ 2 ] = Vector2D_t( a_max.x, a_max.y ); // X+ Y+ (quadrant 2, bottom right)
			outer_verts[ 3 ] = Vector2D_t( a_min.x, a_max.y ); // X- Y+ (quadrant 3, bottom left)

			vtx_write[ 0 ].vecPostion = outer_verts[ 0 ];
			vtx_write[ 0 ].vecUV = Vector2D_t( a_min_uv.x, a_min_uv.y );
			vtx_write[ 0 ].uCol = uCol;
			vtx_write[ 1 ].vecPostion = outer_verts[ 1 ];
			vtx_write[ 1 ].vecUV = Vector2D_t( a_max_uv.x, a_min_uv.y );
			vtx_write[ 1 ].uCol = uCol;
			vtx_write[ 2 ].vecPostion = outer_verts[ 2 ];
			vtx_write[ 2 ].vecUV = Vector2D_t( a_max_uv.x, a_max_uv.y );
			vtx_write[ 2 ].uCol = uCol;
			vtx_write[ 3 ].vecPostion = outer_verts[ 3 ];
			vtx_write[ 3 ].vecUV = Vector2D_t( a_min_uv.x, a_max_uv.y );
			vtx_write[ 3 ].uCol = uCol;

			draw_list->_nVtxCurrentIdx += num_b_points + 4;
			draw_list->_pVtxWritePtr += num_b_points + 4;

			// Now walk the inner vertices in order
			Vector2D_t last_inner_vert = b_points[ num_b_points - 1 ];
			int last_inner_vert_idx = num_b_points - 1;
			int last_outer_vert_idx = -1;
			int first_outer_vert_idx = -1;

			// Triangle-area based check for degenerate triangles
			// Min area (0.1f) is doubled (* 2.0f) because we're calculating (area * 2) here
		#define IS_DEGENERATE(a, b, c) (M_FABS((((a).x * ((b).y - (c).y)) + ((b).x * ((c).y - (a).y)) + ((c).x * ((a).y - (b).y)))) < (0.1f * 2.0f))

					// Check the winding order of the inner vertices using the sign of the triangle area, and set the outer vertex winding to match
			int outer_vertex_winding = ( ( ( b_points[ 0 ].x * ( b_points[ 1 ].y - b_points[ 2 ].y ) ) + ( b_points[ 1 ].x * ( b_points[ 2 ].y - b_points[ 0 ].y ) ) + ( b_points[ 2 ].x * ( b_points[ 0 ].y - b_points[ 1 ].y ) ) ) < 0.0f ) ? -1 : 1;
			for ( int inner_vert_idx = 0; inner_vert_idx < num_b_points; inner_vert_idx++ )
			{
				Vector2D_t current_inner_vert = b_points[ inner_vert_idx ];

				// Calculate normal (not actually normalized, as for our purposes here it doesn't need to be)
				Vector2D_t normal( current_inner_vert.y - last_inner_vert.y, -( current_inner_vert.x - last_inner_vert.x ) );

				// Calculate the outer vertex index based on the quadrant the normal points at (0=top left, 1=top right, 2=bottom right, 3=bottom left)
				int outer_vert_idx = ( M_FABS( normal.x ) > M_FABS( normal.y ) ) ? ( ( normal.x >= 0.0f ) ? ( ( normal.y > 0.0f ) ? 2 : 1 ) : ( ( normal.y > 0.0f ) ? 3 : 0 ) ) : ( ( normal.y >= 0.0f ) ? ( ( normal.x > 0.0f ) ? 2 : 3 ) : ( ( normal.x > 0.0f ) ? 1 : 0 ) );
				Vector2D_t outer_vert = outer_verts[ outer_vert_idx ];

				// Write the main triangle (connecting the inner edge to the corner)
				if ( !IS_DEGENERATE( last_inner_vert, current_inner_vert, outer_vert ) )
				{
					idx_write[ 0 ] = ( DrawIdx_t )( inner_idx + last_inner_vert_idx );
					idx_write[ 1 ] = ( DrawIdx_t )( inner_idx + inner_vert_idx );
					idx_write[ 2 ] = ( DrawIdx_t )( outer_idx + outer_vert_idx );
					idx_write += 3;
				}

				// We don't initially know which outer vertex we are going to start from, so set that here when processing the first inner vertex
				if ( first_outer_vert_idx == -1 )
				{
					first_outer_vert_idx = outer_vert_idx;
					last_outer_vert_idx = outer_vert_idx;
				}

				// Now walk the outer edge and write any filler triangles needed (connecting outer edges to the inner vertex)
				while ( outer_vert_idx != last_outer_vert_idx )
				{
					int next_outer_vert_idx = ( last_outer_vert_idx + outer_vertex_winding ) & 3;
					if ( !IS_DEGENERATE( outer_verts[ last_outer_vert_idx ], outer_verts[ next_outer_vert_idx ], last_inner_vert ) )
					{
						idx_write[ 0 ] = ( DrawIdx_t )( outer_idx + last_outer_vert_idx );
						idx_write[ 1 ] = ( DrawIdx_t )( outer_idx + next_outer_vert_idx );
						idx_write[ 2 ] = ( DrawIdx_t )( inner_idx + last_inner_vert_idx );
						idx_write += 3;
					}
					last_outer_vert_idx = next_outer_vert_idx;
				}

				last_inner_vert = current_inner_vert;
				last_inner_vert_idx = inner_vert_idx;
			}

			// Write remaining filler triangles for any un-traversed outer edges
			if ( first_outer_vert_idx != -1 )
			{
				while ( first_outer_vert_idx != last_outer_vert_idx )
				{
					int next_outer_vert_idx = ( last_outer_vert_idx + outer_vertex_winding ) & 3;
					if ( !IS_DEGENERATE( outer_verts[ last_outer_vert_idx ], outer_verts[ next_outer_vert_idx ], last_inner_vert ) )
					{
						idx_write[ 0 ] = ( DrawIdx_t )( outer_idx + last_outer_vert_idx );
						idx_write[ 1 ] = ( DrawIdx_t )( outer_idx + next_outer_vert_idx );
						idx_write[ 2 ] = ( DrawIdx_t )( inner_idx + last_inner_vert_idx );
						idx_write += 3;
					}
					last_outer_vert_idx = next_outer_vert_idx;
				}
			}
		#undef IS_DEGENERATE

			int used_indices = ( int )( idx_write - draw_list->_pIdxWritePtr );
			draw_list->_pIdxWritePtr = idx_write;
			draw_list->PrimUnreserve( max_indices - used_indices, 0 );
		}
	}
};

void DrawList_t::AddShadowRect( const Vector2D_t& p_min, const Vector2D_t& p_max, float shadow_thickness, const Vector2D_t& offset, const ColorPacked_t uCol, float rounding, DrawFlags_t rounding_corners )
{
	if ( ( uCol & DRAW_COL32_A_MASK ) == 0 )
		return;

	Vector2D_t* inner_rect_points = NULL; // Points that make up the shape of the inner rectangle (used when it has rounded corners)
	int num_inner_rect_points = 0;

	// Generate a path describing the inner rectangle and copy it to our buffer
	const bool is_rounded = ( rounding > 0.0f ) && ( rounding_corners != kDrawFlags_None ); // Do we have rounded corners?
	if ( is_rounded )
	{
		_vecPath.Size = 0;
		PathRect( p_min, p_max, rounding, rounding_corners );
		num_inner_rect_points = _vecPath.Size;
		inner_rect_points = MEM_STACKALLOC( Vector2D_t*, num_inner_rect_points * sizeof( Vector2D_t ) ); //-V630
		CRT::MemoryCopy( inner_rect_points, _vecPath.Data, num_inner_rect_points * sizeof( Vector2D_t ) );
		_vecPath.Size = 0;
	}

	// Draw the relevant chunks of the texture (the texture is split into a 3x3 grid)
	for ( int x = 0; x < 3; x++ )
	{
		for ( int y = 0; y < 3; y++ )
		{
			const int uv_index = x + ( y + y + y ); // y*3 formatted so as to ensure the compiler avoids an actual multiply
			const Vector4D_t uvs = _pData->ShadowRectUvs[ uv_index ];

			Vector2D_t draw_min, draw_max;
			switch ( x )
			{
				case 0:
					draw_min.x = p_min.x - shadow_thickness;
					draw_max.x = p_min.x;
					break;
				case 1:
					draw_min.x = p_min.x;
					draw_max.x = p_max.x;
					break;
				case 2:
					draw_min.x = p_max.x;
					draw_max.x = p_max.x + shadow_thickness;
					break;
			}
			switch ( y )
			{
				case 0:
					draw_min.y = p_min.y - shadow_thickness;
					draw_max.y = p_min.y;
					break;
				case 1:
					draw_min.y = p_min.y;
					draw_max.y = p_max.y;
					break;
				case 2:
					draw_min.y = p_max.y;
					draw_max.y = p_max.y + shadow_thickness;
					break;
			}

			Vector2D_t uv_min( uvs.x, uvs.y );
			Vector2D_t uv_max( uvs.z, uvs.w );
			if ( is_rounded )
				ShadowHelper_t::AddSubtractedRect( this, draw_min + offset, draw_max + offset, uv_min, uv_max, inner_rect_points, num_inner_rect_points, uCol ); // Complex path for rounded rectangles
			else
				ShadowHelper_t::AddSubtractedRect( this, draw_min + offset, draw_max + offset, uv_min, uv_max, p_min, p_max, uCol ); // Simple fast path for non-rounded rectangles
		}
	}
}

void DrawList_t::PathArcTo( const Vector2D_t& vecCenter, float flRadius, float a_min, float a_max, int iSegments )
{
	if ( flRadius == 0.0f )
	{
		_vecPath.push_back( vecCenter );
		return;
	}

	// Note that we are adding a point at both a_min and a_max.
	// If you are trying to draw a full bClosed circle you don't want the overlapping pPoints!
	_vecPath.reserve( _vecPath.Size + ( iSegments + 1 ) );
	for ( int i = 0; i <= iSegments; i++ )
	{
		const float a = a_min + ( ( float )i / ( float )iSegments ) * ( a_max - a_min );
		_vecPath.push_back( Vector2D_t( vecCenter.x + M_COS( a ) * flRadius, vecCenter.y + M_SIN( a ) * flRadius ) );
	}
}

void DrawList_t::PathArcToFast( const Vector2D_t& vecCenter, float flRadius, int a_min_of_12, int a_max_of_12 )
{
	if ( flRadius == 0.0f || a_min_of_12 > a_max_of_12 )
	{
		_vecPath.push_back( vecCenter );
		return;
	}
	_vecPath.reserve( _vecPath.Size + ( a_max_of_12 - a_min_of_12 + 1 ) );
	for ( int a = a_min_of_12; a <= a_max_of_12; a++ )
	{
		const Vector2D_t& c = _pData->arrCircleVtx12[ a % CRT_ARRAYSIZE( _pData->arrCircleVtx12 ) ];
		_vecPath.push_back( Vector2D_t( vecCenter.x + c.x * flRadius, vecCenter.y + c.y * flRadius ) );
	}
}

struct BezierHelper_t
{
	static Vector2D_t DrawBezierCalc( const Vector2D_t& vecFirst, const Vector2D_t& vecSecond, const Vector2D_t& vecThird, const Vector2D_t& vecFourth, float t )
	{
		float u = 1.0f - t;
		float w1 = u * u * u;
		float w2 = 3 * u * u * t;
		float w3 = 3 * u * t * t;
		float w4 = t * t * t;
		return Vector2D_t( w1 * vecFirst.x + w2 * vecSecond.x + w3 * vecThird.x + w4 * vecFourth.x, w1 * vecFirst.y + w2 * vecSecond.y + w3 * vecThird.y + w4 * vecFourth.y );
	}

	// Closely mimics BezierClosestPointCasteljauStep() in imgui.cpp
	static void PathBezierToCasteljau( CRT::Vector<Vector2D_t>* path, float x1, float y1, float x2, float y2, float x3, float y3, float x4, float y4, float tess_tol, int level )
	{
		float dx = x4 - x1;
		float dy = y4 - y1;
		float d2 = ( ( x2 - x4 ) * dy - ( y2 - y4 ) * dx );
		float d3 = ( ( x3 - x4 ) * dy - ( y3 - y4 ) * dx );
		d2 = ( d2 >= 0 ) ? d2 : -d2;
		d3 = ( d3 >= 0 ) ? d3 : -d3;
		if ( ( d2 + d3 ) * ( d2 + d3 ) < tess_tol * ( dx * dx + dy * dy ) )
		{
			path->push_back( Vector2D_t( x4, y4 ) );
		}
		else if ( level < 10 )
		{
			float x12 = ( x1 + x2 ) * 0.5f, y12 = ( y1 + y2 ) * 0.5f;
			float x23 = ( x2 + x3 ) * 0.5f, y23 = ( y2 + y3 ) * 0.5f;
			float x34 = ( x3 + x4 ) * 0.5f, y34 = ( y3 + y4 ) * 0.5f;
			float x123 = ( x12 + x23 ) * 0.5f, y123 = ( y12 + y23 ) * 0.5f;
			float x234 = ( x23 + x34 ) * 0.5f, y234 = ( y23 + y34 ) * 0.5f;
			float x1234 = ( x123 + x234 ) * 0.5f, y1234 = ( y123 + y234 ) * 0.5f;
			PathBezierToCasteljau( path, x1, y1, x12, y12, x123, y123, x1234, y1234, tess_tol, level + 1 );
			PathBezierToCasteljau( path, x1234, y1234, x234, y234, x34, y34, x4, y4, tess_tol, level + 1 );
		}
	}
};

void DrawList_t::PathBezierCurveTo( const Vector2D_t& vecSecond, const Vector2D_t& vecThird, const Vector2D_t& vecFourth, int iSegments )
{
	Vector2D_t vecFirst = _vecPath.back( );
	if ( iSegments == 0 )
	{
		BezierHelper_t::PathBezierToCasteljau( &_vecPath, vecFirst.x, vecFirst.y, vecSecond.x, vecSecond.y, vecThird.x, vecThird.y, vecFourth.x, vecFourth.y, _pData->flCurveTessellationTol, 0 ); // Auto-tessellated
	}
	else
	{
		float t_step = 1.0f / ( float )iSegments;
		for ( int i_step = 1; i_step <= iSegments; i_step++ )
			_vecPath.push_back( BezierHelper_t::DrawBezierCalc( vecFirst, vecSecond, vecThird, vecFourth, t_step * i_step ) );
	}
}

void DrawList_t::PathRect( const Vector2D_t& a, const Vector2D_t& b, float flRounding, DrawFlags_t nDrawFlags )
{
	flRounding = CRT::Min( flRounding, M_FABS( b.x - a.x ) * ( ( ( nDrawFlags & kDrawFlags_Top ) == kDrawFlags_Top ) || ( ( nDrawFlags & kDrawFlags_Bot ) == kDrawFlags_Bot ) ? 0.5f : 1.0f ) - 1.0f );
	flRounding = CRT::Min( flRounding, M_FABS( b.y - a.y ) * ( ( ( nDrawFlags & kDrawFlags_Left ) == kDrawFlags_Left ) || ( ( nDrawFlags & kDrawFlags_Right ) == kDrawFlags_Right ) ? 0.5f : 1.0f ) - 1.0f );

	if ( flRounding <= 0.0f || nDrawFlags == 0 )
	{
		PathLineTo( a );
		PathLineTo( Vector2D_t( b.x, a.y ) );
		PathLineTo( b );
		PathLineTo( Vector2D_t( a.x, b.y ) );
	}
	else
	{
		const float rounding_tl = ( nDrawFlags & kDrawFlags_TopLeft ) ? flRounding : 0.0f;
		const float rounding_tr = ( nDrawFlags & kDrawFlags_TopRight ) ? flRounding : 0.0f;
		const float rounding_br = ( nDrawFlags & kDrawFlags_BotRight ) ? flRounding : 0.0f;
		const float rounding_bl = ( nDrawFlags & kDrawFlags_BotLeft ) ? flRounding : 0.0f;
		PathArcToFast( Vector2D_t( a.x + rounding_tl, a.y + rounding_tl ), rounding_tl, 6, 9 );
		PathArcToFast( Vector2D_t( b.x - rounding_tr, a.y + rounding_tr ), rounding_tr, 9, 12 );
		PathArcToFast( Vector2D_t( b.x - rounding_br, b.y - rounding_br ), rounding_br, 0, 3 );
		PathArcToFast( Vector2D_t( a.x + rounding_bl, b.y - rounding_bl ), rounding_bl, 3, 6 );
	}
}

void DrawList_t::AddCallback( DrawCallback_t fnCallback, void* pData )
{
	DrawCmd_t* current_cmd = vecCmdBuffer.Size ? &vecCmdBuffer.back( ) : NULL;
	if ( current_cmd == NULL || current_cmd->nElemCount != 0 || current_cmd->fnUserCallback != NULL )
	{
		AddDrawCmd( );
		current_cmd = &vecCmdBuffer.back( );
	}
	current_cmd->fnUserCallback = fnCallback;
	current_cmd->pUserCallbackData = pData;

	AddDrawCmd( );
}

void DrawList_t::AddDrawCmd( )
{
	DrawCmd_t draw_cmd;
	draw_cmd.vecClipRect = GetCurrentClipRect( );
	draw_cmd.pTextureId = GetCurrentTextureId( );
	draw_cmd.nVtxOffset = _nVtxCurrentOffset;
	draw_cmd.nIdxOffset = vecIdxBuffer.Size;

	CRT_ASSERTION( draw_cmd.vecClipRect.x <= draw_cmd.vecClipRect.z && draw_cmd.vecClipRect.y <= draw_cmd.vecClipRect.w );
	vecCmdBuffer.push_back( draw_cmd );
}

DrawList_t* DrawList_t::CloneOutput( ) const
{
	DrawList_t* dst = MEM_NEW( DrawList_t( _pData ) );
	dst->vecCmdBuffer = vecCmdBuffer;
	dst->vecIdxBuffer = vecIdxBuffer;
	dst->vecVtxBuffer = vecVtxBuffer;
	dst->nFlags = nFlags;
	return dst;
}

void DrawList_t::ShadeVertsLinearColorGradientKeepAlpha( int iVtxStart, int iVtxEnd, Vector2D_t vecGradient0, Vector2D_t vecGradient1, ColorPacked_t uColPrimary, ColorPacked_t uColSecondary )
{
	Vector2D_t gradient_extent = vecGradient1 - vecGradient0;
	float gradient_inv_length2 = 1.0f / VectorLengthSqr( gradient_extent );
	DrawVert_t* vert_start = vecVtxBuffer.Data + iVtxStart;
	DrawVert_t* vert_end = vecVtxBuffer.Data + iVtxEnd;
	for ( DrawVert_t* vert = vert_start; vert < vert_end; vert++ )
	{
		float d = VectorDot( vert->vecPostion - vecGradient0, gradient_extent );
		float t = CRT::Clamp( d * gradient_inv_length2, 0.0f, 1.0f );
		int r = static_cast< int >( M_LERP( static_cast< int >( uColPrimary >> DRAW_COL32_R_SHIFT ) & 0xFF, static_cast< int >( uColSecondary >> DRAW_COL32_R_SHIFT ) & 0xFF, t ) );
		int g = static_cast< int >( M_LERP( static_cast< int >( uColPrimary >> DRAW_COL32_G_SHIFT ) & 0xFF, static_cast< int >( uColSecondary >> DRAW_COL32_G_SHIFT ) & 0xFF, t ) );
		int b = static_cast< int >( M_LERP( static_cast< int >( uColPrimary >> DRAW_COL32_B_SHIFT ) & 0xFF, static_cast< int >( uColSecondary >> DRAW_COL32_B_SHIFT ) & 0xFF, t ) );
		vert->uCol = ( r << DRAW_COL32_R_SHIFT ) | ( g << DRAW_COL32_G_SHIFT ) | ( b << DRAW_COL32_B_SHIFT ) | ( vert->uCol & DRAW_COL32_A_MASK );
	}
}

void DrawList_t::ShadeVertsLinearUV( int iVtxStart, int iVtxEnd, const Vector2D_t& vecStart, const Vector2D_t& vecEnd, const Vector2D_t& vecUvStart, const Vector2D_t& vecUvEnd, bool bClamp )
{
	const Vector2D_t size = vecEnd - vecStart;
	const Vector2D_t uv_size = vecUvEnd - vecUvStart;
	const Vector2D_t flScale = Vector2D_t(
		size.x != 0.0f ? ( uv_size.x / size.x ) : 0.0f,
		size.y != 0.0f ? ( uv_size.y / size.y ) : 0.0f );

	DrawVert_t* vert_start = vecVtxBuffer.Data + iVtxStart;
	DrawVert_t* vert_end = vecVtxBuffer.Data + iVtxEnd;
	if ( bClamp )
	{
		const Vector2D_t min = VectorMin( vecUvStart, vecUvEnd );
		const Vector2D_t max = VectorMax( vecUvStart, vecUvEnd );
		for ( DrawVert_t* vertex = vert_start; vertex < vert_end; ++vertex )
			vertex->vecUV = VectorClamp( vecUvStart + VectorMul( Vector2D_t( vertex->vecPostion.x, vertex->vecPostion.y ) - vecStart, flScale ), min, max );
	}
	else
	{
		for ( DrawVert_t* vertex = vert_start; vertex < vert_end; ++vertex )
			vertex->vecUV = vecUvStart + VectorMul( Vector2D_t( vertex->vecPostion.x, vertex->vecPostion.y ) - vecStart, flScale );
	}
}

void DrawList_t::Clear( )
{
	vecCmdBuffer.resize( 0 );
	vecIdxBuffer.resize( 0 );
	vecVtxBuffer.resize( 0 );
	nFlags = _pData ? _pData->nInitialFlags : kDrawListFlags_None;
	_nVtxCurrentOffset = 0;
	_nVtxCurrentIdx = 0;
	_pVtxWritePtr = NULL;
	_pIdxWritePtr = NULL;
	_vecClipRectStack.resize( 0 );
	_vecTextureIdStack.resize( 0 );
	_vecPath.resize( 0 );
	_drawSplitter.Clear( );
}

void DrawList_t::ClearFreeMemory( )
{
	vecCmdBuffer.clear( );
	vecIdxBuffer.clear( );
	vecVtxBuffer.clear( );
	_nVtxCurrentIdx = 0;
	_pVtxWritePtr = NULL;
	_pIdxWritePtr = NULL;
	_vecClipRectStack.clear( );
	_vecTextureIdStack.clear( );
	_vecPath.clear( );
	_drawSplitter.ClearFreeMemory( );
}

void DrawList_t::PrimReserve( int nIdxCount, int nVtxCount )
{
#ifdef _DEBUG
	// Large mesh support (when enabled)
	CRT_ASSERTION( nIdxCount >= 0 && nVtxCount >= 0 );
#endif

	if ( sizeof( DrawIdx_t ) == 2 && ( _nVtxCurrentIdx + nVtxCount >= ( 1 << 16 ) ) && ( nFlags & kDrawListFlags_AllowVtxOffset ) )
	{
		_nVtxCurrentOffset = vecVtxBuffer.Size;
		_nVtxCurrentIdx = 0;
		AddDrawCmd( );
	}

	DrawCmd_t& draw_cmd = vecCmdBuffer.Data[ vecCmdBuffer.Size - 1 ];
	draw_cmd.nElemCount += nIdxCount;

	int vtx_buffer_old_size = vecVtxBuffer.Size;
	vecVtxBuffer.resize( vtx_buffer_old_size + nVtxCount );
	_pVtxWritePtr = vecVtxBuffer.Data + vtx_buffer_old_size;

	int idx_buffer_old_size = vecIdxBuffer.Size;
	vecIdxBuffer.resize( idx_buffer_old_size + nIdxCount );
	_pIdxWritePtr = vecIdxBuffer.Data + idx_buffer_old_size;
}

void DrawList_t::PrimUnreserve( int nIdxCount, int nVtxCount )
{
#ifdef _DEBUG
	CRT_ASSERTION( nIdxCount >= 0 && nVtxCount >= 0 );
#endif

	DrawCmd_t& draw_cmd = vecCmdBuffer.Data[ vecCmdBuffer.Size - 1 ];
	draw_cmd.nElemCount -= nIdxCount;
	vecVtxBuffer.shrink( vecVtxBuffer.Size - nVtxCount );
	vecIdxBuffer.shrink( vecIdxBuffer.Size - nIdxCount );
}

void DrawList_t::PrimRect( const Vector2D_t& a, const Vector2D_t& c, const ColorPacked_t uCol )
{
	Vector2D_t b( c.x, a.y ), d( a.x, c.y ), vecUV( _pData->vecTexUvWhitePixel );
	DrawIdx_t idx = ( DrawIdx_t )_nVtxCurrentIdx;
	_pIdxWritePtr[ 0 ] = idx;
	_pIdxWritePtr[ 1 ] = ( DrawIdx_t )( idx + 1 );
	_pIdxWritePtr[ 2 ] = ( DrawIdx_t )( idx + 2 );
	_pIdxWritePtr[ 3 ] = idx;
	_pIdxWritePtr[ 4 ] = ( DrawIdx_t )( idx + 2 );
	_pIdxWritePtr[ 5 ] = ( DrawIdx_t )( idx + 3 );
	_pVtxWritePtr[ 0 ].vecPostion = a;
	_pVtxWritePtr[ 0 ].vecUV = vecUV;
	_pVtxWritePtr[ 0 ].uCol = uCol;
	_pVtxWritePtr[ 1 ].vecPostion = b;
	_pVtxWritePtr[ 1 ].vecUV = vecUV;
	_pVtxWritePtr[ 1 ].uCol = uCol;
	_pVtxWritePtr[ 2 ].vecPostion = c;
	_pVtxWritePtr[ 2 ].vecUV = vecUV;
	_pVtxWritePtr[ 2 ].uCol = uCol;
	_pVtxWritePtr[ 3 ].vecPostion = d;
	_pVtxWritePtr[ 3 ].vecUV = vecUV;
	_pVtxWritePtr[ 3 ].uCol = uCol;
	_pVtxWritePtr += 4;
	_nVtxCurrentIdx += 4;
	_pIdxWritePtr += 6;
}

void DrawList_t::PrimRectUV( const Vector2D_t& a, const Vector2D_t& c, const Vector2D_t& uv_a, const Vector2D_t& uv_c, const ColorPacked_t uCol )
{
	Vector2D_t b( c.x, a.y ), d( a.x, c.y ), uv_b( uv_c.x, uv_a.y ), uv_d( uv_a.x, uv_c.y );
	DrawIdx_t idx = ( DrawIdx_t )_nVtxCurrentIdx;
	_pIdxWritePtr[ 0 ] = idx;
	_pIdxWritePtr[ 1 ] = ( DrawIdx_t )( idx + 1 );
	_pIdxWritePtr[ 2 ] = ( DrawIdx_t )( idx + 2 );
	_pIdxWritePtr[ 3 ] = idx;
	_pIdxWritePtr[ 4 ] = ( DrawIdx_t )( idx + 2 );
	_pIdxWritePtr[ 5 ] = ( DrawIdx_t )( idx + 3 );
	_pVtxWritePtr[ 0 ].vecPostion = a;
	_pVtxWritePtr[ 0 ].vecUV = uv_a;
	_pVtxWritePtr[ 0 ].uCol = uCol;
	_pVtxWritePtr[ 1 ].vecPostion = b;
	_pVtxWritePtr[ 1 ].vecUV = uv_b;
	_pVtxWritePtr[ 1 ].uCol = uCol;
	_pVtxWritePtr[ 2 ].vecPostion = c;
	_pVtxWritePtr[ 2 ].vecUV = uv_c;
	_pVtxWritePtr[ 2 ].uCol = uCol;
	_pVtxWritePtr[ 3 ].vecPostion = d;
	_pVtxWritePtr[ 3 ].vecUV = uv_d;
	_pVtxWritePtr[ 3 ].uCol = uCol;
	_pVtxWritePtr += 4;
	_nVtxCurrentIdx += 4;
	_pIdxWritePtr += 6;
}

void DrawList_t::PrimQuadUV( const Vector2D_t& a, const Vector2D_t& b, const Vector2D_t& c, const Vector2D_t& d, const Vector2D_t& uv_a, const Vector2D_t& uv_b, const Vector2D_t& uv_c, const Vector2D_t& uv_d, const ColorPacked_t uCol )
{
	DrawIdx_t idx = ( DrawIdx_t )_nVtxCurrentIdx;
	_pIdxWritePtr[ 0 ] = idx;
	_pIdxWritePtr[ 1 ] = ( DrawIdx_t )( idx + 1 );
	_pIdxWritePtr[ 2 ] = ( DrawIdx_t )( idx + 2 );
	_pIdxWritePtr[ 3 ] = idx;
	_pIdxWritePtr[ 4 ] = ( DrawIdx_t )( idx + 2 );
	_pIdxWritePtr[ 5 ] = ( DrawIdx_t )( idx + 3 );
	_pVtxWritePtr[ 0 ].vecPostion = a;
	_pVtxWritePtr[ 0 ].vecUV = uv_a;
	_pVtxWritePtr[ 0 ].uCol = uCol;
	_pVtxWritePtr[ 1 ].vecPostion = b;
	_pVtxWritePtr[ 1 ].vecUV = uv_b;
	_pVtxWritePtr[ 1 ].uCol = uCol;
	_pVtxWritePtr[ 2 ].vecPostion = c;
	_pVtxWritePtr[ 2 ].vecUV = uv_c;
	_pVtxWritePtr[ 2 ].uCol = uCol;
	_pVtxWritePtr[ 3 ].vecPostion = d;
	_pVtxWritePtr[ 3 ].vecUV = uv_d;
	_pVtxWritePtr[ 3 ].uCol = uCol;
	_pVtxWritePtr += 4;
	_nVtxCurrentIdx += 4;
	_pIdxWritePtr += 6;
}

void DrawList_t::UpdateClipRect( )
{ // If current command is used with different settings we need to add a new command
	const Vector4D_t curr_clip_rect = GetCurrentClipRect( );
	DrawCmd_t* curr_cmd = vecCmdBuffer.Size > 0 ? &vecCmdBuffer.Data[ vecCmdBuffer.Size - 1 ] : NULL;
	if ( !curr_cmd || ( curr_cmd->nElemCount != 0 && CRT::MemoryCompare( &curr_cmd->vecClipRect, &curr_clip_rect, sizeof( Vector4D_t ) ) != 0 ) || curr_cmd->fnUserCallback != NULL )
	{
		AddDrawCmd( );
		return;
	}

	// Try to merge with previous command if it matches, else use current command
	DrawCmd_t* prev_cmd = vecCmdBuffer.Size > 1 ? curr_cmd - 1 : NULL;
	if ( curr_cmd->nElemCount == 0 && prev_cmd && CRT::MemoryCompare( &prev_cmd->vecClipRect, &curr_clip_rect, sizeof( Vector4D_t ) ) == 0 && prev_cmd->pTextureId == GetCurrentTextureId( ) && prev_cmd->fnUserCallback == NULL )
		vecCmdBuffer.pop_back( );
	else
		curr_cmd->vecClipRect = curr_clip_rect;
}

void DrawList_t::UpdateTextureID( )
{
	// If current command is used with different settings we need to add a new command
	const TextureID_t curr_texture_id = GetCurrentTextureId( );
	DrawCmd_t* curr_cmd = vecCmdBuffer.Size ? &vecCmdBuffer.back( ) : NULL;
	if ( !curr_cmd || ( curr_cmd->nElemCount != 0 && curr_cmd->pTextureId != curr_texture_id ) || curr_cmd->fnUserCallback != NULL )
	{
		AddDrawCmd( );
		return;
	}

	// Try to merge with previous command if it matches, else use current command
	DrawCmd_t* prev_cmd = vecCmdBuffer.Size > 1 ? curr_cmd - 1 : NULL;
	if ( curr_cmd->nElemCount == 0 && prev_cmd && prev_cmd->pTextureId == curr_texture_id && CRT::MemoryCompare( &prev_cmd->vecClipRect, &GetCurrentClipRect( ), sizeof( Vector4D_t ) ) == 0 && prev_cmd->fnUserCallback == NULL )
		vecCmdBuffer.pop_back( );
	else
		curr_cmd->pTextureId = curr_texture_id;
}

#pragma endregion

#pragma region draw_list_utility

void DrawListSplitter_t::ClearFreeMemory( )
{
	for ( size_t i = 0; i < _vecChannels.Size; i++ )
	{
		if ( i == _iCurrent )
			CRT::MemorySet( &_vecChannels[ i ], 0, sizeof( _vecChannels[ i ] ) ); // Current channel is a copy of vecCmdBuffer/vecIdxBuffer, don't destruct again
		_vecChannels[ i ]._vecCmdBuffer.clear( );
		_vecChannels[ i ]._vecIdxBuffer.clear( );
	}
	_iCurrent = 0;
	_iCount = 1;
	_vecChannels.clear( );
}

void DrawListSplitter_t::Split( DrawList_t* pDrawList, int channels_count )
{
	CRT_ASSERTION( _iCurrent == 0 && _iCount <= 1 && "Nested channel splitting is not supported. Please use separate instances of ImDrawListSplitter." );
	int old_channels_count = _vecChannels.Size;
	if ( old_channels_count < channels_count )
		_vecChannels.resize( channels_count );
	_iCount = channels_count;

	// Channels[] (24/32 bytes each) hold storage that we'll swap with pDrawList->_vecCmdBuffer/_vecIdxBuffer
	// The content of Channels[0] at this point doesn't matter. We clear it to make state tidy in a debugger but we don't strictly need to.
	// When we switch to the next channel, we'll copy pDrawList->_vecCmdBuffer/_vecIdxBuffer into Channels[0] and then Channels[1] into pDrawList->vecCmdBuffer/_vecIdxBuffer
	CRT::MemorySet( &_vecChannels[ 0 ], 0, sizeof( DrawChannel_t ) );
	for ( int i = 1; i < channels_count; i++ )
	{
		if ( i >= old_channels_count )
		{
			MEM_PLACEMENT_NEW( &_vecChannels[ i ] )
				DrawChannel_t( );
		}
		else
		{
			_vecChannels[ i ]._vecCmdBuffer.resize( 0 );
			_vecChannels[ i ]._vecIdxBuffer.resize( 0 );
		}
		if ( _vecChannels[ i ]._vecCmdBuffer.Size == 0 )
		{
			DrawCmd_t draw_cmd;
			draw_cmd.vecClipRect = pDrawList->_vecClipRectStack.back( );
			draw_cmd.pTextureId = pDrawList->_vecTextureIdStack.back( );
			_vecChannels[ i ]._vecCmdBuffer.push_back( draw_cmd );
		}
	}
}

void DrawListSplitter_t::Merge( DrawList_t* pDrawList )
{
	struct MergeDrawCmd_t
	{
		static bool CanMerge( DrawCmd_t* a, DrawCmd_t* b )
		{
			return CRT::MemoryCompare( &a->vecClipRect, &b->vecClipRect, sizeof( a->vecClipRect ) ) == 0 && a->pTextureId == b->pTextureId && a->nVtxOffset == b->nVtxOffset && !a->fnUserCallback && !b->fnUserCallback;
		}
	};

	// Note that we never use or rely on channels.Size because it is merely a buffer that we never shrink back to 0 to keep all sub-buffers ready for use.
	if ( _iCount <= 1 )
		return;

	SetCurrentChannel( pDrawList, 0 );
	if ( pDrawList->vecCmdBuffer.Size != 0 && pDrawList->vecCmdBuffer.back( ).nElemCount == 0 )
		pDrawList->vecCmdBuffer.pop_back( );

	// Calculate our final buffer sizes. Also fix the incorrect nIdxOffset values in each command.
	int new_cmd_buffer_count = 0;
	int new_idx_buffer_count = 0;
	DrawCmd_t* last_cmd = ( _iCount > 0 && pDrawList->vecCmdBuffer.Size > 0 ) ? &pDrawList->vecCmdBuffer.back( ) : NULL;
	int idx_offset = last_cmd ? last_cmd->nIdxOffset + last_cmd->nElemCount : 0;
	for ( int i = 1; i < _iCount; i++ )
	{
		DrawChannel_t& ch = _vecChannels[ i ];
		if ( ch._vecCmdBuffer.Size > 0 && ch._vecCmdBuffer.back( ).nElemCount == 0 )
			ch._vecCmdBuffer.pop_back( );
		if ( ch._vecCmdBuffer.Size > 0 && last_cmd != NULL && MergeDrawCmd_t::CanMerge( last_cmd, &ch._vecCmdBuffer[ 0 ] ) )
		{
			// Merge previous channel last draw command with current channel first draw command if matching.
			last_cmd->nElemCount += ch._vecCmdBuffer[ 0 ].nElemCount;
			idx_offset += ch._vecCmdBuffer[ 0 ].nElemCount;
			ch._vecCmdBuffer.erase( ch._vecCmdBuffer.Data ); // FIXME-OPT: Improve for multiple merges.
		}
		if ( ch._vecCmdBuffer.Size > 0 )
			last_cmd = &ch._vecCmdBuffer.back( );
		new_cmd_buffer_count += ch._vecCmdBuffer.Size;
		new_idx_buffer_count += ch._vecIdxBuffer.Size;
		for ( size_t cmd_n = 0; cmd_n < ch._vecCmdBuffer.Size; cmd_n++ )
		{
			ch._vecCmdBuffer.Data[ cmd_n ].nIdxOffset = idx_offset;
			idx_offset += ch._vecCmdBuffer.Data[ cmd_n ].nElemCount;
		}
	}
	pDrawList->vecCmdBuffer.resize( pDrawList->vecCmdBuffer.Size + new_cmd_buffer_count );
	pDrawList->vecIdxBuffer.resize( pDrawList->vecIdxBuffer.Size + new_idx_buffer_count );

	// Write commands and indices in order (they are fairly small structures, we don't copy vertices only indices)
	DrawCmd_t* cmd_write = pDrawList->vecCmdBuffer.Data + pDrawList->vecCmdBuffer.Size - new_cmd_buffer_count;
	DrawIdx_t* idx_write = pDrawList->vecIdxBuffer.Data + pDrawList->vecIdxBuffer.Size - new_idx_buffer_count;
	for ( int i = 1; i < _iCount; i++ )
	{
		DrawChannel_t& ch = _vecChannels[ i ];
		if ( int sz = ch._vecCmdBuffer.Size )
		{
			CRT::MemoryCopy( cmd_write, ch._vecCmdBuffer.Data, sz * sizeof( DrawCmd_t ) );
			cmd_write += sz;
		}
		if ( int sz = ch._vecIdxBuffer.Size )
		{
			CRT::MemoryCopy( idx_write, ch._vecIdxBuffer.Data, sz * sizeof( DrawIdx_t ) );
			idx_write += sz;
		}
	}
	pDrawList->_pIdxWritePtr = idx_write;
	pDrawList->UpdateClipRect( ); // We call this instead of AddDrawCmd(), so that empty channels won't produce an extra draw call.
	pDrawList->UpdateTextureID( );
	_iCount = 1;
}

void DrawListSplitter_t::SetCurrentChannel( DrawList_t* pDrawList, int idx )
{
	CRT_ASSERTION( idx >= 0 && idx < _iCount );
	if ( _iCurrent == idx )
		return;
	// Overwrite ImVector (12/16 bytes), four times. This is merely a silly optimization instead of doing .swap()
	CRT::MemoryCopy( &_vecChannels.Data[ _iCurrent ]._vecCmdBuffer, &pDrawList->vecCmdBuffer, sizeof( pDrawList->vecCmdBuffer ) );
	CRT::MemoryCopy( &_vecChannels.Data[ _iCurrent ]._vecIdxBuffer, &pDrawList->vecIdxBuffer, sizeof( pDrawList->vecIdxBuffer ) );
	_iCurrent = idx;
	CRT::MemoryCopy( &pDrawList->vecCmdBuffer, &_vecChannels.Data[ idx ]._vecCmdBuffer, sizeof( pDrawList->vecCmdBuffer ) );
	CRT::MemoryCopy( &pDrawList->vecIdxBuffer, &_vecChannels.Data[ idx ]._vecIdxBuffer, sizeof( pDrawList->vecIdxBuffer ) );
	pDrawList->_pIdxWritePtr = pDrawList->vecIdxBuffer.Data + pDrawList->vecIdxBuffer.Size;
}

#pragma endregion
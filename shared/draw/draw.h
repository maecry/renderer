#pragma once

#include "../datatypes/vector.h"
#include "../datatypes/color.h"

// used: clogger
#include "../utilities/logger.h"

#ifndef DRAW_API
#define DRAW_API
#endif
#ifndef DRAW_IMPL_API
#define DRAW_IMPL_API DRAW_API
#endif

#define DRAW_UNICODE_CODEPOINT_MAX 0xFFFF
#define DRAW_UNICODE_CODEPOINT_INVALID 0xFFFD

#define DRAW_DRAWLIST_CIRCLE_AUTO_SEGMENT_MIN 12
#define DRAW_DRAWLIST_CIRCLE_AUTO_SEGMENT_MAX 512
#define DRAW_DRAWLIST_CIRCLE_AUTO_SEGMENT_CALC(_RAD, _MAXERROR) CRT::Clamp((int)((M::_PI * 2.0f) / M_ACOS((_RAD - _MAXERROR) / _RAD)), DRAW_DRAWLIST_CIRCLE_AUTO_SEGMENT_MIN, DRAW_DRAWLIST_CIRCLE_AUTO_SEGMENT_MAX)

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 26495) // variable 'X' is uninitialized. Always initialize a member variable (type.6).
#endif
#if defined(__clang__)
#pragma clang diagnostic push
#if __has_warning("-Wunknown-warning-option")
#pragma clang diagnostic ignored "-Wunknown-warning-option" // warning: unknown warning group 'xxx'
#endif
#pragma clang diagnostic ignored "-Wunknown-pragmas" // warning: unknown warning group 'xxx'
#pragma clang diagnostic ignored "-Wold-style-cast"
#pragma clang diagnostic ignored "-Wfloat-equal" // warning: comparing floating point with == or != is unsafe
#pragma clang diagnostic ignored "-Wzero-as-null-pointer-constant"
#pragma clang diagnostic ignored "-Wreserved-identifier" // warning: identifier '_Xxx' is reserved because it starts with '_' followed by a capital letter
#elif defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpragmas" // warning: unknown option after '#pragma GCC diagnostic' kind
#pragma GCC diagnostic ignored "-Wclass-memaccess" // [__GNUC__ >= 8] warning: 'memset/memcpy' clearing/writing an object of type 'xxxx' with no trivial copy-assignment; use assignment or value-initialization instead
#endif

#pragma region draw_forward_declaraions
struct DrawChannel_t;
struct DrawCmd_t;
struct DrawData_t;
struct DrawList_t;
struct DrawListSharedData_t;
struct DrawListSplitter_t;
struct DrawVert_t;
struct Font_t;
struct FontAtlas_t;
struct FontConfig_t;
struct FontGlyph_t;
struct FontGlyphRangesBuilder_t;
struct DrawIO_t;
struct DrawListClipper_t;

class CDrawContext;
#pragma endregion

#pragma region draw_structs
// enum typedefs
typedef int DrawListFlags_t;
typedef int DrawFlags_t;
typedef int FontAtlasFlags_t;
typedef int FontRasterizerFlags_t;

// struct typedefs
typedef std::uint16_t DrawIdx_t;
typedef void* TextureID_t;
typedef std::uint16_t Wchar_t;

// draw call back, will be called if not null, else will be a normal draw call
typedef void ( *DrawCallback_t )( const DrawList_t* parent_list, const DrawCmd_t* cmd );
#define DrawCallback_ResetRenderState (DrawCallback_t)(-1)
#pragma endregion

// clang-format off
#pragma region draw_main
struct DrawCmd_t
{
	std::uint32_t nElemCount; // Number of indices (multiple of 3) to be rendered as triangles. Vertices are stored in the callee ImDrawList's vtx_buffer[] array, indices in idx_buffer[].
	Vector4D_t vecClipRect; // Clipping rectangle (x1, y1, x2, y2). Subtract ImDrawData->vecDisplayPos to get clipping rectangle in "viewport" coordinates
	TextureID_t pTextureId; // User-provided texture ID. Set by user in ImfontAtlas::SetTexID() for fonts or passed to Image*() functions. Ignore if never using images or multiple fonts atlas.
	std::uint32_t nVtxOffset; // Start offset in vertex buffer. Pre-1.71 or without ImGuiBackendFlags_RendererHasVtxOffset: always 0. With ImGuiBackendFlags_RendererHasVtxOffset: may be >0 to support meshes larger than 64K vertices with 16-bit indices.
	std::uint32_t nIdxOffset; // Start offset in index buffer. Always equal to sum of nElemCount drawn so far.
	DrawCallback_t fnUserCallback; // If != NULL, call the function instead of rendering the vertices. clip_rect and texture_id will be set normally.
	void* pUserCallbackData; // The draw fnCallback code can access this.

	DrawCmd_t( )
	{
		nElemCount = 0U;
		pTextureId = ( TextureID_t )NULL;
		nVtxOffset = nIdxOffset = 0;
		fnUserCallback = NULL;
		pUserCallbackData = NULL;
	}
};

struct DrawVert_t
{
	Vector2D_t vecPostion;
	Vector2D_t vecUV;
	ColorPacked_t uCol;
};

struct DrawChannel_t
{
	CRT::Vector<DrawCmd_t> _vecCmdBuffer;
	CRT::Vector<DrawIdx_t> _vecIdxBuffer;
};

// Split/Merge functions are used to split the draw list into different layers which can be drawn into out of order.
// This is used by the Columns api, so items of each column can be batched together in a same draw call.
struct DrawListSplitter_t
{
	int _iCurrent; // Current channel number (0)
	int _iCount; // Number of active channels (1+)
	CRT::Vector<DrawChannel_t> _vecChannels; // Draw channels (not resized down so _iCount might be < Channels.Size)

	__forceinline DrawListSplitter_t( )
	{
		Clear( );
	}

	__forceinline ~DrawListSplitter_t( )
	{
		ClearFreeMemory( );
	}

	__forceinline void Clear( )
	{
		_iCurrent = 0;
		_iCount = 1;
	} // Do not clear Channels[] so our allocations are reused next frame

	DRAW_API void ClearFreeMemory( );
	DRAW_API void Split( DrawList_t* pDrawList, int count );
	DRAW_API void Merge( DrawList_t* pDrawList );
	DRAW_API void SetCurrentChannel( DrawList_t* pDrawList, int channel_idx );
};

enum EDrawFlags : DrawFlags_t
{
	kDrawFlags_None = 0,
	kDrawFlags_TopLeft = 1 << 0, // 0x1
	kDrawFlags_TopRight = 1 << 1, // 0x2
	kDrawFlags_BotLeft = 1 << 2, // 0x4
	kDrawFlags_BotRight = 1 << 3, // 0x8
	kDrawFlags_Top = kDrawFlags_TopLeft | kDrawFlags_TopRight, // 0x3
	kDrawFlags_Bot = kDrawFlags_BotLeft | kDrawFlags_BotRight, // 0xC
	kDrawFlags_Left = kDrawFlags_TopLeft | kDrawFlags_BotLeft, // 0x5
	kDrawFlags_Right = kDrawFlags_TopRight | kDrawFlags_BotRight, // 0xA
	kDrawFlags_All = kDrawFlags_Top | kDrawFlags_Bot, // 0xF
	kDrawFlags_FillConvex = 1 << 4, // 0x10
};

enum EDrawListFlags : DrawListFlags_t
{
	kDrawListFlags_None = 0,
	kDrawListFlags_AntiAliasedLines = 1 << 0, // Lines are anti-aliased (*2 the number of triangles for 1.0f wide line, otherwise *3 the number of triangles)
	kDrawListFlags_AntiAliasedFill = 1 << 1, // Filled shapes have anti-aliased edges (*2 the number of vertices)
	kDrawListFlags_AllowVtxOffset = 1 << 2 // Can emit 'nVtxOffset > 0' to allow large meshes. Set when 'ImGuiBackendFlags_RendererHasVtxOffset' is enabled.
};

struct DrawList_t
{
	// This is what you have to render
	CRT::Vector<DrawCmd_t> vecCmdBuffer; // Draw commands. Typically 1 command = 1 GPU draw call, unless the command is a fnCallback.
	CRT::Vector<DrawIdx_t> vecIdxBuffer; // Index buffer. Each command consume ImDrawCmd::nElemCount of those
	CRT::Vector<DrawVert_t> vecVtxBuffer; // Vertex buffer.
	DrawListFlags_t nFlags; // nFlags, you may poke into these to adjust anti-aliasing settings per-primitive.

	// [Internal, used while building lists]
	const DrawListSharedData_t* _pData; // Pointer to shared draw data (you can use D::GetDrawListSharedData() to get the one from current D context)
	const char* _szOwnerName; // Pointer to owner window's name for debugging
	std::uint32_t _nVtxCurrentOffset; // [Internal] Always 0 unless 'nFlags & ImDrawListFlags_AllowVtxOffset'.
	std::uint32_t _nVtxCurrentIdx; // [Internal] Generally == vecVtxBuffer.Size unless we are past 64K vertices, in which case this gets reset to 0.
	DrawVert_t* _pVtxWritePtr; // [Internal] point within vecVtxBuffer.Data after each add command (to avoid using the ImVector<> operators too much)
	DrawIdx_t* _pIdxWritePtr; // [Internal] point within vecIdxBuffer.Data after each add command (to avoid using the ImVector<> operators too much)
	CRT::Vector<Vector4D_t> _vecClipRectStack; // [Internal]
	CRT::Vector<TextureID_t> _vecTextureIdStack; // [Internal]
	CRT::Vector<Vector2D_t> _vecPath; // [Internal] current path building
	DrawListSplitter_t _drawSplitter; // [Internal] for channels api

	DrawList_t( const DrawListSharedData_t* shared_data ) { _pData = shared_data; _szOwnerName = NULL; Clear( ); }
	~DrawList_t( ) { ClearFreeMemory( ); }
	DRAW_API void  PushClipRect( Vector2D_t clip_rect_min, Vector2D_t clip_rect_max, bool intersect_with_current_clip_rect = false );  // Render-level scissoring. This is passed down to your render function but not used for CPU-side coarse clipping. Prefer using higher-level D::PushClipRect() to affect logic (hit-testing and widget culling)
	DRAW_API void  PushClipRectFullScreen( );
	DRAW_API void  PopClipRect( );
	DRAW_API void  PushTextureID( TextureID_t texture_id );
	DRAW_API void  PopTextureID( );

	DRAW_API TextureID_t  GetCurrentTextureId( ) const;
	DRAW_API const Vector4D_t& GetCurrentClipRect( ) const;
	__forceinline Vector2D_t   GetClipRectMin( ) const { const Vector4D_t& cr = _vecClipRectStack.back( ); return Vector2D_t( cr.x, cr.y ); }
	__forceinline Vector2D_t   GetClipRectMax( ) const { const Vector4D_t& cr = _vecClipRectStack.back( ); return Vector2D_t( cr.z, cr.w ); }

	DRAW_API void  AddLine( const Vector2D_t& vecFirst, const Vector2D_t& vecSecond, const ColorPacked_t uCol, float flThickness = 1.0f );
	DRAW_API void  AddRect( const Vector2D_t& vecMin, const Vector2D_t& vecMax, const ColorPacked_t uCol, float flRounding = 0.0f, DrawFlags_t nRoundingCornerFlags = kDrawFlags_All, float flThickness = 1.0f );
	DRAW_API void  AddRectFilledMultiColor( const Vector2D_t& vecMin, const Vector2D_t& vecMax, const ColorPacked_t uColUpLeft, const ColorPacked_t uColUpRight, const ColorPacked_t uColBotRight, const ColorPacked_t uColBotLeft, float flRounding = 0.0f, DrawFlags_t nRoundingCornerFlags = kDrawFlags_All );
	DRAW_API void  AddQuad( const Vector2D_t& vecFirst, const Vector2D_t& vecSecond, const Vector2D_t& vecThird, const Vector2D_t& vecFourth, const ColorPacked_t uCol, float flThickness = 1.0f, const DrawFlags_t nDrawFlags = kDrawFlags_None );
	DRAW_API void  AddTriangle( const Vector2D_t& vecFirst, const Vector2D_t& vecSecond, const Vector2D_t& vecThird, const ColorPacked_t uCol, float flThickness = 1.0f, const DrawFlags_t nDrawFlags = kDrawFlags_None );
	DRAW_API void  AddCircle( const Vector2D_t& vecCenter, float flRadius, const ColorPacked_t uCol, int iSegments = 12, float flThickness = 1.0f, const DrawFlags_t nDrawFlags = kDrawFlags_None );
	DRAW_API void  AddCircleMultiColor( const Vector2D_t& vecCenter, float flRadius, const ColorPacked_t uColCenter, const ColorPacked_t uColOuter, int iSegments = 12 );
	DRAW_API void  AddNgon( const Vector2D_t& vecCenter, float flRadius, const ColorPacked_t uCol, int iSegments = 12, float flThickness = 1.0f, const DrawFlags_t nDrawFlags = kDrawFlags_None );
	DRAW_API void  AddText( const Vector2D_t& vecPostion, const ColorPacked_t uCol, const char* text_begin, const char* szTextEnd = NULL );
	DRAW_API void  AddText( const Font_t* pFont, float font_size, const Vector2D_t& vecPostion, const ColorPacked_t uCol, const char* text_begin, const char* szTextEnd = NULL, float flWrapWidth = 0.0f, const Vector4D_t* cpu_fine_clip_rect = NULL );

	// @todo: maybe merge this 2 together and use kDrawFlags_FillConvex
	DRAW_API void  AddPolyline( const Vector2D_t* pPoints, size_t iPointCount, const ColorPacked_t uCol, bool bClosed, float flThickness );
	DRAW_API void  AddConvexPolyFilled( const Vector2D_t* pPoints, size_t iPointCount, const ColorPacked_t uCol ); // Note: Anti-aliased filling requires pPoints to be in clockwise order.

	DRAW_API void  AddImage( TextureID_t pTextureID, const Vector2D_t& vecMin, const Vector2D_t& vecMax, const Vector2D_t& vecUVMin = Vector2D_t( 0, 0 ), const Vector2D_t& vecUVMax = Vector2D_t( 1, 1 ), ColorPacked_t uCol = DRAW_COL32_WHITE );
	DRAW_API void  AddImageQuad( TextureID_t pTextureID, const Vector2D_t& vecFirst, const Vector2D_t& vecSecond, const Vector2D_t& vecThird, const Vector2D_t& vecFourth, const Vector2D_t& vecUVFirst = Vector2D_t( 0, 0 ), const Vector2D_t& vecUVSecond = Vector2D_t( 1, 0 ), const Vector2D_t& vecUVThird = Vector2D_t( 1, 1 ), const Vector2D_t& vecUVFourth = Vector2D_t( 0, 1 ), ColorPacked_t uCol = DRAW_COL32_WHITE );
	DRAW_API void  AddImageRounded( TextureID_t pTextureID, const Vector2D_t& vecMin, const Vector2D_t& vecMax, const Vector2D_t& vecUVMin, const Vector2D_t& vecUVMax, ColorPacked_t uCol, float flRounding, DrawFlags_t nRoundingCornerFlags = kDrawFlags_All );

	DRAW_API void AddShadowRect( const Vector2D_t& p_min, const Vector2D_t& p_max, float shadow_thickness, const Vector2D_t& offset, ColorPacked_t col, float rounding = 0.0f, DrawFlags_t rounding_corners = kDrawFlags_All );

	__forceinline    void  PathClear( ) { _vecPath.Size = 0; }
	__forceinline    void  PathLineTo( const Vector2D_t& vecPostion ) { _vecPath.push_back( vecPostion ); }
	__forceinline    void  PathLineToMergeDuplicate( const Vector2D_t& vecPostion ) { if ( _vecPath.Size == 0 || CRT::MemoryCompare( &_vecPath.Data[ _vecPath.Size - 1 ], &vecPostion, 8 ) != 0 ) _vecPath.push_back( vecPostion ); }
	__forceinline    void  PathFillConvex( ColorPacked_t uCol ) { AddConvexPolyFilled( _vecPath.Data, _vecPath.Size, uCol ); _vecPath.Size = 0; }  // Note: Anti-aliased filling requires pPoints to be in clockwise order.
	__forceinline    void  PathStroke( ColorPacked_t uCol, bool bClosed, float flThickness = 1.0f ) { AddPolyline( _vecPath.Data, _vecPath.Size, uCol, bClosed, flThickness ); _vecPath.Size = 0; }
	DRAW_API void  PathArcTo( const Vector2D_t& vecCenter, float flRadius, float a_min, float a_max, int iSegments = 10 );
	DRAW_API void  PathArcToFast( const Vector2D_t& vecCenter, float flRadius, int a_min_of_12, int a_max_of_12 );                                            // Use precomputed angles for a 12 steps circle
	DRAW_API void  PathBezierCurveTo( const Vector2D_t& vecSecond, const Vector2D_t& vecThird, const Vector2D_t& vecFourth, int iSegments = 0 );
	DRAW_API void  PathRect( const Vector2D_t& vecMin, const Vector2D_t& vecMax, float flRounding = 0.0f, DrawFlags_t nRoundingCornerFlags = kDrawFlags_All );

	DRAW_API void  AddCallback( DrawCallback_t fnCallback, void* pData );
	DRAW_API void  AddDrawCmd( );
	DRAW_API DrawList_t* CloneOutput( ) const;

	__forceinline void     ChannelsSplit( int count ) { _drawSplitter.Split( this, count ); }
	__forceinline void     ChannelsMerge( ) { _drawSplitter.Merge( this ); }
	__forceinline void     ChannelsSetCurrent( int n ) { _drawSplitter.SetCurrentChannel( this, n ); }

	void ShadeVertsLinearColorGradientKeepAlpha( int iVtxStart, int iVtxEnd, Vector2D_t vecGradient0, Vector2D_t vecGradient1, ColorPacked_t uColPrimary, ColorPacked_t uColSecondary );
	void ShadeVertsLinearUV( int iVtxStart, int iVtxEnd, const Vector2D_t& vecStart, const Vector2D_t& vecEnd, const Vector2D_t& vecUvStart, const Vector2D_t& vecUvEnd, bool bClamp );

	// Internal helpers
	// NB: all primitives needs to be reserved via PrimReserve() beforehand!
	DRAW_API void  Clear( );
	DRAW_API void  ClearFreeMemory( );
	DRAW_API void  PrimReserve( int nIdxCount, int nVtxCount );
	DRAW_API void  PrimUnreserve( int nIdxCount, int nVtxCount );
	DRAW_API void  PrimRect( const Vector2D_t& a, const Vector2D_t& b, ColorPacked_t uCol );      // Axis aligned rectangle (composed of two triangles)
	DRAW_API void  PrimRectUV( const Vector2D_t& a, const Vector2D_t& b, const Vector2D_t& uv_a, const Vector2D_t& uv_b, ColorPacked_t uCol );
	DRAW_API void  PrimQuadUV( const Vector2D_t& a, const Vector2D_t& b, const Vector2D_t& c, const Vector2D_t& d, const Vector2D_t& uv_a, const Vector2D_t& uv_b, const Vector2D_t& uv_c, const Vector2D_t& uv_d, ColorPacked_t uCol );
	__forceinline   void  PrimWriteVtx( const Vector2D_t& vecPostion, const Vector2D_t& vecUV, ColorPacked_t uCol ) { _pVtxWritePtr->vecPostion = vecPostion; _pVtxWritePtr->vecUV = vecUV; _pVtxWritePtr->uCol = uCol; _pVtxWritePtr++; _nVtxCurrentIdx++; }
	__forceinline   void  PrimWriteIdx( DrawIdx_t nIdx ) { *_pIdxWritePtr = nIdx; _pIdxWritePtr++; }
	__forceinline   void  PrimVtx( const Vector2D_t& vecPostion, const Vector2D_t& vecUV, ColorPacked_t uCol ) { PrimWriteIdx( ( DrawIdx_t )_nVtxCurrentIdx ); PrimWriteVtx( vecPostion, vecUV, uCol ); }
	DRAW_API void  UpdateClipRect( );
	DRAW_API void  UpdateTextureID( );
};

struct PiorityDrawList_t
{
	PiorityDrawList_t( DrawListSharedData_t* pSharedData, size_t nPiority = static_cast< size_t >( -1 ) ) : drawList( pSharedData ), nPiority( nPiority )
	{ }

	~PiorityDrawList_t( )
	{
		drawList.ClearFreeMemory( );
		nPiority = static_cast< size_t >( -1 );
	}

	DrawList_t drawList;
	// @note: this is a piority value, the lower the value the more important it is
	size_t nPiority;
};

struct DrawData_t
{
	bool            bValid;                  // Only valid after Render() is called and before the next NewFrame() is called.
	DrawList_t** ppCmdLists;               // Array of ImDrawList* to render. The ImDrawList are owned by ImGuiContext and only pointed to from here.
	int             iCmdListsCount;          // Number of ImDrawList* to render
	int             iTotalIdxCount;          // For convenience, sum of all ImDrawList's vecIdxBuffer.Size
	int             iTotalVtxCount;          // For convenience, sum of all ImDrawList's vecVtxBuffer.Size
	Vector2D_t      vecDisplayPos;             // Upper-left position of the viewport to render (== upper-left of the orthogonal projection matrix to use)
	Vector2D_t      vecDisplaySize;            // Size of the viewport to render (== io.vecDisplaySize for the main viewport) (vecDisplayPos + vecDisplaySize == lower-right of the orthogonal projection matrix to use)
	Vector2D_t      vecFramebufferScale;       // Amount of pixels for each unit of vecDisplaySize. Based on io.vecDisplayFramebufferScale. Generally (1,1) on normal display, (2,2) on OSX with Retina display.

	bool			bDebugWireframe;

	// Functions
	DrawData_t( ) { bValid = false; bDebugWireframe = false; Clear( ); }
	~DrawData_t( ) { Clear( ); }
	void Clear( ) { bValid = false; ppCmdLists = NULL; iCmdListsCount = iTotalVtxCount = iTotalIdxCount = 0; vecDisplayPos = vecDisplaySize = vecFramebufferScale = Vector2D_t( 0.f, 0.f ); } // The ImDrawList are owned by ImGuiContext!
	DRAW_API void  DeIndexAllBuffers( );                    // Helper to convert all buffers from indexed to non-indexed, in case you cannot render indexed. Note: this is slow and most likely a waste of resources. Always prefer indexed rendering!
	DRAW_API void  ScaleClipRects( const Vector2D_t& fb_scale ); // Helper to flScale the vecClipRect field of each ImDrawCmd. Use if your final output buffer is at a different flScale than Dear D expects, or if there is a difference between your window resolution and framebuffer resolution.
};

#pragma endregion

#pragma region draw_fonts

enum EDrawFontAtlasFlags : FontAtlasFlags_t
{
	kDrawFontAtlasFlags_None = 0,
	kDrawFontAtlasFlags_NoPowerOfTwoHeight = 1 << 0, // Don't round the height to next power of two
};

enum EFontRasterizerFlags : FontRasterizerFlags_t
{
	kFontRasterizerFlags_None = 0,
	// hiting flags
	kFontRasterizerFlags_NoHinting = 1 << 0,
	kFontRasterizerFlags_NoAutoHint = 1 << 1,
	kFontRasterizerFlags_ForceAutoHint = 1 << 2,
	kFontRasterizerFlags_LightHinting = 1 << 3,
	kFontRasterizerFlags_MonoHinting = 1 << 4,
	// styling bold
	kFontRasterizerFlags_Bold = 1 << 5,
	// styling italic/oblique
	kFontRasterizerFlags_Oblique = 1 << 6,
	// @note: combine with kFontRasterizerFlags_MonoHinting for best results.
	kFontRasterizerFlags_Monochrome = 1 << 7,
};

struct FontConfig_t

{
	DRAW_API FontConfig_t( )
	{
		pFontData = NULL;
		iFontDataSize = 0;
		bFontDataOwnedByAtlas = true;
		iFontNo = 0;
		flSizePixels = 0.0f;
		iOversampleH = 3; // FIXME: 2 may be a better default?
		iOversampleV = 1;
		bPixelSnapH = false;
		vecGlyphExtraSpacing = Vector2D_t( 0.0f, 0.0f );
		vecGlyphOffset = Vector2D_t( 0.0f, 0.0f );
		pGlyphRanges = NULL;
		flGlyphMinAdvanceX = 0.0f;
		flGlyphMaxAdvanceX = FLT_MAX;
		bMergeMode = false;
		nRasterizerFlags = 0x00;
		flRasterizerMultiply = 1.0f;
		wcEllipsis = ( Wchar_t )-1;
		CRT::MemorySet( szName, 0, sizeof( szName ) );
		pDstFont = NULL;
	}

	void* pFontData; // nullptr
	int iFontDataSize; // 0
	bool bFontDataOwnedByAtlas; // true
	int iFontNo; // 0
	float flSizePixels; //
	int iOversampleH; // 3
	int iOversampleV; // 1
	bool bPixelSnapH; // false
	Vector2D_t vecGlyphExtraSpacing; // 0, 0
	Vector2D_t vecGlyphOffset; // 0, 0
	const Wchar_t* pGlyphRanges; // NULL
	float flGlyphMinAdvanceX; // 0
	float flGlyphMaxAdvanceX; // FLT_MAX
	bool bMergeMode; // false
	std::uint32_t nRasterizerFlags; // 0x00
	float flRasterizerMultiply; // 1.0f
	Wchar_t wcEllipsis; // -1

	char szName[ 40 ] = { '\0' }; // szName (strictly to ease debugging)
	Font_t* pDstFont;
};

struct FontGlyph_t
{
	Wchar_t Codepoint; // 0x0000..0xFFFF
	float AdvanceX; // Distance to next character (= data from pFont + ImFontConfig::vecGlyphExtraSpacing.x baked in)
	float X0, Y0, X1, Y1; // Glyph corners
	float U0, V0, U1, V1; // Texture coordinates
};

struct FontGlyphRangesBuilder_t
{
	FontGlyphRangesBuilder_t( )
	{
		Clear( );
	}

	__forceinline void Clear( )
	{
		int size_in_bytes = ( DRAW_UNICODE_CODEPOINT_MAX + 1 ) / 8;
		vecUsedChars.resize( size_in_bytes / ( int )sizeof( std::uint32_t ) );
		CRT::MemorySet( vecUsedChars.Data, 0, ( size_t )size_in_bytes );
	}

	__forceinline bool GetBit( int n ) const
	{
		int off = ( n >> 5 );
		std::uint32_t mask = 1u << ( n & 31 );
		return ( vecUsedChars[ off ] & mask ) != 0;
	} // Get bit n in the array

	__forceinline void SetBit( int n )
	{
		int off = ( n >> 5 );
		std::uint32_t mask = 1u << ( n & 31 );
		vecUsedChars[ off ] |= mask;
	} // Set bit n in the array

	__forceinline void AddChar( Wchar_t c )
	{
		SetBit( c );
	} // Add character

	DRAW_API void AddText( const char* szText, const char* szTextEnd = NULL ); // Add string (each character of the UTF-8 string are added)
	DRAW_API void AddRanges( const Wchar_t* pRanges ); // Add pRanges, e.g. builder.AddRanges(FontAtlas_t::GetGlyphRangesDefault()) to force add all of ASCII/Latin+Ext
	DRAW_API void BuildRanges( CRT::Vector<Wchar_t>* pvecOutRanges ); // Output new pRanges

	CRT::Vector<std::uint32_t> vecUsedChars; // Store 1-bit per Unicode code point (0=unused, 1=used)
};

struct FontAtlasCustomRect_t
{
	std::uint32_t ID; // Input    // User ID. Use < 0x110000 to map into a pFont glyph, >= 0x110000 for other/internal/custom texture data.
	std::uint16_t W, H; // Input    // Desired rectangle dimension
	std::uint16_t X, Y; // Output   // Packed position in Atlas
	float flGlyphAdvanceX; // Input    // For custom pFont glyphs only (ID < 0x110000): glyph xadvance
	Vector2D_t vecGlyphOffset; // Input    // For custom pFont glyphs only (ID < 0x110000): glyph display offset
	Font_t* pFont; // Input    // For custom pFont glyphs only (ID < 0x110000): target pFont

	FontAtlasCustomRect_t( )
	{
		ID = 0xFFFFFFFF;
		W = H = 0;
		X = Y = 0xFFFF;
		flGlyphAdvanceX = 0.0f;
		vecGlyphOffset = Vector2D_t( 0, 0 );
		pFont = NULL;
	}

	bool IsPacked( ) const
	{
		return X != 0xFFFF;
	}
};

struct FontAtlasShadowTexConfig_t
{
	int TexCornerSize; // Size of the corner areas.
	int TexEdgeSize; // Size of the edge areas (and by extension the center). Changing this is normally unnecessary.
	float TexFalloffPower; // The power factor for the shadow falloff curve.
	float TexDistanceFieldOffset; // How much to offset the distance field by (allows over/under-shadowing, potentially useful for accommodating rounded corners on the "casting" shape).
	bool TexBlur; // Do we want to Gaussian blur the shadow texture?

	DRAW_API FontAtlasShadowTexConfig_t( );

	constexpr int GetPadding( ) const
	{
		return 2;
	} // Number of pixels of padding to add to avoid sampling artifacts at the edges.

	int CalcTexSize( ) const
	{
		return TexCornerSize + TexEdgeSize + GetPadding( );
	} // The size of the texture area required for the actual 2x2 shadow texture (after the redundant corners have been removed). Padding is required here to avoid sampling artifacts at the edge adjoining the removed corners.
};

struct FontAtlas_t
{
	DRAW_API FontAtlas_t( );
	DRAW_API ~FontAtlas_t( );
	DRAW_API Font_t* AddFont( const FontConfig_t* font_cfg );
	DRAW_API Font_t* AddFontDefault( const FontConfig_t* font_cfg = NULL );
	DRAW_API Font_t* AddFontFromFileTTF( const char* filename, float size_pixels, const FontConfig_t* font_cfg = NULL, const Wchar_t* glyph_ranges = NULL );
	DRAW_API Font_t* AddFontFromMemoryTTF( void* font_data, int font_size, float size_pixels, const FontConfig_t* font_cfg = NULL, const Wchar_t* glyph_ranges = NULL );
	DRAW_API Font_t* AddFontFromMemoryCompressedTTF( const void* compressed_font_data, int compressed_font_size, float size_pixels, const FontConfig_t* font_cfg = NULL, const Wchar_t* glyph_ranges = NULL );
	DRAW_API void ClearInputData( );
	DRAW_API void ClearTexData( );
	DRAW_API void ClearFonts( );
	DRAW_API void Clear( );

	DRAW_API bool Build( const FontRasterizerFlags_t nFlags = kFontRasterizerFlags_LightHinting );
	DRAW_API void GetTexDataAsAlpha8( std::uint8_t** out_pixels, int* out_width, int* out_height, int* out_bytes_per_pixel = NULL );
	DRAW_API void GetTexDataAsRGBA32( std::uint8_t** out_pixels, int* out_width, int* out_height, int* out_bytes_per_pixel = NULL );
	DRAW_API void ApplyShadowEffect( std::uint8_t** out_pixels, int in_width, int in_height );

	bool IsBuilt( ) const
	{
		return vecFonts.Size > 0 && ( pTexPixelsAlpha8 != NULL || pTexPixelsRGBA32 != NULL );
	}

	void SetTexID( TextureID_t id )
	{
		pTexID = id;
	}

	DRAW_API const Wchar_t* GetGlyphRangesDefault( ); // Basic Latin, Extended Latin
	DRAW_API const Wchar_t* GetGlyphRangesKorean( ); // Default + Korean characters
	DRAW_API const Wchar_t* GetGlyphRangesJapanese( ); // Default + Hiragana, Katakana, Half-W, Selection of 1946 Ideographs
	DRAW_API const Wchar_t* GetGlyphRangesChineseFull( ); // Default + Half-W + Japanese Hiragana/Katakana + full set of about 21000 CJK Unified Ideographs
	DRAW_API const Wchar_t* GetGlyphRangesChineseSimplifiedCommon( ); // Default + Half-W + Japanese Hiragana/Katakana + set of 2500 CJK Unified Ideographs for common simplified Chinese
	DRAW_API const Wchar_t* GetGlyphRangesCyrillic( ); // Default + about 400 Cyrillic characters
	DRAW_API const Wchar_t* GetGlyphRangesThai( ); // Default + Thai characters
	DRAW_API const Wchar_t* GetGlyphRangesVietnamese( ); // Default + Vietnamese characters

	DRAW_API size_t AddCustomRectRegular( std::uint32_t id, int width, int height );
	DRAW_API size_t AddCustomRectFontGlyph( Font_t* pFont, Wchar_t id, int width, int height, float advance_x, const Vector2D_t& offset = Vector2D_t( 0, 0 ) );

	const FontAtlasCustomRect_t* GetCustomRectByIndex( int index )
	{
		CRT_ASSERTION( index >= 0 );
		return &vecCustomRects[ index ];
	}

	DRAW_API void CalcCustomRectUV( const FontAtlasCustomRect_t* rect, Vector2D_t* out_uv_min, Vector2D_t* out_uv_max ) const;

	bool bLocked;
	FontAtlasFlags_t nFlags;
	TextureID_t pTexID;
	int iTexDesiredWidth;
	int iTexGlyphPadding;
	Vector2D_t vecTexGlyphShadowOffset;
	std::uint8_t* pTexPixelsAlpha8;
	std::uint32_t* pTexPixelsRGBA32;
	int iTexWidth;
	int iTexHeight;
	Vector2D_t vecTexUvScale;
	Vector2D_t vecTexUvWhitePixel;
	CRT::Vector<Font_t*> vecFonts;
	CRT::Vector<FontAtlasCustomRect_t> vecCustomRects;
	CRT::Vector<FontConfig_t> vecConfigData;
	int iMainRectId;
	int iShadowRectId;
	Vector4D_t ShadowRectUvs[ 9 ]; // UV coordinates for shadow texture.
	FontAtlasShadowTexConfig_t ShadowTexConfig;
};

struct Font_t
{
	// Methods
	DRAW_API Font_t( );
	DRAW_API ~Font_t( );
	DRAW_API const FontGlyph_t* FindGlyph( Wchar_t c ) const;
	DRAW_API const FontGlyph_t* FindGlyphNoFallback( Wchar_t c ) const;

	float GetCharAdvance( Wchar_t c ) const
	{
		return ( ( int )c < vecIndexAdvanceX.Size ) ? vecIndexAdvanceX[ ( int )c ] : flFallbackAdvanceX;
	}

	bool IsLoaded( ) const
	{
		return pContainerAtlas != NULL;
	}

	const char* GetDebugName( ) const
	{
		return pConfig ? pConfig->szName : "<unknown>";
	}

	// 'max_width' stops rendering after a certain width (could be turned into a 2d size). FLT_MAX to disable.
	// 'flWrapWidth' enable automatic word-wrapping across multiple lines to fit into given width. 0.0f to disable.
	DRAW_API Vector2D_t CalcTextSizeA( float size, float max_width, float flWrapWidth, const char* text_begin, const char* szTextEnd = NULL, const char** remaining = NULL ) const; // utf8
	DRAW_API const char* CalcWordWrapPositionA( float flScale, const char* szText, const char* szTextEnd, float flWrapWidth ) const;
	DRAW_API void RenderChar( DrawList_t* pDrawList, float size, Vector2D_t vecPostion, ColorPacked_t uCol, Wchar_t c ) const;
	DRAW_API void RenderText( DrawList_t* pDrawList, float size, Vector2D_t vecPostion, ColorPacked_t uCol, const Vector4D_t& clip_rect, const char* text_begin, const char* szTextEnd, float flWrapWidth = 0.0f, bool cpu_fine_clip = false ) const;

	// [Internal] Don't use!
	DRAW_API void BuildLookupTable( );
	DRAW_API void ClearOutputData( );
	DRAW_API void GrowIndex( size_t new_size );
	DRAW_API void AddGlyph( Wchar_t c, float x0, float y0, float x1, float y1, float u0, float v0, float u1, float v1, float advance_x );
	DRAW_API void AddRemapChar( Wchar_t dst, Wchar_t src, bool overwrite_dst = true ); // Makes 'dst' character/glyph pPoints to 'src' character/glyph. Currently needs to be called AFTER fonts have been built.
	DRAW_API void SetFallbackChar( Wchar_t c );

	// Members: Hot ~20/24 bytes (for CalcTextSize)
	CRT::Vector<float> vecIndexAdvanceX; // 12-16 // out //            // Sparse. vecGlyphs->AdvanceX in a directly indexable way (cache-friendly for CalcTextSize functions which only this this info, and are often bottleneck in large UI).
	float flFallbackAdvanceX; // 4     // out // = pFallbackGlyph->AdvanceX
	float flFontSize; // 4     // in  //            // H of characters/line, set during loading (don't change after loading)

	// Members: Hot ~36/48 bytes (for CalcTextSize + render loop)
	CRT::Vector<Wchar_t> vecIndexLookup; // 12-16 // out //            // Sparse. Index glyphs by Unicode code-point.
	CRT::Vector<FontGlyph_t> vecGlyphs; // 12-16 // out //            // All glyphs.
	const FontGlyph_t* pFallbackGlyph; // 4-8   // out // = FindGlyph(FontFallbackChar)
	Vector2D_t vecDisplayOffset; // 8     // in  // = (0,0)    // Offset pFont rendering by xx pixels

	// Members: Cold ~32/40 bytes
	FontAtlas_t* pContainerAtlas; // 4-8   // out //            // What we has been loaded into
	const FontConfig_t* pConfig; // 4-8   // in  //            // Pointer within pContainerAtlas->vecConfigData
	short nConfigDataCount; // 2     // in  // ~ 1        // Number of ImFontConfig involved in creating this pFont. Bigger than 1 when merging multiple pFont sources into one ImFont.
	Wchar_t wcFallback; // 2     // in  // = '?'      // Replacement character if a glyph isn't found. Only set via SetFallbackChar()
	Wchar_t wcEllipsis; // 2     // out // = -1       // Character used for ellipsis rendering.
	bool bDirtyLookupTables; // 1     // out //
	float flScale; // 4     // in  // = 1.f      // Base pFont flScale, multiplied by the per-window pFont flScale which you can adjust with SetWindowFontScale()
	float flAscent, flDescent; // 4+4   // out //            // flAscent: distance from top to bottom of e.g. 'A' [0..flFontSize]
	int iMetricsTotalSurface; // 4     // out //            // Total surface in pixels to get an idea of the pFont rasterization/texture cost (not exact, we approximate the cost of padding between glyphs)
};

#pragma endregion

struct BoolVector_t
{
	CRT::Vector<int> storage;

	BoolVector_t( ) { }

	void Resize( int sz )
	{
		storage.resize( ( sz + 31 ) >> 5 );
		CRT::MemorySet( storage.Data, 0, ( size_t )storage.Size * sizeof( storage.Data[ 0 ] ) );
	}

	void Clear( )
	{
		storage.clear( );
	}

	bool GetBit( int n ) const
	{
		int off = ( n >> 5 );
		int mask = 1 << ( n & 31 );
		return ( storage[ off ] & mask ) != 0;
	}

	void SetBit( int n, bool v )
	{
		int off = ( n >> 5 );
		int mask = 1 << ( n & 31 );
		if ( v )
			storage[ off ] |= mask;
		else
			storage[ off ] &= ~mask;
	}
};

struct DRAW_API DrawListSharedData_t
{
	Vector2D_t vecTexUvWhitePixel = Vector2D_t( ); // UV of white pixel in the atlas
	Font_t* pFont = nullptr; // Current/default pFont (optional, for simplified AddText overload)
	float flFontSize = 0.0f; // Current/default pFont size (optional, for simplified AddText overload)
	float flCurveTessellationTol = 0.0f; // Tessellation tolerance when using PathBezierCurveTo()
	float flCircleSegmentMaxError = 0.0f; // Number of circle segments to use per pixel of flRadius for AddCircle() etc
	Vector4D_t vecClipRectFullscreen = Vector4D_t( ); // Value for PushClipRectFullscreen()
	DrawListFlags_t nInitialFlags = 0; // Initial flags at the beginning of the frame (it is possible to alter flags on a per-drawlist basis afterwards)

	// [Internal] Lookup tables
	Vector2D_t arrCircleVtx12[ 12 ] = {}; // FIXME: Bake rounded corners fill/borders in atlas
	std::uint8_t arrCircleSegmentCounts[ 64 ] = {}; // Precomputed segment count for given flRadius (array index + 1) before we calculate it dynamically (to avoid calculation overhead)

	int ShadowRectId = -1; // IDs of rects for shadow texture (2 entries)
	const Vector4D_t* ShadowRectUvs = nullptr; // UV coordinates for shadow texture (10 entries)

	DrawListSharedData_t( );
	void SetCircleSegmentMaxError( float max_error );
};

struct DrawDataBuilder_t
{
	CRT::Vector<DrawList_t*> vecLayers[ 2 ]; // global layers for: regular, tooltip

	void Clear( )
	{
		for ( int n = 0; n < CRT_ARRAYSIZE( vecLayers ); n++ )
			vecLayers[ n ].resize( 0 );
	}

	void ClearFreeMemory( )
	{
		for ( int n = 0; n < CRT_ARRAYSIZE( vecLayers ); n++ )
			vecLayers[ n ].clear( );
	}

	DRAW_API void AddDrawList( const size_t nLayer, DrawList_t* pDrawList );
	// aka merge into single layer
	DRAW_API void FlattenIntoSingleLayer( );
};

class CDrawContext
{
public:
	CDrawContext( );
	virtual ~CDrawContext( );
	virtual void NewFrame( const DrawListFlags_t nDrawFlags = 0 ) = 0;
	virtual void Render( ) = 0;
	virtual bool CreateDeviceObjects( ) = 0;
	virtual void InvalidateDeviceObjects( ) = 0;

	// device backend helper
	// clang-format off
	virtual void* CreateVertexShader( const char* szSource ) { return nullptr; }
	virtual void DestroyVertexShader( void* pShader ) { CRT_UNUNSED( pShader ); }
	virtual void* CreatePixelShader( const char* szSource ) { return nullptr; }
	virtual void DestroyPixelShader( void* pShader ) { CRT_UNUNSED( pShader ); }

	DrawData_t* GetDrawData( )
	{
		return drawData.bValid ? &drawData : NULL;
	}

	void SetCurrentFont( Font_t* pFont );

	Font_t* GetDefaultFont( )
	{
		return pFontAtlas != nullptr ? pFontAtlas->vecFonts[ 0 ] : nullptr;
	}

	void SetDisplaySize( const Vector2D_t& vecSize )
	{
		vecDisplaySize = vecSize;
	}

	void SetDeltaTime( const float flDeltaTime )
	{
		this->flDeltaTime = flDeltaTime;
	}

	size_t AddDrawList( size_t nPiority )
	{
		vecPiorityDrawList.push_back( PiorityDrawList_t( &drawListSharedData, nPiority ) );
		return vecPiorityDrawList.size( ) - 1;
	}

	DrawList_t* GetDrawList( size_t nIdx )
	{
		CRT_ASSERTION( vecPiorityDrawList[ nIdx ].nPiority != static_cast< size_t >( -1 ) && nIdx < static_cast< size_t >( vecPiorityDrawList.size( ) ) );
		return &vecPiorityDrawList[ nIdx ].drawList;
	}

private:
	void SortPiorityDrawList( );

protected:
	void UpdateForNewFrame( const DrawListFlags_t nDrawFlags = 0 );
	[[nodiscard]] DrawData_t* BuildDrawData( );
	void EndFrame( );
	void SetupDrawData( CRT::Vector<DrawList_t*>* draw_lists );

	// helper to release interfaces
	template <typename T>
	void SafeRelease( T*& p )
	{
		if ( p )
		{
			p->Release( );
			p = nullptr;
		}
	}

public:
	bool bInitialized = false;

	Vector2D_t vecDisplaySize = Vector2D_t( 0, 0 );
	float flDeltaTime = 0.0f;

	FontAtlas_t* pFontAtlas = nullptr;
	float flFontGlobalScale = 1.0f;
	bool bFontAllowUserScaling = false;
	Font_t* pFontDefault = nullptr;
	Vector2D_t vecDisplayFramebufferScale = Vector2D_t( 1.0f, 1.0f );

	void* pBackendRendererUserData = nullptr;

	float flFramerate = 0.0f;

	int iMetricsRenderVertices = 0;
	int iMetricsRenderIndices = 0;
	int iMetricsActiveAllocations = 0;

	DrawListSharedData_t drawListSharedData;
	Font_t* pFont = nullptr;
	float flFontSize = 0.0f;
	float FontBaseSize = 0.0f;
	double dbTime = 0.0;
	int iFrameCount = 0;
	int iFrameCountEnded = 0;
	int iFrameCountRendered = 0;
	bool bWithinFrameScope = false;

	// Render
	DrawData_t drawData;
	DrawDataBuilder_t drawDataBuilder;
	DrawList_t backgroundDrawList;
	DrawList_t foregroundDrawList;

	CRT::Vector<PiorityDrawList_t> vecPiorityDrawList;

	float arrFramerateSecPerFrame[ 60 ] = { 0.0f };
	int iFramerateSecPerFrameIdx = 0;
	int iFramerateSecPerFrameCount = 0;
	float flFramerateSecPerFrameAccum = 0.0f;
};

inline CDrawContext* g_pDrawContext = nullptr;

#ifdef _MSC_VER
#pragma warning(pop)
#endif
#if defined(__clang__)
#pragma clang diagnostic pop
#elif defined(__GNUC__)
#pragma GCC diagnostic pop
#endif
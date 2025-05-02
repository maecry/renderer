#include "draw.h"

// @todo: cleanup :sob:

// used: crt, math, file_t, heapalloc, heapfree
#include "../utilities/math.h"
#include "../utilities/crt.h"
#include "../utilities/filehelpers.h"
#include "../utilities/memory.h"

// used: [ext] stb_rectpack
#define STBRP_STATIC
#define STBRP_ASSERT(x) CRT_ASSERTION(x)
#define STBRP_SORT qsort
#define STB_RECT_PACK_IMPLEMENTATION
#include "../dependencies/stb/stb_rectpack.h"

// used: [ext] freetype
#include <stdint.h>
#include <ft2build.h>
#include FT_FREETYPE_H // <freetype/freetype.h>
#include FT_MODULE_H // <freetype/ftmodapi.h>
#include FT_GLYPH_H // <freetype/ftglyph.h>
#include FT_SYNTHESIS_H // <freetype/ftsynth.h>

constexpr int FONT_ATLAS_DEFAULT_TEX_DATA_W_HALF = 108;
constexpr int FONT_ATLAS_DEFAULT_TEX_DATA_H = 27;
constexpr std::uint32_t FONT_ATLAS_DEFAULT_TEX_DATA_ID = 0x80000000;

FontAtlas_t::FontAtlas_t( )
{
	bLocked = false;
	nFlags = kDrawFontAtlasFlags_None;
	pTexID = ( TextureID_t )NULL;
	iTexDesiredWidth = 0;
	iTexGlyphPadding = 1;
	vecTexGlyphShadowOffset = Vector2D_t( 0.0f, 0.0f );

	pTexPixelsAlpha8 = NULL;
	pTexPixelsRGBA32 = NULL;
	iTexWidth = iTexHeight = 0;
	vecTexUvScale = Vector2D_t( 0.0f, 0.0f );
	vecTexUvWhitePixel = Vector2D_t( 0.0f, 0.0f );
	iMainRectId = -1;
	iShadowRectId = -1;
}

FontAtlas_t::~FontAtlas_t( )
{
	CRT_ASSERTION( !bLocked && "Cannot modify a locked FontAtlas_t between NewFrame() and EndFrame/Render()!" );
	//L_PRINT(LOG_INFO) << __FUNCTION__;
	Clear( );
}

void FontAtlas_t::ClearInputData( )
{
	CRT_ASSERTION( !bLocked && "Cannot modify a locked FontAtlas_t between NewFrame() and EndFrame/Render()!" );
	for ( size_t i = 0; i < vecConfigData.Size; i++ )
		if ( vecConfigData[ i ].pFontData && vecConfigData[ i ].bFontDataOwnedByAtlas )
		{
			MEM_HEAPFREE( vecConfigData[ i ].pFontData );
			vecConfigData[ i ].pFontData = NULL;
		}

	// When clearing this we lose access to the pFont name and other information used to build the pFont.
	for ( size_t i = 0; i < vecFonts.Size; i++ )
		if ( vecFonts[ i ]->pConfig >= vecConfigData.Data && vecFonts[ i ]->pConfig < vecConfigData.Data + vecConfigData.Size )
		{
			vecFonts[ i ]->pConfig = NULL;
			vecFonts[ i ]->nConfigDataCount = 0;
		}
	vecConfigData.clear( );
	vecCustomRects.clear( );
	iMainRectId = -1;
	iShadowRectId = -1;
}

void FontAtlas_t::ClearTexData( )
{
	CRT_ASSERTION( !bLocked && "Cannot modify a locked FontAtlas_t between NewFrame() and EndFrame/Render()!" );
	if ( pTexPixelsAlpha8 )
		MEM_HEAPFREE( pTexPixelsAlpha8 );
	if ( pTexPixelsRGBA32 )
		MEM_HEAPFREE( pTexPixelsRGBA32 );
	pTexPixelsAlpha8 = NULL;
	pTexPixelsRGBA32 = NULL;
}

void FontAtlas_t::ClearFonts( )
{
	CRT_ASSERTION( !bLocked && "Cannot modify a locked FontAtlas_t between NewFrame() and EndFrame/Render()!" );
	for ( size_t i = 0; i < vecFonts.Size; i++ )
		MEM_DELETE( vecFonts[ i ] );
	vecFonts.clear( );
}

void FontAtlas_t::Clear( )
{
	ClearInputData( );
	ClearTexData( );
	ClearFonts( );
}

void FontAtlas_t::GetTexDataAsAlpha8( std::uint8_t** out_pixels, int* out_width, int* out_height, int* out_bytes_per_pixel )
{
	// Build pAtlas on demand
	if ( pTexPixelsAlpha8 == NULL )
	{
		if ( vecConfigData.empty( ) )
			AddFontDefault( );
		Build( );
		//L_PRINT(LOG_INFO) << "GetTexDataAsAlpha8: " << iTexWidth << "x" << iTexHeight;
	}

	*out_pixels = pTexPixelsAlpha8;
	if ( out_width )
		*out_width = iTexWidth;
	if ( out_height )
		*out_height = iTexHeight;
	if ( out_bytes_per_pixel )
		*out_bytes_per_pixel = 1;
}

void FontAtlas_t::GetTexDataAsRGBA32( std::uint8_t** out_pixels, int* out_width, int* out_height, int* out_bytes_per_pixel )
{
	// convert to RGBA32 format on demand
	// although it is likely to be the most commonly used format, our pFont rendering is 1 channel / 8 bpp
	if ( pTexPixelsRGBA32 == nullptr )
	{
		std::uint8_t* pixels = NULL;
		if ( GetTexDataAsAlpha8( &pixels, NULL, NULL ); pixels != nullptr )
		{
			pTexPixelsRGBA32 = MEM_HEAPALLOC( std::uint32_t*, ( size_t )iTexWidth * ( size_t )iTexHeight * 4 );
			const std::uint8_t* src = pixels;
			std::uint32_t* dst = pTexPixelsRGBA32;
			for ( int n = iTexWidth * iTexHeight; n > 0; n-- )
				*dst++ = DRAW_COL32( 255, 255, 255, ( std::uint32_t )( *src++ ) );
		}
		//L_PRINT(LOG_INFO) << "GetTexDataAsRGBA32: " << iTexWidth << "x" << iTexHeight << " => " << (pTexPixelsRGBA32 ? "RGBA32" : "NULL");
	}

	*out_pixels = ( std::uint8_t* )pTexPixelsRGBA32;
	if ( out_width )
		*out_width = iTexWidth;
	if ( out_height )
		*out_height = iTexHeight;
	if ( out_bytes_per_pixel )
		*out_bytes_per_pixel = 4;
}

void FontAtlas_t::ApplyShadowEffect( std::uint8_t** out_pixels, int in_width, int in_height )
{
	CRT_ASSERTION( false && "Not implemented" );
}

Font_t* FontAtlas_t::AddFont( const FontConfig_t* cfg )
{
	CRT_ASSERTION( !bLocked && "Cannot modify a locked FontAtlas_t between NewFrame() and EndFrame/Render()!" );
	CRT_ASSERTION( cfg->pFontData != NULL && cfg->iFontDataSize > 0 );
	CRT_ASSERTION( cfg->flSizePixels > 0.0f );

	// Create new pFont
	if ( !cfg->bMergeMode )
		vecFonts.push_back( MEM_NEW( Font_t ) );
	else
		CRT_ASSERTION( !vecFonts.empty( ) && "Cannot use bMergeMode for the first pFont" ); // When using bMergeMode make sure that a pFont has already been added before. You can use D::GetIO().pFontAtlas->AddFontDefault() to add the default imgui pFont.

	vecConfigData.push_back( *cfg );
	FontConfig_t& new_font_cfg = vecConfigData.back( );
	if ( new_font_cfg.pDstFont == NULL )
		new_font_cfg.pDstFont = vecFonts.back( );
	if ( !new_font_cfg.bFontDataOwnedByAtlas )
	{
		new_font_cfg.pFontData = MEM_HEAPALLOC( void*, new_font_cfg.iFontDataSize );
		new_font_cfg.bFontDataOwnedByAtlas = true;
		CRT::MemoryCopy( new_font_cfg.pFontData, cfg->pFontData, ( size_t )new_font_cfg.iFontDataSize );
	}

	if ( new_font_cfg.pDstFont->wcEllipsis == ( Wchar_t )-1 )
		new_font_cfg.pDstFont->wcEllipsis = cfg->wcEllipsis;

	// invalidate texture
	ClearTexData( );

	return new_font_cfg.pDstFont;
}

struct Gaussian_t
{
	// Perform a single Gaussian blur pass with a fixed kernel size and sigma
	static void Pass( float* src, float* dest, int size, bool horizontal )
	{
		// See http://dev.theomader.com/gaussian-kernel-calculator/
		const float coefficients[ ] = { 0.0f, 0.0f, 0.000003f, 0.000229f, 0.005977f, 0.060598f, 0.24173f, 0.382925f, 0.24173f, 0.060598f, 0.005977f, 0.000229f, 0.000003f, 0.0f, 0.0f };
		const int kernel_size = CRT_ARRAYSIZE( coefficients );

		int sample_step = horizontal ? 1 : size;

		float* read_ptr = src;
		float* write_ptr = dest;

		for ( int y = 0; y < size; y++ )
			for ( int x = 0; x < size; x++ )
			{
				float result = 0;
				int current_offset = ( horizontal ? x : y ) - ( ( kernel_size - 1 ) >> 1 );
				float* sample_ptr = read_ptr - ( ( ( kernel_size - 1 ) >> 1 ) * sample_step );
				for ( int j = 0; j < kernel_size; j++ )
				{
					if ( ( current_offset >= 0 ) && ( current_offset < size ) )
						result += ( *sample_ptr ) * coefficients[ j ];
					current_offset++;
					sample_ptr += sample_step;
				}
				read_ptr++;
				*( write_ptr++ ) = result;
			}
	}

	// Perform an in-place Gaussian blur of a square array of floats with a fixed kernel size and sigma
	// Uses a stack allocation for the temporary data so potentially dangerous with large size values
	static void Perform( float* data, int size )
	{
		// Do two passes, one from data into temp and then the second back to data again
		float* temp = MEM_STACKALLOC( float*, size * size * sizeof( float ) );
		Pass( data, temp, size, true );
		Pass( temp, data, size, false );
	}
};

struct FontBuilderHelper_t
{
	static bool Build( FT_Library ft_library, FontAtlas_t* pAtlas, std::uint32_t extra_flags );
	static void RegisterDefaultCustomRects( FontAtlas_t* pAtlas );
	static void SetupFont( FontAtlas_t* pAtlas, Font_t* pFont, FontConfig_t* pConfig, float ascent, float descent );
	static void PackCustomRects( FontAtlas_t* pAtlas, void* stbrp_context_opaque );
	static void Finish( FontAtlas_t* pAtlas );
	static void MultiplyCalcLookupTable( std::uint8_t out_table[ 256 ], float in_multiply_factor );
	static void MultiplyRectAlpha8( const std::uint8_t table[ 256 ], std::uint8_t* pixels, int x, int y, int w, int h, int stride );

	static void RenderDefaultTexData( FontAtlas_t* pAtlas )
	{
		CRT_ASSERTION( pAtlas->iMainRectId >= 0 );
		CRT_ASSERTION( pAtlas->pTexPixelsAlpha8 != NULL );
		FontAtlasCustomRect_t& r = pAtlas->vecCustomRects[ pAtlas->iMainRectId ];
		CRT_ASSERTION( r.ID == FONT_ATLAS_DEFAULT_TEX_DATA_ID );
		CRT_ASSERTION( r.IsPacked( ) );

		const int w = pAtlas->iTexWidth;

		CRT_ASSERTION( r.W == 2 && r.H == 2 );
		const int offset = ( int )( r.X ) + ( int )( r.Y ) * w;
		pAtlas->pTexPixelsAlpha8[ offset ] = pAtlas->pTexPixelsAlpha8[ offset + 1 ] = pAtlas->pTexPixelsAlpha8[ offset + w ] = pAtlas->pTexPixelsAlpha8[ offset + w + 1 ] = 0xFF;

		pAtlas->vecTexUvWhitePixel = Vector2D_t( ( r.X + 0.5f ) * pAtlas->vecTexUvScale.x, ( r.Y + 0.5f ) * pAtlas->vecTexUvScale.y );
	}

	static void RegisterShadowCustomRects( FontAtlas_t* pAtlas )
	{
		if ( pAtlas->iShadowRectId >= 0 )
			return;

		// The actual size we want to reserve, including padding
		const FontAtlasShadowTexConfig_t* shadow_cfg = &pAtlas->ShadowTexConfig;
		const std::uint32_t effective_size = shadow_cfg->CalcTexSize( ) + shadow_cfg->GetPadding( );
		pAtlas->iShadowRectId = pAtlas->AddCustomRectRegular( 0x110000, effective_size, effective_size );
	}

	// Generate the actual pixel data for rounded corners in the pAtlas
	static void RenderShadowTexData( FontAtlas_t* pAtlas )
	{
		CRT_ASSERTION( pAtlas->pTexPixelsAlpha8 != NULL );
		CRT_ASSERTION( pAtlas->iShadowRectId >= 0 );

		struct RectangleHelper_t
		{
			// calculates the signed distance from samplePos to the nearest point on the rectangle defined by rectMin-rectMax
			static float CalcDistanceFromRect( Vector2D_t samplePos, Vector2D_t rectMin, Vector2D_t rectMax )
			{
				Vector2D_t rect_centre = ( rectMin + rectMax ) * 0.5f;
				Vector2D_t rect_half_size = ( rectMax - rectMin ) * 0.5f;

				Vector2D_t local_sample_pos = samplePos - rect_centre;

				Vector2D_t axis_dist = Vector2D_t( M_FABS( local_sample_pos.x ), M_FABS( local_sample_pos.y ) ) - rect_half_size;

				float out_dist = VectorLength( Vector2D_t( CRT::Max( axis_dist.x, 0.0f ), CRT::Max( axis_dist.y, 0.0f ) ), 0.00001f );
				float in_dist = CRT::Min( CRT::Max( axis_dist.x, axis_dist.y ), 0.0f );

				return out_dist + in_dist;
			}
		};

		// Because of the blur, we have to generate the full 3x3 texture here, and then we chop that down to just the 2x2 section we need later.
		// 'size' correspond to the our 3x3 size, whereas 'shadow_tex_size' correspond to our 2x2 version where duplicate mirrored corners are not stored.
		const FontAtlasShadowTexConfig_t* shadow_cfg = &pAtlas->ShadowTexConfig;
		const int size = shadow_cfg->TexCornerSize + shadow_cfg->TexEdgeSize + shadow_cfg->TexCornerSize;
		const int corner_size = shadow_cfg->TexCornerSize;
		const int edge_size = shadow_cfg->TexEdgeSize;

		// The bounds of the rectangle we are generating the shadow from
		const Vector2D_t shadow_rect_min( ( float )corner_size, ( float )corner_size );
		const Vector2D_t shadow_rect_max( ( float )( corner_size + edge_size ), ( float )( corner_size + edge_size ) );

		// Render the texture
		FontAtlasCustomRect_t r = pAtlas->vecCustomRects[ pAtlas->iShadowRectId ];

		// Remove the padding we added
		const int padding = shadow_cfg->GetPadding( );
		r.X += ( std::uint16_t )padding;
		r.Y += ( std::uint16_t )padding;
		r.W -= ( std::uint16_t )padding * 2;
		r.H -= ( std::uint16_t )padding * 2;

		// We draw the actual texture content by evaluating the distance field for the inner rectangle
		// Generate distance field
		float* tex_data = MEM_STACKALLOC( float*, size * size * sizeof( float ) );
		for ( int y = 0; y < size; y++ )
			for ( int x = 0; x < size; x++ )
			{
				float dist = RectangleHelper_t::CalcDistanceFromRect( Vector2D_t( ( float )x, ( float )y ), shadow_rect_min, shadow_rect_max );
				float alpha = 1.0f - CRT::Min( CRT::Max( dist + shadow_cfg->TexDistanceFieldOffset, 0.0f ) / CRT::Max( shadow_cfg->TexCornerSize + shadow_cfg->TexDistanceFieldOffset, 0.001f ), 1.0f );
				alpha = M_POW( alpha, shadow_cfg->TexFalloffPower ); // Apply power curve to give a nicer falloff
				tex_data[ x + ( y * size ) ] = alpha;
			}

		// Blur
		if ( shadow_cfg->TexBlur )
			Gaussian_t::Perform( tex_data, size );

		// Copy to texture, truncating to the actual required texture size (the bottom/right of the source data is chopped off, as we don't need it - see below). The truncated size is essentially the top 2x2 of our data, plus a little bit of padding for sampling.
		const int tex_w = pAtlas->iTexWidth;
		const int shadow_tex_size = shadow_cfg->CalcTexSize( );
		for ( int y = 0; y < shadow_tex_size; y++ )
			for ( int x = 0; x < shadow_tex_size; x++ )
			{
				const float alpha = tex_data[ x + ( y * size ) ];
				const std::uint32_t offset = ( int )( r.X + x ) + ( int )( r.Y + y ) * tex_w;
				pAtlas->pTexPixelsAlpha8[ offset ] = ( std::uint8_t )( 0xFF * alpha );
			}

		// Generate UVs for each of the nine sections, which are arranged in a 3x3 grid starting from 0 in the top-left and going across then down
		for ( int i = 0; i < 9; i++ )
		{
			FontAtlasCustomRect_t sub_rect = r;

			// The third row/column of the 3x3 grid are generated by flipping the appropriate chunks of the upper 2x2 grid.
			bool flip_h = false; // Do we need to flip the UVs horizontally?
			bool flip_v = false; // Do we need to flip the UVs vertically?

			switch ( i % 3 )
			{
				case 0:
					sub_rect.W = ( std::uint16_t )corner_size;
					break;
				case 1:
					sub_rect.X += ( std::uint16_t )corner_size;
					sub_rect.W = ( std::uint16_t )edge_size;
					break;
				case 2:
					sub_rect.W = ( std::uint16_t )corner_size;
					flip_h = true;
					break;
			}

			switch ( i / 3 )
			{
				case 0:
					sub_rect.H = ( std::uint16_t )corner_size;
					break;
				case 1:
					sub_rect.Y += ( std::uint16_t )corner_size;
					sub_rect.H = ( std::uint16_t )edge_size;
					break;
				case 2:
					sub_rect.H = ( std::uint16_t )corner_size;
					flip_v = true;
					break;
			}

			Vector2D_t uv0, uv1;
			pAtlas->CalcCustomRectUV( &sub_rect, &uv0, &uv1 );
			pAtlas->ShadowRectUvs[ i ] = Vector4D_t( flip_h ? uv1.x : uv0.x, flip_v ? uv1.y : uv0.y, flip_h ? uv0.x : uv1.x, flip_v ? uv0.y : uv1.y );
		}
	}
};

// Default pFont TTF is compressed with stb_compress then base85 encoded (see misc/fonts/binary_to_compressed_c.cpp for encoder)
static std::uint32_t stb_decompress_length( const std::uint8_t* input );
static std::uint32_t stb_decompress( std::uint8_t* output, const std::uint8_t* input, std::uint32_t length );

Font_t* FontAtlas_t::AddFontDefault( const FontConfig_t* font_cfg_template )
{
	FontConfig_t cfg = font_cfg_template ? *font_cfg_template : FontConfig_t( );
	if ( !font_cfg_template )
	{
		cfg.iOversampleH = cfg.iOversampleV = 1;
		cfg.bPixelSnapH = true;
	}
	if ( cfg.flSizePixels <= 0.0f )
		cfg.flSizePixels = 13.0f * 1.0f;

	cfg.nRasterizerFlags = kFontRasterizerFlags_LightHinting;

	Font_t* pFont = AddFontFromFileTTF( "C:\\Windows\\Fonts\\Tahoma.ttf", cfg.flSizePixels, &cfg, GetGlyphRangesCyrillic( ) );
	pFont->vecDisplayOffset.y = 1.0f;

	if ( cfg.szName[ 0 ] == '\0' )
		CRT::StringPrintN( cfg.szName, CRT_ARRAYSIZE( cfg.szName ), "Tahoma.ttf, %dpx", ( int )cfg.flSizePixels );
	return pFont;
}

Font_t* FontAtlas_t::AddFontFromFileTTF( const char* filename, float size_pixels, const FontConfig_t* font_cfg_template, const Wchar_t* glyph_ranges )
{
	CRT_ASSERTION( !bLocked && "Cannot modify a locked FontAtlas_t between NewFrame() and EndFrame/Render()!" );
	size_t nDataSize = 0;
	void* pData = FILEHELPERS::LoadFileToMemory( filename, &nDataSize, 0 );
	if ( !pData )
	{
		CRT_ASSERTION( 0 && "Could not load pFont file!" );
		return NULL;
	}
	FontConfig_t cfg = font_cfg_template ? *font_cfg_template : FontConfig_t( );
	if ( cfg.szName[ 0 ] == '\0' )
	{
		// Store a short copy of filename into into the pFont name for convenience
		const char* p;
		for ( p = filename + CRT::StringLength( filename ); p > filename && p[ -1 ] != '/' && p[ -1 ] != '\\'; p-- )
		{
		}
		CRT::StringPrintN( cfg.szName, CRT_ARRAYSIZE( cfg.szName ), "%s, %.0fpx", p, size_pixels );
	}
	return AddFontFromMemoryTTF( pData, ( int )nDataSize, size_pixels, &cfg, glyph_ranges );
}

// NB: Transfer ownership of 'ttf_data' to FontAtlas_t, unless font_cfg_template->bFontDataOwnedByAtlas == false. Owned TTF buffer will be deleted after Build().
Font_t* FontAtlas_t::AddFontFromMemoryTTF( void* ttf_data, int ttf_size, float size_pixels, const FontConfig_t* font_cfg_template, const Wchar_t* glyph_ranges )
{
	CRT_ASSERTION( !bLocked && "Cannot modify a locked FontAtlas_t between NewFrame() and EndFrame/Render()!" );
	FontConfig_t cfg = font_cfg_template ? *font_cfg_template : FontConfig_t( );
	CRT_ASSERTION( cfg.pFontData == NULL );
	cfg.pFontData = ttf_data;
	cfg.iFontDataSize = ttf_size;
	cfg.flSizePixels = size_pixels;
	if ( glyph_ranges )
		cfg.pGlyphRanges = glyph_ranges;
	return AddFont( &cfg );
}

Font_t* FontAtlas_t::AddFontFromMemoryCompressedTTF( const void* compressed_ttf_data, int compressed_ttf_size, float size_pixels, const FontConfig_t* font_cfg_template, const Wchar_t* glyph_ranges )
{
	const std::uint32_t buf_decompressed_size = stb_decompress_length( ( const std::uint8_t* )compressed_ttf_data );
	std::uint8_t* buf_decompressed_data = MEM_HEAPALLOC( std::uint8_t*, buf_decompressed_size );
	stb_decompress( buf_decompressed_data, ( const std::uint8_t* )compressed_ttf_data, ( std::uint32_t )compressed_ttf_size );

	FontConfig_t cfg = font_cfg_template ? *font_cfg_template : FontConfig_t( );
	CRT_ASSERTION( cfg.pFontData == NULL );
	cfg.bFontDataOwnedByAtlas = true;
	return AddFontFromMemoryTTF( buf_decompressed_data, ( int )buf_decompressed_size, size_pixels, &cfg, glyph_ranges );
}

size_t FontAtlas_t::AddCustomRectRegular( std::uint32_t id, int width, int height )
{
	// Breaking change on 2019/11/21 (1.74): FontAtlas_t::AddCustomRectRegular() now requires an ID >= 0x110000 (instead of >= 0x10000)
	CRT_ASSERTION( id >= 0x110000 );
	CRT_ASSERTION( width > 0 && width <= DRAW_UNICODE_CODEPOINT_MAX );
	CRT_ASSERTION( height > 0 && height <= DRAW_UNICODE_CODEPOINT_MAX );
	FontAtlasCustomRect_t r;
	r.ID = id;
	r.W = ( std::uint16_t )width;
	r.H = ( std::uint16_t )height;
	vecCustomRects.push_back( r );
	return vecCustomRects.Size - 1U; // Return index
}

size_t FontAtlas_t::AddCustomRectFontGlyph( Font_t* pFont, Wchar_t id, int width, int height, float advance_x, const Vector2D_t& offset )
{
	CRT_ASSERTION( pFont != NULL );
	CRT_ASSERTION( width > 0 && width <= DRAW_UNICODE_CODEPOINT_MAX );
	CRT_ASSERTION( height > 0 && height <= DRAW_UNICODE_CODEPOINT_MAX );
	FontAtlasCustomRect_t r;
	r.ID = id;
	r.W = ( std::uint16_t )width;
	r.H = ( std::uint16_t )height;
	r.flGlyphAdvanceX = advance_x;
	r.vecGlyphOffset = offset;
	r.pFont = pFont;
	vecCustomRects.push_back( r );
	return vecCustomRects.Size - 1U; // Return index
}

void FontAtlas_t::CalcCustomRectUV( const FontAtlasCustomRect_t* rect, Vector2D_t* out_uv_min, Vector2D_t* out_uv_max ) const
{
	CRT_ASSERTION( iTexWidth > 0 && iTexHeight > 0 ); // pFont pAtlas needs to be built before we can calculate UV coordinates
	CRT_ASSERTION( rect->IsPacked( ) ); // Make sure the rectangle has been packed
	*out_uv_min = Vector2D_t( ( float )rect->X * vecTexUvScale.x, ( float )rect->Y * vecTexUvScale.y );
	*out_uv_max = Vector2D_t( ( float )( rect->X + rect->W ) * vecTexUvScale.x, ( float )( rect->Y + rect->H ) * vecTexUvScale.y );
}

namespace
{
	// Glyph metrics:
	// --------------
	//
	//                       xmin                     xmax
	//                        |                         |
	//                        |<-------- width -------->|
	//                        |                         |
	//              |         +-------------------------+----------------- ymax
	//              |         |    ggggggggg   ggggg    |     ^        ^
	//              |         |   g:::::::::ggg::::g    |     |        |
	//              |         |  g:::::::::::::::::g    |     |        |
	//              |         | g::::::ggggg::::::gg    |     |        |
	//              |         | g:::::g     g:::::g     |     |        |
	//    offsetX  -|-------->| g:::::g     g:::::g     |  offsetY     |
	//              |         | g:::::g     g:::::g     |     |        |
	//              |         | g::::::g    g:::::g     |     |        |
	//              |         | g:::::::ggggg:::::g     |     |        |
	//              |         |  g::::::::::::::::g     |     |      height
	//              |         |   gg::::::::::::::g     |     |        |
	//  baseline ---*---------|---- gggggggg::::::g-----*--------      |
	//            / |         |             g:::::g     |              |
	//     origin   |         | gggggg      g:::::g     |              |
	//              |         | g:::::gg   gg:::::g     |              |
	//              |         |  g::::::ggg:::::::g     |              |
	//              |         |   gg:::::::::::::g      |              |
	//              |         |     ggg::::::ggg        |              |
	//              |         |         gggggg          |              v
	//              |         +-------------------------+----------------- ymin
	//              |                                   |
	//              |------------- advanceX ----------->|

	/// A structure that describe a glyph.
	struct GlyphInfo_t
	{
		int Width; // Glyph's width in pixels.
		int Height; // Glyph's height in pixels.
		FT_Int OffsetX; // The distance from the origin ("pen position") to the left of the glyph.
		FT_Int OffsetY; // The distance from the origin to the top of the glyph. This is usually a value < 0.
		float AdvanceX; // The distance from the origin to the origin of the next glyph. This is usually a value > 0.
	};

	// Font parameters and metrics.
	struct FontInfo_t
	{
		uint32_t PixelHeight; // Size this font was generated with.
		float Ascender; // The pixel extents above the baseline in pixels (typically positive).
		float Descender; // The extents below the baseline in pixels (typically negative).
		float LineSpacing; // The baseline-to-baseline distance. Note that it usually is larger than the sum of the ascender and descender taken as absolute values. There is also no guarantee that no glyphs extend above or below subsequent baselines when using this distance. Think of it as a value the designer of the font finds appropriate.
		float LineGap; // The spacing in pixels between one row's descent and the next row's ascent.
		float MaxAdvanceWidth; // This field gives the maximum horizontal cursor advance for all glyphs in the font.
	};

	// FreeType glyph rasterizer.
	// NB: No ctor/dtor, explicitly call Init()/Shutdown()
	struct FreeTypeFont_t
	{
		bool InitFont( FT_Library ft_library, const FontConfig_t& cfg, std::uint32_t extra_user_flags ); // Initialize from an external data buffer. Doesn't copy data, and you must ensure it stays valid up to this object lifetime.
		void CloseFont( );
		void SetPixelHeight( int pixel_height ); // Change font pixel size. All following calls to RasterizeGlyph() will use this size
		const FT_Glyph_Metrics* LoadGlyph( uint32_t in_codepoint );
		const FT_Bitmap* RenderGlyphAndGetInfo( GlyphInfo_t* out_glyph_info );
		void BlitGlyph( const FT_Bitmap* ft_bitmap, uint8_t* dst, uint32_t dst_pitch, std::uint8_t* multiply_table = NULL );

		~FreeTypeFont_t( )
		{
			CloseFont( );
		}

		// [Internals]
		FontInfo_t Info; // Font descriptor of the current font.
		FT_Face Face;
		std::uint32_t UserFlags; // = FontConfig_t::RasterizerFlags
		FT_Int32 LoadFlags;
		FT_Render_Mode RenderMode;
	};

	// From SDL_ttf: Handy routines for converting from fixed point
#define FT_CEIL(X) (((X + 63) & -64) / 64)

	bool FreeTypeFont_t::InitFont( FT_Library ft_library, const FontConfig_t& cfg, std::uint32_t extra_user_flags )
	{
		FT_Error error = FT_New_Memory_Face( ft_library, ( uint8_t* )cfg.pFontData, ( uint32_t )cfg.iFontDataSize, ( uint32_t )cfg.iFontNo, &Face );
		if ( error != 0 )
			return false;
		error = FT_Select_Charmap( Face, FT_ENCODING_UNICODE );
		if ( error != 0 )
			return false;

		CRT::MemorySet( &Info, 0, sizeof( Info ) );
		SetPixelHeight( ( uint32_t )cfg.flSizePixels );

		// Convert to FreeType flags (NB: Bold and Oblique are processed separately)
		UserFlags = cfg.nRasterizerFlags | extra_user_flags;
		LoadFlags = FT_LOAD_NO_BITMAP;
		if ( UserFlags & kFontRasterizerFlags_NoHinting )
			LoadFlags |= FT_LOAD_NO_HINTING;
		if ( UserFlags & kFontRasterizerFlags_NoAutoHint )
			LoadFlags |= FT_LOAD_NO_AUTOHINT;
		if ( UserFlags & kFontRasterizerFlags_ForceAutoHint )
			LoadFlags |= FT_LOAD_FORCE_AUTOHINT;
		if ( UserFlags & kFontRasterizerFlags_LightHinting )
			LoadFlags |= FT_LOAD_TARGET_LIGHT;
		else if ( UserFlags & kFontRasterizerFlags_MonoHinting )
			LoadFlags |= FT_LOAD_TARGET_MONO;
		else
			LoadFlags |= FT_LOAD_TARGET_NORMAL;

		if ( UserFlags & FT_LOAD_MONOCHROME )
			RenderMode = FT_RENDER_MODE_MONO;
		else
			RenderMode = FT_RENDER_MODE_NORMAL;

		return true;
	}

	void FreeTypeFont_t::CloseFont( )
	{
		if ( Face )
		{
			FT_Done_Face( Face );
			Face = NULL;
		}
	}

	void FreeTypeFont_t::SetPixelHeight( int pixel_height )
	{
		// Vuhdo: I'm not sure how to deal with font sizes properly. As far as I understand, currently ImGui assumes that the 'pixel_height'
		// is a maximum height of an any given glyph, i.e. it's the sum of font's ascender and descender. Seems strange to me.
		// NB: FT_Set_Pixel_Sizes() doesn't seem to get us the same result.
		FT_Size_RequestRec req;
		req.type = FT_SIZE_REQUEST_TYPE_REAL_DIM;
		req.width = 0;
		req.height = ( uint32_t )pixel_height * 64;
		req.horiResolution = 0;
		req.vertResolution = 0;
		FT_Request_Size( Face, &req );

		// Update font info
		FT_Size_Metrics metrics = Face->size->metrics;
		Info.PixelHeight = ( uint32_t )pixel_height;
		Info.Ascender = ( float )FT_CEIL( metrics.ascender );
		Info.Descender = ( float )FT_CEIL( metrics.descender );
		Info.LineSpacing = ( float )FT_CEIL( metrics.height );
		Info.LineGap = ( float )FT_CEIL( metrics.height - metrics.ascender + metrics.descender );
		Info.MaxAdvanceWidth = ( float )FT_CEIL( metrics.max_advance );
	}

	const FT_Glyph_Metrics* FreeTypeFont_t::LoadGlyph( uint32_t wcCodepoint )
	{
		uint32_t glyph_index = FT_Get_Char_Index( Face, wcCodepoint );
		if ( glyph_index == 0 )
			return NULL;
		FT_Error error = FT_Load_Glyph( Face, glyph_index, LoadFlags );
		if ( error )
			return NULL;

		// Need an outline for this to work
		FT_GlyphSlot slot = Face->glyph;
		CRT_ASSERTION( slot->format == FT_GLYPH_FORMAT_OUTLINE );

		// Apply convenience transform (this is not picking from real "Bold"/"Italic" fonts! Merely applying FreeType helper transform. Oblique == Slanting)
		if ( UserFlags & kFontRasterizerFlags_Bold )
			FT_GlyphSlot_Embolden( slot );
		if ( UserFlags & kFontRasterizerFlags_Oblique )
		{
			FT_GlyphSlot_Oblique( slot );
			//FT_BBox bbox;
			//FT_Outline_Get_BBox(&slot->outline, &bbox);
			//slot->metrics.width = bbox.xMax - bbox.xMin;
			//slot->metrics.height = bbox.yMax - bbox.yMin;
		}

		return &slot->metrics;
	}

	const FT_Bitmap* FreeTypeFont_t::RenderGlyphAndGetInfo( GlyphInfo_t* out_glyph_info )
	{
		FT_GlyphSlot slot = Face->glyph;
		FT_Error error = FT_Render_Glyph( slot, RenderMode );
		if ( error != 0 )
			return NULL;

		FT_Bitmap* ft_bitmap = &Face->glyph->bitmap;
		out_glyph_info->Width = ( int )ft_bitmap->width;
		out_glyph_info->Height = ( int )ft_bitmap->rows;
		out_glyph_info->OffsetX = Face->glyph->bitmap_left;
		out_glyph_info->OffsetY = -Face->glyph->bitmap_top;
		out_glyph_info->AdvanceX = ( float )FT_CEIL( slot->advance.x );

		return ft_bitmap;
	}

	void FreeTypeFont_t::BlitGlyph( const FT_Bitmap* ft_bitmap, uint8_t* dst, uint32_t dst_pitch, std::uint8_t* multiply_table )
	{
		CRT_ASSERTION( ft_bitmap != NULL );
		const uint32_t w = ft_bitmap->width;
		const uint32_t h = ft_bitmap->rows;
		const uint8_t* src = ft_bitmap->buffer;
		const uint32_t src_pitch = ft_bitmap->pitch;

		switch ( ft_bitmap->pixel_mode )
		{
			case FT_PIXEL_MODE_GRAY: // Grayscale image, 1 byte per pixel.
			{
				if ( multiply_table == NULL )
				{
					for ( uint32_t y = 0; y < h; y++, src += src_pitch, dst += dst_pitch )
						CRT::MemoryCopy( dst, src, w );
				}
				else
				{
					for ( uint32_t y = 0; y < h; y++, src += src_pitch, dst += dst_pitch )
						for ( uint32_t x = 0; x < w; x++ )
							dst[ x ] = multiply_table[ src[ x ] ];
				}
				break;
			}
			case FT_PIXEL_MODE_MONO: // Monochrome image, 1 bit per pixel. The bits in each byte are ordered from MSB to LSB.
			{
				uint8_t color0 = multiply_table ? multiply_table[ 0 ] : 0;
				uint8_t color1 = multiply_table ? multiply_table[ 255 ] : 255;
				for ( uint32_t y = 0; y < h; y++, src += src_pitch, dst += dst_pitch )
				{
					uint8_t bits = 0;
					const uint8_t* bits_ptr = src;
					for ( uint32_t x = 0; x < w; x++, bits <<= 1 )
					{
						if ( ( x & 7 ) == 0 )
							bits = *bits_ptr++;
						dst[ x ] = ( bits & 0x80 ) ? color1 : color0;
					}
				}
				break;
			}
			default:
				CRT_ASSERTION( 0 && "FreeTypeFont_t::BlitGlyph(): Unknown bitmap pixel mode!" );
		}
	}
}

FontAtlasShadowTexConfig_t::FontAtlasShadowTexConfig_t( )
{
	TexCornerSize = 16;
	TexEdgeSize = 1;
	TexFalloffPower = 4.8f;
	TexDistanceFieldOffset = 3.8f;
	TexBlur = true;
}

struct FontBuildSrcGlyphFT_t
{
	GlyphInfo_t Info;
	uint32_t Codepoint;
	std::uint8_t* BitmapData; // Point within one of the dst_tmp_bitmap_buffers[] array
};

struct FontBuildSrcDataFT_t
{
	FreeTypeFont_t Font;
	stbrp_rect* Rects; // Rectangle to pack. We first fill in their size and the packer will give us their position.
	const Wchar_t* SrcRanges; // Ranges as requested by user (user is allowed to request too much, e.g. 0x0020..DRAW_UNICODE_CODEPOINT_MAX)
	int DstIndex; // Index into pAtlas->Fonts[] and dst_tmp_array[]
	int GlyphsHighest; // Highest requested wcCodepoint
	int GlyphsCount; // Glyph count (excluding missing glyphs and glyphs already set by an earlier source font)
	BoolVector_t GlyphsSet; // Glyph bit map (random access, 1-bit per wcCodepoint. This will be a maximum of 8KB)
	CRT::Vector<FontBuildSrcGlyphFT_t> GlyphsList;
};

// Temporary data for one destination Font_t* (multiple source fonts can be merged into one destination Font_t)
struct ImFontBuildDstDataFT
{
	int SrcCount; // Number of source fonts targeting this destination font.
	int GlyphsHighest;
	int GlyphsCount;
	BoolVector_t GlyphsSet; // This is used to resolve collision when multiple sources are merged into a same destination font.
};

bool FontBuilderHelper_t::Build( FT_Library ft_library, FontAtlas_t* pAtlas, std::uint32_t extra_flags )
{
	CRT_ASSERTION( pAtlas->vecConfigData.Size > 0 );

	RegisterDefaultCustomRects( pAtlas );

	// Clear pAtlas
	pAtlas->pTexID = ( TextureID_t )NULL;
	pAtlas->iTexWidth = pAtlas->iTexHeight = 0;
	pAtlas->vecTexUvScale = Vector2D_t( 0.0f, 0.0f );
	pAtlas->vecTexUvWhitePixel = Vector2D_t( 0.0f, 0.0f );
	pAtlas->ClearTexData( );

	// Temporary storage for building
	CRT::Vector<FontBuildSrcDataFT_t> src_tmp_array;
	CRT::Vector<ImFontBuildDstDataFT> dst_tmp_array;
	src_tmp_array.resize( pAtlas->vecConfigData.Size );
	dst_tmp_array.resize( pAtlas->vecFonts.Size );
	CRT::MemorySet( src_tmp_array.Data, 0, ( size_t )src_tmp_array.size_in_bytes( ) );
	CRT::MemorySet( dst_tmp_array.Data, 0, ( size_t )dst_tmp_array.size_in_bytes( ) );

	// 1. Initialize font loading structure, check font data validity
	for ( size_t src_i = 0; src_i < pAtlas->vecConfigData.Size; src_i++ )
	{
		FontBuildSrcDataFT_t& src_tmp = src_tmp_array[ src_i ];
		FontConfig_t& cfg = pAtlas->vecConfigData[ src_i ];
		FreeTypeFont_t& font_face = src_tmp.Font;
		CRT_ASSERTION( cfg.pDstFont && ( !cfg.pDstFont->IsLoaded( ) || cfg.pDstFont->pContainerAtlas == pAtlas ) );

		// Find index from cfg.DstFont (we allow the user to set cfg.DstFont. Also it makes casual debugging nicer than when storing indices)
		src_tmp.DstIndex = -1;
		for ( size_t output_i = 0; output_i < pAtlas->vecFonts.Size && src_tmp.DstIndex == -1; output_i++ )
			if ( cfg.pDstFont == pAtlas->vecFonts[ output_i ] )
				src_tmp.DstIndex = static_cast< int >( output_i );
		CRT_ASSERTION( src_tmp.DstIndex != -1 ); // cfg.DstFont not pointing within pAtlas->Fonts[] array?
		if ( src_tmp.DstIndex == -1 )
			return false;

		// Load font
		if ( !font_face.InitFont( ft_library, cfg, extra_flags ) )
			return false;

		// Measure highest codepoints
		ImFontBuildDstDataFT& dst_tmp = dst_tmp_array[ src_tmp.DstIndex ];
		src_tmp.SrcRanges = cfg.pGlyphRanges ? cfg.pGlyphRanges : pAtlas->GetGlyphRangesDefault( );
		for ( const Wchar_t* src_range = src_tmp.SrcRanges; src_range[ 0 ] && src_range[ 1 ]; src_range += 2 )
			src_tmp.GlyphsHighest = CRT::Max( src_tmp.GlyphsHighest, ( int )src_range[ 1 ] );
		dst_tmp.SrcCount++;
		dst_tmp.GlyphsHighest = CRT::Max( dst_tmp.GlyphsHighest, src_tmp.GlyphsHighest );
	}

	// 2. For every requested wcCodepoint, check for their presence in the font data, and handle redundancy or overlaps between source fonts to avoid unused glyphs.
	int total_glyphs_count = 0;
	for ( size_t src_i = 0; src_i < src_tmp_array.Size; src_i++ )
	{
		FontBuildSrcDataFT_t& src_tmp = src_tmp_array[ src_i ];
		ImFontBuildDstDataFT& dst_tmp = dst_tmp_array[ src_tmp.DstIndex ];
		src_tmp.GlyphsSet.Resize( src_tmp.GlyphsHighest + 1 );
		if ( dst_tmp.GlyphsSet.storage.empty( ) )
			dst_tmp.GlyphsSet.Resize( dst_tmp.GlyphsHighest + 1 );

		for ( const Wchar_t* src_range = src_tmp.SrcRanges; src_range[ 0 ] && src_range[ 1 ]; src_range += 2 )
			for ( int wcCodepoint = src_range[ 0 ]; wcCodepoint <= src_range[ 1 ]; wcCodepoint++ )
			{
				if ( dst_tmp.GlyphsSet.GetBit( wcCodepoint ) ) // Don't overwrite existing glyphs. We could make this an option (e.g. MergeOverwrite)
					continue;
				uint32_t glyph_index = FT_Get_Char_Index( src_tmp.Font.Face, wcCodepoint ); // It is actually in the font? (FIXME-OPT: We are not storing the glyph_index..)
				if ( glyph_index == 0 )
					continue;

				// Add to avail set/counters
				src_tmp.GlyphsCount++;
				dst_tmp.GlyphsCount++;
				src_tmp.GlyphsSet.SetBit( wcCodepoint, true );
				dst_tmp.GlyphsSet.SetBit( wcCodepoint, true );
				total_glyphs_count++;
			}
	}

	// 3. Unpack our bit map into a flat list (we now have all the Unicode pPoints that we know are requested _and_ available _and_ not overlapping another)
	for ( size_t src_i = 0; src_i < src_tmp_array.Size; src_i++ )
	{
		FontBuildSrcDataFT_t& src_tmp = src_tmp_array[ src_i ];
		src_tmp.GlyphsList.reserve( src_tmp.GlyphsCount );

		CRT_ASSERTION( sizeof( src_tmp.GlyphsSet.storage.Data[ 0 ] ) == sizeof( int ) );
		const int* it_begin = src_tmp.GlyphsSet.storage.begin( );
		const int* it_end = src_tmp.GlyphsSet.storage.end( );
		for ( const int* it = it_begin; it < it_end; it++ )
			if ( int entries_32 = *it )
				for ( int bit_n = 0; bit_n < 32; bit_n++ )
					if ( entries_32 & ( 1 << bit_n ) )
					{
						FontBuildSrcGlyphFT_t src_glyph;
						CRT::MemorySet( &src_glyph, 0, sizeof( src_glyph ) );
						src_glyph.Codepoint = ( Wchar_t )( ( ( it - it_begin ) << 5 ) + bit_n );
						//src_glyph.GlyphIndex = 0; // FIXME-OPT: We had this info in the previous step and lost it..
						src_tmp.GlyphsList.push_back( src_glyph );
					}
		src_tmp.GlyphsSet.Clear( );
		CRT_ASSERTION( src_tmp.GlyphsList.Size == src_tmp.GlyphsCount );
	}
	for ( size_t dst_i = 0; dst_i < dst_tmp_array.Size; dst_i++ )
		dst_tmp_array[ dst_i ].GlyphsSet.Clear( );
	dst_tmp_array.clear( );

	// Allocate packing character data and flag packed characters buffer as non-packed (x0=y0=x1=y1=0)
	// (We technically don't need to zero-clear buf_rects, but let's do it for the sake of sanity)
	CRT::Vector<stbrp_rect> buf_rects;
	buf_rects.resize( total_glyphs_count );
	CRT::MemorySet( buf_rects.Data, 0, ( size_t )buf_rects.size_in_bytes( ) );

	// Allocate temporary rasterization data buffers.
	// We could not find a way to retrieve accurate glyph size without rendering them.
	// (e.g. slot->metrics->width not always matching bitmap->width, especially considering the Oblique transform)
	// We allocate in chunks of 256 KB to not waste too much extra memory ahead. Hopefully users of FreeType won't find the temporary allocations.
	const int BITMAP_BUFFERS_CHUNK_SIZE = 256 * 1024;
	int buf_bitmap_current_used_bytes = 0;
	CRT::Vector<std::uint8_t*> buf_bitmap_buffers;
	buf_bitmap_buffers.push_back( MEM_HEAPALLOC( std::uint8_t*, BITMAP_BUFFERS_CHUNK_SIZE ) );

	// 4. Gather glyphs sizes so we can pack them in our virtual canvas.
	// 8. Render/rasterize font characters into the texture
	int total_surface = 0;
	int buf_rects_out_n = 0;
	for ( size_t src_i = 0; src_i < src_tmp_array.Size; src_i++ )
	{
		FontBuildSrcDataFT_t& src_tmp = src_tmp_array[ src_i ];
		FontConfig_t& cfg = pAtlas->vecConfigData[ src_i ];
		if ( src_tmp.GlyphsCount == 0 )
			continue;

		src_tmp.Rects = &buf_rects[ buf_rects_out_n ];
		buf_rects_out_n += src_tmp.GlyphsCount;

		// Compute multiply table if requested
		const bool multiply_enabled = ( cfg.flRasterizerMultiply != 1.0f );
		std::uint8_t multiply_table[ 256 ];
		if ( multiply_enabled )
			MultiplyCalcLookupTable( multiply_table, cfg.flRasterizerMultiply );

		// Gather the sizes of all rectangles we will need to pack
		const int padding = pAtlas->iTexGlyphPadding;
		for ( size_t glyph_i = 0; glyph_i < src_tmp.GlyphsList.Size; glyph_i++ )
		{
			FontBuildSrcGlyphFT_t& src_glyph = src_tmp.GlyphsList[ glyph_i ];

			const FT_Glyph_Metrics* metrics = src_tmp.Font.LoadGlyph( src_glyph.Codepoint );
			CRT_ASSERTION( metrics != NULL );
			if ( metrics == NULL )
				continue;

			// Render glyph into a bitmap (currently held by FreeType)
			const FT_Bitmap* ft_bitmap = src_tmp.Font.RenderGlyphAndGetInfo( &src_glyph.Info );
			CRT_ASSERTION( ft_bitmap );

			// Allocate new temporary chunk if needed
			const int bitmap_size_in_bytes = src_glyph.Info.Width * src_glyph.Info.Height;
			if ( buf_bitmap_current_used_bytes + bitmap_size_in_bytes > BITMAP_BUFFERS_CHUNK_SIZE )
			{
				buf_bitmap_current_used_bytes = 0;
				buf_bitmap_buffers.push_back( MEM_HEAPALLOC( std::uint8_t*, BITMAP_BUFFERS_CHUNK_SIZE ) );
			}

			// Blit rasterized pixels to our temporary buffer and keep a pointer to it.
			src_glyph.BitmapData = buf_bitmap_buffers.back( ) + buf_bitmap_current_used_bytes;
			buf_bitmap_current_used_bytes += bitmap_size_in_bytes;
			src_tmp.Font.BlitGlyph( ft_bitmap, src_glyph.BitmapData, src_glyph.Info.Width * 1, multiply_enabled ? multiply_table : NULL );

			src_tmp.Rects[ glyph_i ].w = ( stbrp_coord )( src_glyph.Info.Width + padding );
			src_tmp.Rects[ glyph_i ].h = ( stbrp_coord )( src_glyph.Info.Height + padding );
			total_surface += src_tmp.Rects[ glyph_i ].w * src_tmp.Rects[ glyph_i ].h;
		}
	}

	// We need a width for the skyline algorithm, any width!
	// The exact width doesn't really matter much, but some API/GPU have texture size limitations and increasing width can decrease height.
	// User can override TexDesiredWidth and TexGlyphPadding if they wish, otherwise we use a simple heuristic to select the width based on expected surface.
	const int surface_sqrt = ( int )M_SQRT( ( float )total_surface ) + 1;
	pAtlas->iTexHeight = 0;
	if ( pAtlas->iTexDesiredWidth > 0 )
		pAtlas->iTexWidth = pAtlas->iTexDesiredWidth;
	else
		pAtlas->iTexWidth = ( surface_sqrt >= 4096 * 0.7f ) ? 4096 : ( surface_sqrt >= 2048 * 0.7f ) ? 2048 :
		( surface_sqrt >= 1024 * 0.7f ) ? 1024 :
		512;

	// 5. Start packing
	// Pack our extra data rectangles first, so it will be on the upper-left corner of our texture (UV will have small values).
	const int TEX_HEIGHT_MAX = 1024 * 32;
	const int num_nodes_for_packing_algorithm = pAtlas->iTexWidth - pAtlas->iTexGlyphPadding;
	CRT::Vector<stbrp_node> pack_nodes;
	pack_nodes.resize( num_nodes_for_packing_algorithm );
	stbrp_context pack_context;
	stbrp_init_target( &pack_context, pAtlas->iTexWidth, TEX_HEIGHT_MAX, pack_nodes.Data, pack_nodes.Size );
	PackCustomRects( pAtlas, &pack_context );

	// 6. Pack each source font. No rendering yet, we are working with rectangles in an infinitely tall texture at this point.
	for ( size_t src_i = 0; src_i < src_tmp_array.Size; src_i++ )
	{
		FontBuildSrcDataFT_t& src_tmp = src_tmp_array[ src_i ];
		if ( src_tmp.GlyphsCount == 0 )
			continue;

		stbrp_pack_rects( &pack_context, src_tmp.Rects, src_tmp.GlyphsCount );

		// Extend texture height and mark missing glyphs as non-packed so we won't render them.
		// FIXME: We are not handling packing failure here (would happen if we got off TEX_HEIGHT_MAX or if a single if larger than TexWidth?)
		for ( int glyph_i = 0; glyph_i < src_tmp.GlyphsCount; glyph_i++ )
			if ( src_tmp.Rects[ glyph_i ].was_packed )
				pAtlas->iTexHeight = CRT::Max( pAtlas->iTexHeight, src_tmp.Rects[ glyph_i ].y + src_tmp.Rects[ glyph_i ].h );
	}

	// 7. Allocate texture
	pAtlas->iTexHeight = ( pAtlas->nFlags & kDrawFontAtlasFlags_NoPowerOfTwoHeight ) ? ( pAtlas->iTexHeight + 1 ) : M::UpperPowerOfTwo( pAtlas->iTexHeight );
	pAtlas->vecTexUvScale = Vector2D_t( 1.0f / pAtlas->iTexWidth, 1.0f / pAtlas->iTexHeight );
	pAtlas->pTexPixelsAlpha8 = MEM_HEAPALLOC( std::uint8_t*, pAtlas->iTexWidth * pAtlas->iTexHeight );
	CRT::MemorySet( pAtlas->pTexPixelsAlpha8, 0, pAtlas->iTexWidth * pAtlas->iTexHeight );

	// 8. Copy rasterized font characters back into the main texture
	// 9. Setup Font_t and glyphs for runtime
	for ( size_t src_i = 0; src_i < src_tmp_array.Size; src_i++ )
	{
		FontBuildSrcDataFT_t& src_tmp = src_tmp_array[ src_i ];
		if ( src_tmp.GlyphsCount == 0 )
			continue;

		FontConfig_t& cfg = pAtlas->vecConfigData[ src_i ];
		Font_t* dst_font = cfg.pDstFont; // We can have multiple input fonts writing into a same destination font (when using MergeMode=true)

		const float ascent = src_tmp.Font.Info.Ascender;
		const float descent = src_tmp.Font.Info.Descender;
		SetupFont( pAtlas, dst_font, &cfg, ascent, descent );
		const float font_off_x = cfg.vecGlyphOffset.x;
		const float font_off_y = cfg.vecGlyphOffset.y + M_ROUND( dst_font->flAscent );

		const int padding = pAtlas->iTexGlyphPadding; /*CRT::Max(M_FABS(pAtlas->vecTexGlyphShadowOffset.x), M_FABS(pAtlas->vecTexGlyphShadowOffset.y));*/
		for ( int glyph_i = 0; glyph_i < src_tmp.GlyphsCount; glyph_i++ )
		{
			FontBuildSrcGlyphFT_t& src_glyph = src_tmp.GlyphsList[ glyph_i ];
			stbrp_rect& pack_rect = src_tmp.Rects[ glyph_i ];
			CRT_ASSERTION( pack_rect.was_packed );

			GlyphInfo_t& info = src_glyph.Info;
			CRT_ASSERTION( info.Width + padding <= pack_rect.w );
			CRT_ASSERTION( info.Height + padding <= pack_rect.h );
			const int tx = pack_rect.x + padding;
			const int ty = pack_rect.y + padding;

			// Blit from temporary buffer to final texture
			size_t blit_src_stride = ( size_t )src_glyph.Info.Width;
			size_t blit_dst_stride = ( size_t )pAtlas->iTexWidth;
			std::uint8_t* blit_src = src_glyph.BitmapData;
			std::uint8_t* blit_dst = pAtlas->pTexPixelsAlpha8 + ( ty * blit_dst_stride ) + tx;
			for ( int y = info.Height; y > 0; y--, blit_dst += blit_dst_stride, blit_src += blit_src_stride )
				CRT::MemoryCopy( blit_dst, blit_src, blit_src_stride );

			float char_advance_x_org = info.AdvanceX;
			float char_advance_x_mod = CRT::Clamp( char_advance_x_org, cfg.flGlyphMinAdvanceX, cfg.flGlyphMaxAdvanceX );
			float char_off_x = font_off_x;
			if ( char_advance_x_org != char_advance_x_mod )
				char_off_x += cfg.bPixelSnapH ? M_FLOOR( ( char_advance_x_mod - char_advance_x_org ) * 0.5f ) : ( char_advance_x_mod - char_advance_x_org ) * 0.5f;

			//const Vector2D_t& shadow_off = pAtlas->vecTexGlyphShadowOffset;
			const Vector2D_t& uv_scale = pAtlas->vecTexUvScale;

			//Vector4D_t offset = Vector4D_t(0.0f, 0.0f, 0.0f, 0.0f);
			//offset.x = (shadow_off.x > 0.0f ? 0.0f : shadow_off.x);
			//offset.y = (shadow_off.y > 0.0f ? 0.0f : shadow_off.y);
			//offset.z = (shadow_off.x > 0.0f ? shadow_off.x : 0.0f);
			//offset.w = (shadow_off.y > 0.0f ? shadow_off.y : 0.0f);

			// Register glyph
			float x0 = info.OffsetX + char_off_x;
			float y0 = info.OffsetY + font_off_y;
			float x1 = x0 + info.Width;
			float y1 = y0 + info.Height;
			float u0 = ( tx ) / ( float )pAtlas->iTexWidth;
			float v0 = ( ty ) / ( float )pAtlas->iTexHeight;
			float u1 = ( tx + info.Width ) / ( float )pAtlas->iTexWidth;
			float v1 = ( ty + info.Height ) / ( float )pAtlas->iTexHeight;

			//dst_font->AddGlyph((Wchar_t)src_glyph.Codepoint,
			//x0 + offset.x,
			//y0 + offset.y,
			//x1 + offset.z,
			//y1 + offset.w,
			//u0 + (uv_scale.x * offset.x),
			//v0 + (uv_scale.y * offset.y),
			//u1 + (uv_scale.x * offset.z),
			//v1 + (uv_scale.y * offset.w),
			//char_advance_x_mod);
			dst_font->AddGlyph( ( Wchar_t )src_glyph.Codepoint, x0, y0, x1, y1, u0, v0, u1, v1, char_advance_x_mod );
		}

		src_tmp.Rects = NULL;
	}

	// Cleanup
	for ( size_t buf_i = 0; buf_i < buf_bitmap_buffers.Size; buf_i++ )
		MEM_HEAPFREE( buf_bitmap_buffers[ buf_i ] );
	for ( size_t src_i = 0; src_i < src_tmp_array.Size; src_i++ )
		src_tmp_array[ src_i ].~FontBuildSrcDataFT_t( );

	Finish( pAtlas );

	return true;
}

void FontBuilderHelper_t::RegisterDefaultCustomRects( FontAtlas_t* pAtlas )
{
	if ( pAtlas->iMainRectId >= 0 )
		return;

	pAtlas->iMainRectId = pAtlas->AddCustomRectRegular( FONT_ATLAS_DEFAULT_TEX_DATA_ID, 2, 2 );

	RegisterShadowCustomRects( pAtlas );
}

void FontBuilderHelper_t::SetupFont( FontAtlas_t* pAtlas, Font_t* pFont, FontConfig_t* pConfig, float ascent, float descent )
{
	if ( !pConfig->bMergeMode )
	{
		pFont->ClearOutputData( );
		pFont->flFontSize = pConfig->flSizePixels;
		pFont->pConfig = pConfig;
		pFont->pContainerAtlas = pAtlas;
		pFont->flAscent = ascent;
		pFont->flDescent = descent;
	}
	pFont->nConfigDataCount++;
}

void FontBuilderHelper_t::PackCustomRects( FontAtlas_t* pAtlas, void* stbrp_context_opaque )
{
	stbrp_context* pack_context = ( stbrp_context* )stbrp_context_opaque;
	CRT_ASSERTION( pack_context != NULL );

	CRT::Vector<FontAtlasCustomRect_t>& user_rects = pAtlas->vecCustomRects;
	CRT_ASSERTION( user_rects.Size >= 1 ); // We expect at least the default custom rects to be registered, else something went wrong.

	CRT::Vector<stbrp_rect> pack_rects;
	pack_rects.resize( user_rects.Size );
	CRT::MemorySet( pack_rects.Data, 0, ( size_t )pack_rects.size_in_bytes( ) );
	for ( size_t i = 0; i < user_rects.Size; i++ )
	{
		pack_rects[ i ].w = user_rects[ i ].W;
		pack_rects[ i ].h = user_rects[ i ].H;
	}
	stbrp_pack_rects( pack_context, &pack_rects[ 0 ], pack_rects.Size );
	for ( size_t i = 0; i < pack_rects.Size; i++ )
		if ( pack_rects[ i ].was_packed )
		{
			user_rects[ i ].X = pack_rects[ i ].x;
			user_rects[ i ].Y = pack_rects[ i ].y;
			CRT_ASSERTION( pack_rects[ i ].w == user_rects[ i ].W && pack_rects[ i ].h == user_rects[ i ].H );
			pAtlas->iTexHeight = CRT::Max( pAtlas->iTexHeight, pack_rects[ i ].y + pack_rects[ i ].h );
		}
}

void FontBuilderHelper_t::Finish( FontAtlas_t* pAtlas )
{
	// Render into our custom pData block
	FontBuilderHelper_t::RenderDefaultTexData( pAtlas );
	FontBuilderHelper_t::RenderShadowTexData( pAtlas );

	// Register custom rectangle glyphs
	for ( size_t i = 0; i < pAtlas->vecCustomRects.Size; i++ )
	{
		const FontAtlasCustomRect_t& r = pAtlas->vecCustomRects[ i ];
		if ( r.pFont == NULL || r.ID >= 0x110000 )
			continue;

		CRT_ASSERTION( r.pFont->pContainerAtlas == pAtlas );
		Vector2D_t uv0, vecUVFirst;
		pAtlas->CalcCustomRectUV( &r, &uv0, &vecUVFirst );
		r.pFont->AddGlyph( ( Wchar_t )r.ID, r.vecGlyphOffset.x, r.vecGlyphOffset.y, r.vecGlyphOffset.x + r.W, r.vecGlyphOffset.y + r.H, uv0.x, uv0.y, vecUVFirst.x, vecUVFirst.y, r.flGlyphAdvanceX );
	}

	// Build all fonts lookup tables
	for ( size_t i = 0; i < pAtlas->vecFonts.Size; i++ )
		if ( pAtlas->vecFonts[ i ]->bDirtyLookupTables )
			pAtlas->vecFonts[ i ]->BuildLookupTable( );

	// Ellipsis character is required for rendering elided szText. We prefer using U+2026 (horizontal ellipsis).
	// However some old fonts may contain ellipsis at U+0085. Here we auto-detect most suitable ellipsis character.
	// FIXME: Also note that 0x2026 is currently seldomly included in our pFont pRanges. Because of this we are more likely to use three individual dots.
	for ( size_t i = 0; i < pAtlas->vecFonts.size( ); i++ )
	{
		Font_t* pFont = pAtlas->vecFonts[ i ];
		if ( pFont->wcEllipsis != ( Wchar_t )-1 )
			continue;
		const Wchar_t ellipsis_variants[ ] = { ( Wchar_t )0x2026, ( Wchar_t )0x0085 };
		for ( int j = 0; j < CRT_ARRAYSIZE( ellipsis_variants ); j++ )
			if ( pFont->FindGlyphNoFallback( ellipsis_variants[ j ] ) != NULL ) // Verify glyph exists
			{
				pFont->wcEllipsis = ellipsis_variants[ j ];
				break;
			}
	}
}

// FreeType memory allocation callbacks
static void* FreeType_Alloc( FT_Memory memory, long size )
{
	return MEM_HEAPALLOC( void*, ( size_t )size );
}

static void FreeType_Free( FT_Memory /*memory*/, void* block )
{
	MEM_HEAPFREE( block );
}

static void* FreeType_Realloc( FT_Memory /*memory*/, long cur_size, long new_size, void* block )
{
	// Implement realloc() as we don't ask user to provide it.
	if ( block == NULL )
		return MEM_HEAPALLOC( void*, ( size_t )new_size );

	if ( new_size == 0 )
	{
		MEM_HEAPFREE( block );
		return NULL;
	}

	if ( new_size > cur_size )
	{
		void* new_block = MEM_HEAPALLOC( void*, ( size_t )new_size );
		CRT::MemoryCopy( new_block, block, ( size_t )cur_size );
		MEM_HEAPFREE( block );
		return new_block;
	}

	return block;
}

bool FontAtlas_t::Build( const FontRasterizerFlags_t nFlags )
{
	// FreeType memory management: https://www.freetype.org/freetype2/docs/design/design-4.html
	FT_MemoryRec_ memory_rec = {};
	memory_rec.user = NULL;
	memory_rec.alloc = &FreeType_Alloc;
	memory_rec.free = &FreeType_Free;
	memory_rec.realloc = &FreeType_Realloc;

	// https://www.freetype.org/freetype2/docs/reference/ft2-module_management.html#FT_New_Library
	FT_Library ft_library;
	FT_Error error = FT_New_Library( &memory_rec, &ft_library );
	if ( error != 0 )
		return false;

	// If you don't call FT_Add_Default_Modules() the rest of code may work, but FreeType won't use our custom allocator.
	FT_Add_Default_Modules( ft_library );

	CRT_ASSERTION( !bLocked && "Cannot modify a locked FontAtlas_t between NewFrame() and EndFrame/Render()!" );
	bool ret = FontBuilderHelper_t::Build( ft_library, this, nFlags );
	FT_Done_Library( ft_library );

	return ret;
}

void FontBuilderHelper_t::MultiplyCalcLookupTable( std::uint8_t out_table[ 256 ], float in_brighten_factor )
{
	for ( std::uint32_t i = 0; i < 256; i++ )
	{
		std::uint32_t value = ( std::uint32_t )( i * in_brighten_factor );
		out_table[ i ] = value > 255 ? 255 : ( value & 0xFF );
	}
}

void FontBuilderHelper_t::MultiplyRectAlpha8( const std::uint8_t table[ 256 ], std::uint8_t* pixels, int x, int y, int w, int h, int stride )
{
	std::uint8_t* pData = pixels + x + y * stride;
	for ( int j = h; j > 0; j--, pData += stride )
		for ( int i = 0; i < w; i++ )
			pData[ i ] = table[ pData[ i ] ];
}

#pragma region font_ranges

// clang-format off
const Wchar_t* FontAtlas_t::GetGlyphRangesDefault( )
{
	static const Wchar_t arrRanges[ ] = {
		0x0020,
		0x00FF, // Basic Latin + Latin Supplement
		0,
	};
	return &arrRanges[ 0 ];
}

const Wchar_t* FontAtlas_t::GetGlyphRangesKorean( )
{
	static const Wchar_t arrRanges[ ] = {
		0x0020,
		0x00FF, // Basic Latin + Latin Supplement
		0x3131,
		0x3163, // Korean alphabets
		0xAC00,
		0xD79D, // Korean characters
		0,
	};
	return &arrRanges[ 0 ];
}

const Wchar_t* FontAtlas_t::GetGlyphRangesChineseFull( )
{
	static const Wchar_t arrRanges[ ] = {
		0x0020,
		0x00FF, // Basic Latin + Latin Supplement
		0x2000,
		0x206F, // General Punctuation
		0x3000,
		0x30FF, // CJK Symbols and Punctuations, Hiragana, Katakana
		0x31F0,
		0x31FF, // Katakana Phonetic Extensions
		0xFF00,
		0xFFEF, // Half-width characters
		0x4e00,
		0x9FAF, // CJK Ideograms
		0,
	};
	return &arrRanges[ 0 ];
}

static void UnpackAccumulativeOffsetsIntoRanges( int base_codepoint, const short* accumulative_offsets, int accumulative_offsets_count, Wchar_t* pvecOutRanges )
{
	for ( int n = 0; n < accumulative_offsets_count; n++, pvecOutRanges += 2 )
	{
		pvecOutRanges[ 0 ] = pvecOutRanges[ 1 ] = ( Wchar_t )( base_codepoint + accumulative_offsets[ n ] );
		base_codepoint += accumulative_offsets[ n ];
	}
	pvecOutRanges[ 0 ] = 0;
}

const Wchar_t* FontAtlas_t::GetGlyphRangesChineseSimplifiedCommon( )
{
	// Store 2500 regularly used characters for Simplified Chinese.
	// Sourced from https://zh.wiktionary.org/wiki/%E9%99%84%E5%BD%95:%E7%8E%B0%E4%BB%A3%E6%B1%89%E8%AF%AD%E5%B8%B8%E7%94%A8%E5%AD%97%E8%A1%A8
	// This table covers 97.97% of all characters used during the month in July, 1987.
	// You can use FontGlyphRangesBuilder_t to create your own pRanges derived from this, by merging existing pRanges or adding new characters.
	// (Stored as accumulative offsets from the initial unicode wcCodepoint 0x4E00. This encoding is designed to helps us compact the source code size.)
	static const short arrAccumulativeOffsetsFrom0x4E00[ ] =
	{
		0,1,2,4,1,1,1,1,2,1,3,2,1,2,2,1,1,1,1,1,5,2,1,2,3,3,3,2,2,4,1,1,1,2,1,5,2,3,1,2,1,2,1,1,2,1,1,2,2,1,4,1,1,1,1,5,10,1,2,19,2,1,2,1,2,1,2,1,2,
		1,5,1,6,3,2,1,2,2,1,1,1,4,8,5,1,1,4,1,1,3,1,2,1,5,1,2,1,1,1,10,1,1,5,2,4,6,1,4,2,2,2,12,2,1,1,6,1,1,1,4,1,1,4,6,5,1,4,2,2,4,10,7,1,1,4,2,4,
		2,1,4,3,6,10,12,5,7,2,14,2,9,1,1,6,7,10,4,7,13,1,5,4,8,4,1,1,2,28,5,6,1,1,5,2,5,20,2,2,9,8,11,2,9,17,1,8,6,8,27,4,6,9,20,11,27,6,68,2,2,1,1,
		1,2,1,2,2,7,6,11,3,3,1,1,3,1,2,1,1,1,1,1,3,1,1,8,3,4,1,5,7,2,1,4,4,8,4,2,1,2,1,1,4,5,6,3,6,2,12,3,1,3,9,2,4,3,4,1,5,3,3,1,3,7,1,5,1,1,1,1,2,
		3,4,5,2,3,2,6,1,1,2,1,7,1,7,3,4,5,15,2,2,1,5,3,22,19,2,1,1,1,1,2,5,1,1,1,6,1,1,12,8,2,9,18,22,4,1,1,5,1,16,1,2,7,10,15,1,1,6,2,4,1,2,4,1,6,
		1,1,3,2,4,1,6,4,5,1,2,1,1,2,1,10,3,1,3,2,1,9,3,2,5,7,2,19,4,3,6,1,1,1,1,1,4,3,2,1,1,1,2,5,3,1,1,1,2,2,1,1,2,1,1,2,1,3,1,1,1,3,7,1,4,1,1,2,1,
		1,2,1,2,4,4,3,8,1,1,1,2,1,3,5,1,3,1,3,4,6,2,2,14,4,6,6,11,9,1,15,3,1,28,5,2,5,5,3,1,3,4,5,4,6,14,3,2,3,5,21,2,7,20,10,1,2,19,2,4,28,28,2,3,
		2,1,14,4,1,26,28,42,12,40,3,52,79,5,14,17,3,2,2,11,3,4,6,3,1,8,2,23,4,5,8,10,4,2,7,3,5,1,1,6,3,1,2,2,2,5,28,1,1,7,7,20,5,3,29,3,17,26,1,8,4,
		27,3,6,11,23,5,3,4,6,13,24,16,6,5,10,25,35,7,3,2,3,3,14,3,6,2,6,1,4,2,3,8,2,1,1,3,3,3,4,1,1,13,2,2,4,5,2,1,14,14,1,2,2,1,4,5,2,3,1,14,3,12,
		3,17,2,16,5,1,2,1,8,9,3,19,4,2,2,4,17,25,21,20,28,75,1,10,29,103,4,1,2,1,1,4,2,4,1,2,3,24,2,2,2,1,1,2,1,3,8,1,1,1,2,1,1,3,1,1,1,6,1,5,3,1,1,
		1,3,4,1,1,5,2,1,5,6,13,9,16,1,1,1,1,3,2,3,2,4,5,2,5,2,2,3,7,13,7,2,2,1,1,1,1,2,3,3,2,1,6,4,9,2,1,14,2,14,2,1,18,3,4,14,4,11,41,15,23,15,23,
		176,1,3,4,1,1,1,1,5,3,1,2,3,7,3,1,1,2,1,2,4,4,6,2,4,1,9,7,1,10,5,8,16,29,1,1,2,2,3,1,3,5,2,4,5,4,1,1,2,2,3,3,7,1,6,10,1,17,1,44,4,6,2,1,1,6,
		5,4,2,10,1,6,9,2,8,1,24,1,2,13,7,8,8,2,1,4,1,3,1,3,3,5,2,5,10,9,4,9,12,2,1,6,1,10,1,1,7,7,4,10,8,3,1,13,4,3,1,6,1,3,5,2,1,2,17,16,5,2,16,6,
		1,4,2,1,3,3,6,8,5,11,11,1,3,3,2,4,6,10,9,5,7,4,7,4,7,1,1,4,2,1,3,6,8,7,1,6,11,5,5,3,24,9,4,2,7,13,5,1,8,82,16,61,1,1,1,4,2,2,16,10,3,8,1,1,
		6,4,2,1,3,1,1,1,4,3,8,4,2,2,1,1,1,1,1,6,3,5,1,1,4,6,9,2,1,1,1,2,1,7,2,1,6,1,5,4,4,3,1,8,1,3,3,1,3,2,2,2,2,3,1,6,1,2,1,2,1,3,7,1,8,2,1,2,1,5,
		2,5,3,5,10,1,2,1,1,3,2,5,11,3,9,3,5,1,1,5,9,1,2,1,5,7,9,9,8,1,3,3,3,6,8,2,3,2,1,1,32,6,1,2,15,9,3,7,13,1,3,10,13,2,14,1,13,10,2,1,3,10,4,15,
		2,15,15,10,1,3,9,6,9,32,25,26,47,7,3,2,3,1,6,3,4,3,2,8,5,4,1,9,4,2,2,19,10,6,2,3,8,1,2,2,4,2,1,9,4,4,4,6,4,8,9,2,3,1,1,1,1,3,5,5,1,3,8,4,6,
		2,1,4,12,1,5,3,7,13,2,5,8,1,6,1,2,5,14,6,1,5,2,4,8,15,5,1,23,6,62,2,10,1,1,8,1,2,2,10,4,2,2,9,2,1,1,3,2,3,1,5,3,3,2,1,3,8,1,1,1,11,3,1,1,4,
		3,7,1,14,1,2,3,12,5,2,5,1,6,7,5,7,14,11,1,3,1,8,9,12,2,1,11,8,4,4,2,6,10,9,13,1,1,3,1,5,1,3,2,4,4,1,18,2,3,14,11,4,29,4,2,7,1,3,13,9,2,2,5,
		3,5,20,7,16,8,5,72,34,6,4,22,12,12,28,45,36,9,7,39,9,191,1,1,1,4,11,8,4,9,2,3,22,1,1,1,1,4,17,1,7,7,1,11,31,10,2,4,8,2,3,2,1,4,2,16,4,32,2,
		3,19,13,4,9,1,5,2,14,8,1,1,3,6,19,6,5,1,16,6,2,10,8,5,1,2,3,1,5,5,1,11,6,6,1,3,3,2,6,3,8,1,1,4,10,7,5,7,7,5,8,9,2,1,3,4,1,1,3,1,3,3,2,6,16,
		1,4,6,3,1,10,6,1,3,15,2,9,2,10,25,13,9,16,6,2,2,10,11,4,3,9,1,2,6,6,5,4,30,40,1,10,7,12,14,33,6,3,6,7,3,1,3,1,11,14,4,9,5,12,11,49,18,51,31,
		140,31,2,2,1,5,1,8,1,10,1,4,4,3,24,1,10,1,3,6,6,16,3,4,5,2,1,4,2,57,10,6,22,2,22,3,7,22,6,10,11,36,18,16,33,36,2,5,5,1,1,1,4,10,1,4,13,2,7,
		5,2,9,3,4,1,7,43,3,7,3,9,14,7,9,1,11,1,1,3,7,4,18,13,1,14,1,3,6,10,73,2,2,30,6,1,11,18,19,13,22,3,46,42,37,89,7,3,16,34,2,2,3,9,1,7,1,1,1,2,
		2,4,10,7,3,10,3,9,5,28,9,2,6,13,7,3,1,3,10,2,7,2,11,3,6,21,54,85,2,1,4,2,2,1,39,3,21,2,2,5,1,1,1,4,1,1,3,4,15,1,3,2,4,4,2,3,8,2,20,1,8,7,13,
		4,1,26,6,2,9,34,4,21,52,10,4,4,1,5,12,2,11,1,7,2,30,12,44,2,30,1,1,3,6,16,9,17,39,82,2,2,24,7,1,7,3,16,9,14,44,2,1,2,1,2,3,5,2,4,1,6,7,5,3,
		2,6,1,11,5,11,2,1,18,19,8,1,3,24,29,2,1,3,5,2,2,1,13,6,5,1,46,11,3,5,1,1,5,8,2,10,6,12,6,3,7,11,2,4,16,13,2,5,1,1,2,2,5,2,28,5,2,23,10,8,4,
		4,22,39,95,38,8,14,9,5,1,13,5,4,3,13,12,11,1,9,1,27,37,2,5,4,4,63,211,95,2,2,2,1,3,5,2,1,1,2,2,1,1,1,3,2,4,1,2,1,1,5,2,2,1,1,2,3,1,3,1,1,1,
		3,1,4,2,1,3,6,1,1,3,7,15,5,3,2,5,3,9,11,4,2,22,1,6,3,8,7,1,4,28,4,16,3,3,25,4,4,27,27,1,4,1,2,2,7,1,3,5,2,28,8,2,14,1,8,6,16,25,3,3,3,14,3,
		3,1,1,2,1,4,6,3,8,4,1,1,1,2,3,6,10,6,2,3,18,3,2,5,5,4,3,1,5,2,5,4,23,7,6,12,6,4,17,11,9,5,1,1,10,5,12,1,1,11,26,33,7,3,6,1,17,7,1,5,12,1,11,
		2,4,1,8,14,17,23,1,2,1,7,8,16,11,9,6,5,2,6,4,16,2,8,14,1,11,8,9,1,1,1,9,25,4,11,19,7,2,15,2,12,8,52,7,5,19,2,16,4,36,8,1,16,8,24,26,4,6,2,9,
		5,4,36,3,28,12,25,15,37,27,17,12,59,38,5,32,127,1,2,9,17,14,4,1,2,1,1,8,11,50,4,14,2,19,16,4,17,5,4,5,26,12,45,2,23,45,104,30,12,8,3,10,2,2,
		3,3,1,4,20,7,2,9,6,15,2,20,1,3,16,4,11,15,6,134,2,5,59,1,2,2,2,1,9,17,3,26,137,10,211,59,1,2,4,1,4,1,1,1,2,6,2,3,1,1,2,3,2,3,1,3,4,4,2,3,3,
		1,4,3,1,7,2,2,3,1,2,1,3,3,3,2,2,3,2,1,3,14,6,1,3,2,9,6,15,27,9,34,145,1,1,2,1,1,1,1,2,1,1,1,1,2,2,2,3,1,2,1,1,1,2,3,5,8,3,5,2,4,1,3,2,2,2,12,
		4,1,1,1,10,4,5,1,20,4,16,1,15,9,5,12,2,9,2,5,4,2,26,19,7,1,26,4,30,12,15,42,1,6,8,172,1,1,4,2,1,1,11,2,2,4,2,1,2,1,10,8,1,2,1,4,5,1,2,5,1,8,
		4,1,3,4,2,1,6,2,1,3,4,1,2,1,1,1,1,12,5,7,2,4,3,1,1,1,3,3,6,1,2,2,3,3,3,2,1,2,12,14,11,6,6,4,12,2,8,1,7,10,1,35,7,4,13,15,4,3,23,21,28,52,5,
		26,5,6,1,7,10,2,7,53,3,2,1,1,1,2,163,532,1,10,11,1,3,3,4,8,2,8,6,2,2,23,22,4,2,2,4,2,1,3,1,3,3,5,9,8,2,1,2,8,1,10,2,12,21,20,15,105,2,3,1,1,
		3,2,3,1,1,2,5,1,4,15,11,19,1,1,1,1,5,4,5,1,1,2,5,3,5,12,1,2,5,1,11,1,1,15,9,1,4,5,3,26,8,2,1,3,1,1,15,19,2,12,1,2,5,2,7,2,19,2,20,6,26,7,5,
		2,2,7,34,21,13,70,2,128,1,1,2,1,1,2,1,1,3,2,2,2,15,1,4,1,3,4,42,10,6,1,49,85,8,1,2,1,1,4,4,2,3,6,1,5,7,4,3,211,4,1,2,1,2,5,1,2,4,2,2,6,5,6,
		10,3,4,48,100,6,2,16,296,5,27,387,2,2,3,7,16,8,5,38,15,39,21,9,10,3,7,59,13,27,21,47,5,21,6
	};
	static Wchar_t arrRanges[ ] = // not zero-terminated
	{
		0x0020, 0x00FF, // Basic Latin + Latin Supplement
		0x2000, 0x206F, // General Punctuation
		0x3000, 0x30FF, // CJK Symbols and Punctuations, Hiragana, Katakana
		0x31F0, 0x31FF, // Katakana Phonetic Extensions
		0xFF00, 0xFFEF  // Half-width characters
	};
	static Wchar_t arrFullRanges[ CRT_ARRAYSIZE( arrRanges ) + CRT_ARRAYSIZE( arrAccumulativeOffsetsFrom0x4E00 ) * 2 + 1 ] = { 0 };
	if ( !arrFullRanges[ 0 ] )
	{
		CRT::MemoryCopy( arrFullRanges, arrRanges, sizeof( arrRanges ) );
		UnpackAccumulativeOffsetsIntoRanges( 0x4E00, arrAccumulativeOffsetsFrom0x4E00, CRT_ARRAYSIZE( arrAccumulativeOffsetsFrom0x4E00 ), arrFullRanges + CRT_ARRAYSIZE( arrRanges ) );
	}
	return &arrFullRanges[ 0 ];
}

const Wchar_t* FontAtlas_t::GetGlyphRangesJapanese( )
{
	// 1946 common ideograms code pPoints for Japanese
	// Sourced from http://theinstructionlimit.com/common-kanji-character-pRanges-for-xna-spritefont-rendering
	// FIXME: Source a list of the revised 2136 Joyo Kanji list from 2010 and rebuild this.
	// You can use FontGlyphRangesBuilder_t to create your own pRanges derived from this, by merging existing pRanges or adding new characters.
	// (Stored as accumulative offsets from the initial unicode wcCodepoint 0x4E00. This encoding is designed to helps us compact the source code size.)
	static const short arrAccumulativeOffsetsFrom0x4E00[ ] =
	{
		0,1,2,4,1,1,1,1,2,1,6,2,2,1,8,5,7,11,1,2,10,10,8,2,4,20,2,11,8,2,1,2,1,6,2,1,7,5,3,7,1,1,13,7,9,1,4,6,1,2,1,10,1,1,9,2,2,4,5,6,14,1,1,9,3,18,
		5,4,2,2,10,7,1,1,1,3,2,4,3,23,2,10,12,2,14,2,4,13,1,6,10,3,1,7,13,6,4,13,5,2,3,17,2,2,5,7,6,4,1,7,14,16,6,13,9,15,1,1,7,16,4,7,1,19,9,2,7,15,
		2,6,5,13,25,4,14,13,11,25,1,1,1,2,1,2,2,3,10,11,3,3,1,1,4,4,2,1,4,9,1,4,3,5,5,2,7,12,11,15,7,16,4,5,16,2,1,1,6,3,3,1,1,2,7,6,6,7,1,4,7,6,1,1,
		2,1,12,3,3,9,5,8,1,11,1,2,3,18,20,4,1,3,6,1,7,3,5,5,7,2,2,12,3,1,4,2,3,2,3,11,8,7,4,17,1,9,25,1,1,4,2,2,4,1,2,7,1,1,1,3,1,2,6,16,1,2,1,1,3,12,
		20,2,5,20,8,7,6,2,1,1,1,1,6,2,1,2,10,1,1,6,1,3,1,2,1,4,1,12,4,1,3,1,1,1,1,1,10,4,7,5,13,1,15,1,1,30,11,9,1,15,38,14,1,32,17,20,1,9,31,2,21,9,
		4,49,22,2,1,13,1,11,45,35,43,55,12,19,83,1,3,2,3,13,2,1,7,3,18,3,13,8,1,8,18,5,3,7,25,24,9,24,40,3,17,24,2,1,6,2,3,16,15,6,7,3,12,1,9,7,3,3,
		3,15,21,5,16,4,5,12,11,11,3,6,3,2,31,3,2,1,1,23,6,6,1,4,2,6,5,2,1,1,3,3,22,2,6,2,3,17,3,2,4,5,1,9,5,1,1,6,15,12,3,17,2,14,2,8,1,23,16,4,2,23,
		8,15,23,20,12,25,19,47,11,21,65,46,4,3,1,5,6,1,2,5,26,2,1,1,3,11,1,1,1,2,1,2,3,1,1,10,2,3,1,1,1,3,6,3,2,2,6,6,9,2,2,2,6,2,5,10,2,4,1,2,1,2,2,
		3,1,1,3,1,2,9,23,9,2,1,1,1,1,5,3,2,1,10,9,6,1,10,2,31,25,3,7,5,40,1,15,6,17,7,27,180,1,3,2,2,1,1,1,6,3,10,7,1,3,6,17,8,6,2,2,1,3,5,5,8,16,14,
		15,1,1,4,1,2,1,1,1,3,2,7,5,6,2,5,10,1,4,2,9,1,1,11,6,1,44,1,3,7,9,5,1,3,1,1,10,7,1,10,4,2,7,21,15,7,2,5,1,8,3,4,1,3,1,6,1,4,2,1,4,10,8,1,4,5,
		1,5,10,2,7,1,10,1,1,3,4,11,10,29,4,7,3,5,2,3,33,5,2,19,3,1,4,2,6,31,11,1,3,3,3,1,8,10,9,12,11,12,8,3,14,8,6,11,1,4,41,3,1,2,7,13,1,5,6,2,6,12,
		12,22,5,9,4,8,9,9,34,6,24,1,1,20,9,9,3,4,1,7,2,2,2,6,2,28,5,3,6,1,4,6,7,4,2,1,4,2,13,6,4,4,3,1,8,8,3,2,1,5,1,2,2,3,1,11,11,7,3,6,10,8,6,16,16,
		22,7,12,6,21,5,4,6,6,3,6,1,3,2,1,2,8,29,1,10,1,6,13,6,6,19,31,1,13,4,4,22,17,26,33,10,4,15,12,25,6,67,10,2,3,1,6,10,2,6,2,9,1,9,4,4,1,2,16,2,
		5,9,2,3,8,1,8,3,9,4,8,6,4,8,11,3,2,1,1,3,26,1,7,5,1,11,1,5,3,5,2,13,6,39,5,1,5,2,11,6,10,5,1,15,5,3,6,19,21,22,2,4,1,6,1,8,1,4,8,2,4,2,2,9,2,
		1,1,1,4,3,6,3,12,7,1,14,2,4,10,2,13,1,17,7,3,2,1,3,2,13,7,14,12,3,1,29,2,8,9,15,14,9,14,1,3,1,6,5,9,11,3,38,43,20,7,7,8,5,15,12,19,15,81,8,7,
		1,5,73,13,37,28,8,8,1,15,18,20,165,28,1,6,11,8,4,14,7,15,1,3,3,6,4,1,7,14,1,1,11,30,1,5,1,4,14,1,4,2,7,52,2,6,29,3,1,9,1,21,3,5,1,26,3,11,14,
		11,1,17,5,1,2,1,3,2,8,1,2,9,12,1,1,2,3,8,3,24,12,7,7,5,17,3,3,3,1,23,10,4,4,6,3,1,16,17,22,3,10,21,16,16,6,4,10,2,1,1,2,8,8,6,5,3,3,3,39,25,
		15,1,1,16,6,7,25,15,6,6,12,1,22,13,1,4,9,5,12,2,9,1,12,28,8,3,5,10,22,60,1,2,40,4,61,63,4,1,13,12,1,4,31,12,1,14,89,5,16,6,29,14,2,5,49,18,18,
		5,29,33,47,1,17,1,19,12,2,9,7,39,12,3,7,12,39,3,1,46,4,12,3,8,9,5,31,15,18,3,2,2,66,19,13,17,5,3,46,124,13,57,34,2,5,4,5,8,1,1,1,4,3,1,17,5,
		3,5,3,1,8,5,6,3,27,3,26,7,12,7,2,17,3,7,18,78,16,4,36,1,2,1,6,2,1,39,17,7,4,13,4,4,4,1,10,4,2,4,6,3,10,1,19,1,26,2,4,33,2,73,47,7,3,8,2,4,15,
		18,1,29,2,41,14,1,21,16,41,7,39,25,13,44,2,2,10,1,13,7,1,7,3,5,20,4,8,2,49,1,10,6,1,6,7,10,7,11,16,3,12,20,4,10,3,1,2,11,2,28,9,2,4,7,2,15,1,
		27,1,28,17,4,5,10,7,3,24,10,11,6,26,3,2,7,2,2,49,16,10,16,15,4,5,27,61,30,14,38,22,2,7,5,1,3,12,23,24,17,17,3,3,2,4,1,6,2,7,5,1,1,5,1,1,9,4,
		1,3,6,1,8,2,8,4,14,3,5,11,4,1,3,32,1,19,4,1,13,11,5,2,1,8,6,8,1,6,5,13,3,23,11,5,3,16,3,9,10,1,24,3,198,52,4,2,2,5,14,5,4,22,5,20,4,11,6,41,
		1,5,2,2,11,5,2,28,35,8,22,3,18,3,10,7,5,3,4,1,5,3,8,9,3,6,2,16,22,4,5,5,3,3,18,23,2,6,23,5,27,8,1,33,2,12,43,16,5,2,3,6,1,20,4,2,9,7,1,11,2,
		10,3,14,31,9,3,25,18,20,2,5,5,26,14,1,11,17,12,40,19,9,6,31,83,2,7,9,19,78,12,14,21,76,12,113,79,34,4,1,1,61,18,85,10,2,2,13,31,11,50,6,33,159,
		179,6,6,7,4,4,2,4,2,5,8,7,20,32,22,1,3,10,6,7,28,5,10,9,2,77,19,13,2,5,1,4,4,7,4,13,3,9,31,17,3,26,2,6,6,5,4,1,7,11,3,4,2,1,6,2,20,4,1,9,2,6,
		3,7,1,1,1,20,2,3,1,6,2,3,6,2,4,8,1,5,13,8,4,11,23,1,10,6,2,1,3,21,2,2,4,24,31,4,10,10,2,5,192,15,4,16,7,9,51,1,2,1,1,5,1,1,2,1,3,5,3,1,3,4,1,
		3,1,3,3,9,8,1,2,2,2,4,4,18,12,92,2,10,4,3,14,5,25,16,42,4,14,4,2,21,5,126,30,31,2,1,5,13,3,22,5,6,6,20,12,1,14,12,87,3,19,1,8,2,9,9,3,3,23,2,
		3,7,6,3,1,2,3,9,1,3,1,6,3,2,1,3,11,3,1,6,10,3,2,3,1,2,1,5,1,1,11,3,6,4,1,7,2,1,2,5,5,34,4,14,18,4,19,7,5,8,2,6,79,1,5,2,14,8,2,9,2,1,36,28,16,
		4,1,1,1,2,12,6,42,39,16,23,7,15,15,3,2,12,7,21,64,6,9,28,8,12,3,3,41,59,24,51,55,57,294,9,9,2,6,2,15,1,2,13,38,90,9,9,9,3,11,7,1,1,1,5,6,3,2,
		1,2,2,3,8,1,4,4,1,5,7,1,4,3,20,4,9,1,1,1,5,5,17,1,5,2,6,2,4,1,4,5,7,3,18,11,11,32,7,5,4,7,11,127,8,4,3,3,1,10,1,1,6,21,14,1,16,1,7,1,3,6,9,65,
		51,4,3,13,3,10,1,1,12,9,21,110,3,19,24,1,1,10,62,4,1,29,42,78,28,20,18,82,6,3,15,6,84,58,253,15,155,264,15,21,9,14,7,58,40,39,
	};
	static Wchar_t arrBaseRanges[ ] = // not zero-terminated
	{
		0x0020, 0x00FF, // Basic Latin + Latin Supplement
		0x3000, 0x30FF, // CJK Symbols and Punctuations, Hiragana, Katakana
		0x31F0, 0x31FF, // Katakana Phonetic Extensions
		0xFF00, 0xFFEF  // Half-width characters
	};
	static Wchar_t arrFullRanges[ CRT_ARRAYSIZE( arrBaseRanges ) + CRT_ARRAYSIZE( arrAccumulativeOffsetsFrom0x4E00 ) * 2 + 1 ] = { 0 };
	if ( !arrFullRanges[ 0 ] )
	{
		CRT::MemoryCopy( arrFullRanges, arrBaseRanges, sizeof( arrBaseRanges ) );
		UnpackAccumulativeOffsetsIntoRanges( 0x4E00, arrAccumulativeOffsetsFrom0x4E00, CRT_ARRAYSIZE( arrAccumulativeOffsetsFrom0x4E00 ), arrFullRanges + CRT_ARRAYSIZE( arrBaseRanges ) );
	}
	return &arrFullRanges[ 0 ];
}

const Wchar_t* FontAtlas_t::GetGlyphRangesCyrillic( )
{
	static const Wchar_t arrRanges[ ] =
	{
		0x0020, 0x00FF, // Basic Latin + Latin Supplement
		0x0400, 0x052F, // Cyrillic + Cyrillic Supplement
		0x2DE0, 0x2DFF, // Cyrillic Extended-A
		0xA640, 0xA69F, // Cyrillic Extended-B
		0,
	};
	return &arrRanges[ 0 ];
}

const Wchar_t* FontAtlas_t::GetGlyphRangesThai( )
{
	static const Wchar_t arrRanges[ ] =
	{
		0x0020, 0x00FF, // Basic Latin
		0x2010, 0x205E, // Punctuations
		0x0E00, 0x0E7F, // Thai
		0,
	};
	return &arrRanges[ 0 ];
}

const Wchar_t* FontAtlas_t::GetGlyphRangesVietnamese( )
{
	static const Wchar_t arrRanges[ ] =
	{
		0x0020, 0x00FF, // Basic Latin
		0x0102, 0x0103,
		0x0110, 0x0111,
		0x0128, 0x0129,
		0x0168, 0x0169,
		0x01A0, 0x01A1,
		0x01AF, 0x01B0,
		0x1EA0, 0x1EF9,
		0,
	};
	return &arrRanges[ 0 ];
}

// clang-format on
#pragma endregion

void FontGlyphRangesBuilder_t::AddText( const char* szText, const char* szTextEnd )
{
	while ( szTextEnd ? ( szText < szTextEnd ) : *szText )
	{
		std::uint32_t c = 0;
		int c_len = CRT::CharMultiByteToUTF32( szText, szTextEnd, &c );
		szText += c_len;
		if ( c_len == 0 )
			break;
		if ( c <= DRAW_UNICODE_CODEPOINT_MAX )
			AddChar( ( Wchar_t )c );
	}
}

void FontGlyphRangesBuilder_t::AddRanges( const Wchar_t* pRanges )
{
	for ( ; pRanges[ 0 ]; pRanges += 2 )
		for ( Wchar_t c = pRanges[ 0 ]; c <= pRanges[ 1 ]; c++ )
			AddChar( c );
}

void FontGlyphRangesBuilder_t::BuildRanges( CRT::Vector<Wchar_t>* pvecOutRanges )
{
	const int iMaxCodePoint = DRAW_UNICODE_CODEPOINT_MAX;
	for ( int n = 0; n <= iMaxCodePoint; n++ )
		if ( GetBit( n ) )
		{
			pvecOutRanges->push_back( ( Wchar_t )n );
			while ( n < iMaxCodePoint && GetBit( n + 1 ) )
				n++;
			pvecOutRanges->push_back( ( Wchar_t )n );
		}
	pvecOutRanges->push_back( 0 );
}

Font_t::Font_t( )
{
	flFontSize = 0.0f;
	flFallbackAdvanceX = 0.0f;
	wcFallback = ( Wchar_t )'?';
	wcEllipsis = ( Wchar_t )-1;
	vecDisplayOffset = Vector2D_t( 0.0f, 0.0f );
	pFallbackGlyph = NULL;
	pContainerAtlas = NULL;
	pConfig = NULL;
	nConfigDataCount = 0;
	bDirtyLookupTables = false;
	flScale = 1.0f;
	flAscent = flDescent = 0.0f;
	iMetricsTotalSurface = 0;
}

Font_t::~Font_t( )
{
	ClearOutputData( );
	//L_PRINT(LOG_INFO) << "Released \"" << this->GetDebugName() << "\" font";
}

void Font_t::ClearOutputData( )
{
	flFontSize = 0.0f;
	flFallbackAdvanceX = 0.0f;
	vecGlyphs.clear( );
	vecIndexAdvanceX.clear( );
	vecIndexLookup.clear( );
	pFallbackGlyph = NULL;
	pContainerAtlas = NULL;
	bDirtyLookupTables = true;
	flAscent = flDescent = 0.0f;
	iMetricsTotalSurface = 0;
}

void Font_t::BuildLookupTable( )
{
	int iMaxCodePoint = 0;
	for ( int i = 0; i != vecGlyphs.Size; i++ )
		iMaxCodePoint = CRT::Max( iMaxCodePoint, ( int )vecGlyphs[ i ].Codepoint );

	CRT_ASSERTION( vecGlyphs.Size < DRAW_UNICODE_CODEPOINT_MAX ); // -1 is reserved
	vecIndexAdvanceX.clear( );
	vecIndexLookup.clear( );
	bDirtyLookupTables = false;
	GrowIndex( iMaxCodePoint + 1 );
	for ( size_t i = 0; i < vecGlyphs.Size; i++ )
	{
		int wcCodepoint = ( int )vecGlyphs[ i ].Codepoint;
		vecIndexAdvanceX[ wcCodepoint ] = vecGlyphs[ i ].AdvanceX;
		vecIndexLookup[ wcCodepoint ] = ( Wchar_t )i;
	}

	// Create a glyph to handle TAB
	// FIXME: Needs proper TAB handling but it needs to be contextualized (or we could arbitrary say that each string starts at "column 0" ?)
	if ( FindGlyph( ( Wchar_t )' ' ) )
	{
		if ( vecGlyphs.back( ).Codepoint != '\t' ) // So we can call this function multiple times
			vecGlyphs.resize( vecGlyphs.Size + 1 );
		FontGlyph_t& tabGlyph = vecGlyphs.back( );
		tabGlyph = *FindGlyph( ( Wchar_t )' ' );
		tabGlyph.Codepoint = '\t';
		tabGlyph.AdvanceX *= 4;
		vecIndexAdvanceX[ ( int )tabGlyph.Codepoint ] = ( float )tabGlyph.AdvanceX;
		vecIndexLookup[ ( int )tabGlyph.Codepoint ] = ( Wchar_t )( vecGlyphs.Size - 1 );
	}

	pFallbackGlyph = FindGlyphNoFallback( wcFallback );
	flFallbackAdvanceX = pFallbackGlyph ? pFallbackGlyph->AdvanceX : 0.0f;
	for ( int i = 0; i < iMaxCodePoint + 1; i++ )
		if ( vecIndexAdvanceX[ i ] < 0.0f )
			vecIndexAdvanceX[ i ] = flFallbackAdvanceX;
}

void Font_t::SetFallbackChar( Wchar_t c )
{
	wcFallback = c;
	BuildLookupTable( );
}

void Font_t::GrowIndex( size_t new_size )
{
	CRT_ASSERTION( vecIndexAdvanceX.Size == vecIndexLookup.Size );
	if ( new_size <= vecIndexLookup.Size )
		return;
	vecIndexAdvanceX.resize( new_size, -1.0f );
	vecIndexLookup.resize( new_size, ( Wchar_t )-1 );
}

// x0/y0/x1/y1 are offset from the character upper-left layout position, in pixels. Therefore x0/y0 are often fairly close to zero.
// Not to be mistaken with texture coordinates, which are held by u0/v0/u1/v1 in normalized format (0.0..1.0 on each texture axis).
void Font_t::AddGlyph( Wchar_t wcCodepoint, float x0, float y0, float x1, float y1, float u0, float v0, float u1, float v1, float advance_x )
{
	vecGlyphs.resize( vecGlyphs.Size + 1 );
	FontGlyph_t& glyph = vecGlyphs.back( );
	glyph.Codepoint = wcCodepoint;
	glyph.X0 = x0;
	glyph.Y0 = y0;
	glyph.X1 = x1;
	glyph.Y1 = y1;
	glyph.U0 = u0;
	glyph.V0 = v0;
	glyph.U1 = u1;
	glyph.V1 = v1;
	glyph.AdvanceX = advance_x + pConfig->vecGlyphExtraSpacing.x; // Bake spacing into AdvanceX

	if ( pConfig->bPixelSnapH )
		glyph.AdvanceX = M_ROUND( glyph.AdvanceX );

	// Compute rough surface usage metrics (+1 to account for average padding, +0.99 to round)
	bDirtyLookupTables = true;
	iMetricsTotalSurface += ( int )( ( glyph.U1 - glyph.U0 ) * pContainerAtlas->iTexWidth + 1.99f ) * ( int )( ( glyph.V1 - glyph.V0 ) * pContainerAtlas->iTexHeight + 1.99f );
}

void Font_t::AddRemapChar( Wchar_t dst, Wchar_t src, bool overwrite_dst )
{
	CRT_ASSERTION( vecIndexLookup.Size > 0 ); // Currently this can only be called AFTER the pFont has been built, aka after calling FontAtlas_t::GetTexDataAs*() function.
	std::uint32_t nIndexSize = ( std::uint32_t )vecIndexLookup.Size;

	if ( dst < nIndexSize && vecIndexLookup.Data[ dst ] == ( Wchar_t )-1 && !overwrite_dst ) // 'dst' already exists
		return;
	if ( src >= nIndexSize && dst >= nIndexSize ) // both 'dst' and 'src' don't exist -> no-op
		return;

	GrowIndex( dst + 1 );
	vecIndexLookup[ dst ] = ( src < nIndexSize ) ? vecIndexLookup.Data[ src ] : ( Wchar_t )-1;
	vecIndexAdvanceX[ dst ] = ( src < nIndexSize ) ? vecIndexAdvanceX.Data[ src ] : 1.0f;
}

const FontGlyph_t* Font_t::FindGlyph( Wchar_t c ) const
{
	if ( c >= vecIndexLookup.Size )
		return pFallbackGlyph;
	const Wchar_t i = vecIndexLookup.Data[ c ];
	if ( i == ( Wchar_t )-1 )
		return pFallbackGlyph;
	return &vecGlyphs.Data[ i ];
}

const FontGlyph_t* Font_t::FindGlyphNoFallback( Wchar_t c ) const
{
	if ( c >= vecIndexLookup.Size )
		return NULL;
	const Wchar_t i = vecIndexLookup.Data[ c ];
	if ( i == ( Wchar_t )-1 )
		return NULL;
	return &vecGlyphs.Data[ i ];
}

const char* Font_t::CalcWordWrapPositionA( float flScale, const char* szText, const char* szTextEnd, float flWrapWidth ) const
{
	// Simple word-wrapping for English, not full-featured. Please submit failing cases!
	// FIXME: Much possible improvements (don't cut things like "word !", "word!!!" but cut within "word,,,,", more sensible support for punctuations, support for Unicode punctuations, etc.)

	// For references, possible wrap point marked with ^
	//  "aaa bbb, ccc,ddd. eee   fff. ggg!"
	//      ^    ^    ^   ^   ^__    ^    ^

	// List of hardcoded separators: .,;!?'"

	// Skip extra blanks after a line returns (that includes not counting them in width computation)
	// e.g. "Hello    world" --> "Hello" "World"

	// Cut words that cannot possibly fit within one line.
	// e.g.: "The tropical fish" with ~5 characters worth of width --> "The tr" "opical" "fish"

	float flLineWidth = 0.0f;
	float flWordWidth = 0.0f;
	float flBlankWidth = 0.0f;
	flWrapWidth /= flScale; // We work with unscaled widths to avoid scaling every characters

	const char* szWordEnd = szText;
	const char* prev_word_end = NULL;
	bool bInsideWord = true;

	const char* s = szText;
	while ( s < szTextEnd )
	{
		std::uint32_t c = ( std::uint32_t )*s;
		const char* szNextChar;
		if ( c < 0x80 )
			szNextChar = s + 1;
		else
			szNextChar = s + CRT::CharMultiByteToUTF32( szText, szTextEnd, &c );
		if ( c == 0 )
			break;

		if ( c < 32 )
		{
			if ( c == '\n' )
			{
				flLineWidth = flWordWidth = flBlankWidth = 0.0f;
				bInsideWord = true;
				s = szNextChar;
				continue;
			}
			if ( c == '\r' )
			{
				s = szNextChar;
				continue;
			}
		}

		const float flCharWidth = ( ( int )c < vecIndexAdvanceX.Size ? vecIndexAdvanceX.Data[ c ] : flFallbackAdvanceX );
		if ( CRT::IsBlank( ( uint8_t )c ) || c == 0x3000 )
		{
			if ( bInsideWord )
			{
				flLineWidth += flBlankWidth;
				flBlankWidth = 0.0f;
				szWordEnd = s;
			}
			flBlankWidth += flCharWidth;
			bInsideWord = false;
		}
		else
		{
			flWordWidth += flCharWidth;
			if ( bInsideWord )
			{
				szWordEnd = szNextChar;
			}
			else
			{
				prev_word_end = szWordEnd;
				flLineWidth += flWordWidth + flBlankWidth;
				flWordWidth = flBlankWidth = 0.0f;
			}

			// Allow wrapping after punctuation.
			bInsideWord = !( c == '.' || c == ',' || c == ';' || c == '!' || c == '?' || c == '\"' );
		}

		// We ignore blank width at the end of the line (they can be skipped)
		if ( flLineWidth + flWordWidth > flWrapWidth )
		{
			// Words that cannot possibly fit within an entire line will be cut anywhere.
			if ( flWordWidth < flWrapWidth )
				s = prev_word_end ? prev_word_end : szWordEnd;
			break;
		}

		s = szNextChar;
	}

	return s;
}

Vector2D_t Font_t::CalcTextSizeA( float size, float max_width, float flWrapWidth, const char* szTextBegin, const char* szTextEnd, const char** remaining ) const
{
	if ( !szTextEnd )
		szTextEnd = szTextBegin + CRT::StringLength( szTextBegin ); // FIXME-OPT: Need to avoid this.

	const float flLineHeight = size;
	const float flScale = size / flFontSize;

	Vector2D_t vecTextSize = Vector2D_t( 0, 0 );
	float flLineWidth = 0.0f;

	const bool bWordWrapEnabled = ( flWrapWidth > 0.0f );
	const char* szWordWrapEOL = NULL;

	const char* s = szTextBegin;
	while ( s < szTextEnd )
	{
		if ( bWordWrapEnabled )
		{
			// Calculate how far we can render. Requires two passes on the string pData but keeps the code simple and not intrusive for what's essentially an uncommon feature.
			if ( !szWordWrapEOL )
			{
				szWordWrapEOL = CalcWordWrapPositionA( flScale, s, szTextEnd, flWrapWidth - flLineWidth );
				if ( szWordWrapEOL == s ) // Wrap_width is too small to fit anything. Force displaying 1 character to minimize the height discontinuity.
					szWordWrapEOL++; // +1 may not be a character start point in UTF-8 but it's ok because we use s >= szWordWrapEOL below
			}

			if ( s >= szWordWrapEOL )
			{
				if ( vecTextSize.x < flLineWidth )
					vecTextSize.x = flLineWidth;
				vecTextSize.y += flLineHeight;
				flLineWidth = 0.0f;
				szWordWrapEOL = NULL;

				// Wrapping skips upcoming blanks
				while ( s < szTextEnd )
				{
					const char c = *s;
					if ( CRT::IsBlank( ( uint8_t )c ) )
					{
						s++;
					}
					else if ( c == '\n' )
					{
						s++;
						break;
					}
					else
					{
						break;
					}
				}
				continue;
			}
		}

		// Decode and advance source
		const char* szPrevChar = s;
		std::uint32_t c = ( std::uint32_t )*s;
		if ( c < 0x80 )
		{
			s += 1;
		}
		else
		{
			s += CRT::CharMultiByteToUTF32( s, szTextEnd, &c );
			if ( c == 0 ) // Malformed UTF-8?
				break;
		}

		if ( c < 32 )
		{
			if ( c == '\n' )
			{
				vecTextSize.x = CRT::Max( vecTextSize.x, flLineWidth );
				vecTextSize.y += flLineHeight;
				flLineWidth = 0.0f;
				continue;
			}
			if ( c == '\r' )
				continue;
		}

		const float flCharWidth = ( ( int )c < vecIndexAdvanceX.Size ? vecIndexAdvanceX.Data[ c ] : flFallbackAdvanceX ) * flScale;
		if ( flLineWidth + flCharWidth >= max_width )
		{
			s = szPrevChar;
			break;
		}

		flLineWidth += flCharWidth;
	}

	if ( vecTextSize.x < flLineWidth )
		vecTextSize.x = flLineWidth;

	if ( flLineWidth > 0 || vecTextSize.y == 0.0f )
		vecTextSize.y += flLineHeight;

	if ( remaining )
		*remaining = s;

	return vecTextSize;
}

void Font_t::RenderChar( DrawList_t* pDrawList, float size, Vector2D_t vecPostion, ColorPacked_t uCol, Wchar_t c ) const
{
	if ( c == ' ' || c == '\t' || c == '\n' || c == '\r' ) // Match behavior of RenderText(), those 4 codepoints are hard-coded.
		return;
	if ( const FontGlyph_t* pGlyph = FindGlyph( c ) )
	{
		float flScale = ( size >= 0.0f ) ? ( size / flFontSize ) : 1.0f;
		vecPostion.x = M_FLOOR( vecPostion.x + vecDisplayOffset.x );
		vecPostion.y = M_FLOOR( vecPostion.y + vecDisplayOffset.y );
		pDrawList->PrimReserve( 6, 4 );
		pDrawList->PrimRectUV( Vector2D_t( vecPostion.x + pGlyph->X0 * flScale, vecPostion.y + pGlyph->Y0 * flScale ), Vector2D_t( vecPostion.x + pGlyph->X1 * flScale, vecPostion.y + pGlyph->Y1 * flScale ), Vector2D_t( pGlyph->U0, pGlyph->V0 ), Vector2D_t( pGlyph->U1, pGlyph->V1 ), uCol );
	}
}

void Font_t::RenderText( DrawList_t* pDrawList, float size, Vector2D_t vecPostion, ColorPacked_t uCol, const Vector4D_t& vecClipRect, const char* szTextBegin, const char* szTextEnd, float flWrapWidth, bool bCPUFineClip ) const
{
	if ( !szTextEnd )
		szTextEnd = szTextBegin + CRT::StringLength( szTextBegin ); // D:: functions generally already provides a valid szTextEnd, so this is merely to handle direct calls.

	// Align to be pixel perfect
	vecPostion.x = M_FLOOR( vecPostion.x + vecDisplayOffset.x );
	vecPostion.y = M_FLOOR( vecPostion.y + vecDisplayOffset.y );
	float x = vecPostion.x;
	float y = vecPostion.y;
	if ( y > vecClipRect.w )
		return;

	const float flScale = size / flFontSize;
	const float flLineHeight = flFontSize * flScale;
	const bool bWordWrapEnabled = ( flWrapWidth > 0.0f );
	const char* szWordWrapEOL = NULL;

	// Fast-forward to first visible line
	const char* s = szTextBegin;
	if ( y + flLineHeight < vecClipRect.y && !bWordWrapEnabled )
		while ( y + flLineHeight < vecClipRect.y && s < szTextEnd )
		{
			s = ( const char* )CRT::MemoryChar( s, '\n', szTextEnd - s );
			s = s ? s + 1 : szTextEnd;
			y += flLineHeight;
		}

	// For large szText, scan for the last visible line in order to avoid over-reserving in the call to PrimReserve()
	// Note that very large horizontal line will still be affected by the issue (e.g. a one megabyte string buffer without a newline will likely crash atm)
	if ( szTextEnd - s > 10000 && !bWordWrapEnabled )
	{
		const char* s_end = s;
		float y_end = y;
		while ( y_end < vecClipRect.w && s_end < szTextEnd )
		{
			s_end = ( const char* )CRT::MemoryChar( s_end, '\n', szTextEnd - s_end );
			s_end = s_end ? s_end + 1 : szTextEnd;
			y_end += flLineHeight;
		}
		szTextEnd = s_end;
	}
	if ( s == szTextEnd )
		return;

	// Reserve vertices for remaining worse case (over-reserving is useful and easily amortized)
	const int vtx_count_max = ( int )( szTextEnd - s ) * 4;
	const int idx_count_max = ( int )( szTextEnd - s ) * 6;
	const int idx_expected_size = pDrawList->vecIdxBuffer.Size + idx_count_max;
	pDrawList->PrimReserve( idx_count_max, vtx_count_max );

	DrawVert_t* vtx_write = pDrawList->_pVtxWritePtr;
	DrawIdx_t* idx_write = pDrawList->_pIdxWritePtr;
	std::uint32_t vtx_current_idx = pDrawList->_nVtxCurrentIdx;

	while ( s < szTextEnd )
	{
		if ( bWordWrapEnabled )
		{
			// Calculate how far we can render. Requires two passes on the string pData but keeps the code simple and not intrusive for what's essentially an uncommon feature.
			if ( !szWordWrapEOL )
			{
				szWordWrapEOL = CalcWordWrapPositionA( flScale, s, szTextEnd, flWrapWidth - ( x - vecPostion.x ) );
				if ( szWordWrapEOL == s ) // Wrap_width is too small to fit anything. Force displaying 1 character to minimize the height discontinuity.
					szWordWrapEOL++; // +1 may not be a character start point in UTF-8 but it's ok because we use s >= szWordWrapEOL below
			}

			if ( s >= szWordWrapEOL )
			{
				x = vecPostion.x;
				y += flLineHeight;
				szWordWrapEOL = NULL;

				// Wrapping skips upcoming blanks
				while ( s < szTextEnd )
				{
					const char c = *s;
					if ( CRT::IsBlank( ( uint16_t )c ) )
					{
						s++;
					}
					else if ( c == '\n' )
					{
						s++;
						break;
					}
					else
					{
						break;
					}
				}
				continue;
			}
		}

		// Decode and advance source
		std::uint32_t c = ( std::uint32_t )*s;
		if ( c < 0x80 )
		{
			s += 1;
		}
		else
		{
			s += CRT::CharMultiByteToUTF32( s, szTextEnd, &c );
			if ( c == 0 ) // Malformed UTF-8?
				break;
		}

		if ( c < 32 )
		{
			if ( c == '\n' )
			{
				x = vecPostion.x;
				y += flLineHeight;
				if ( y > vecClipRect.w )
					break; // break out of main loop
				continue;
			}
			if ( c == '\r' )
				continue;
		}

		float flCharWidth = 0.0f;
		if ( const FontGlyph_t* glyph = FindGlyph( ( Wchar_t )c ) )
		{
			flCharWidth = glyph->AdvanceX * flScale;

			// Arbitrarily assume that both space and tabs are empty glyphs as an optimization
			if ( c != ' ' && c != '\t' )
			{
				// We don't do a second finer clipping test on the Y axis as we've already skipped anything before vecClipRect.y and exit once we pass vecClipRect.w
				float x1 = x + glyph->X0 * flScale;
				float x2 = x + glyph->X1 * flScale;
				float y1 = y + glyph->Y0 * flScale;
				float y2 = y + glyph->Y1 * flScale;
				if ( x1 <= vecClipRect.z && x2 >= vecClipRect.x )
				{
					// Render a character
					float u1 = glyph->U0;
					float v1 = glyph->V0;
					float u2 = glyph->U1;
					float v2 = glyph->V1;

					// CPU side clipping used to fit szText in their frame when the frame is too small. Only does clipping for axis aligned quads.
					if ( bCPUFineClip )
					{
						if ( x1 < vecClipRect.x )
						{
							u1 = u1 + ( 1.0f - ( x2 - vecClipRect.x ) / ( x2 - x1 ) ) * ( u2 - u1 );
							x1 = vecClipRect.x;
						}
						if ( y1 < vecClipRect.y )
						{
							v1 = v1 + ( 1.0f - ( y2 - vecClipRect.y ) / ( y2 - y1 ) ) * ( v2 - v1 );
							y1 = vecClipRect.y;
						}
						if ( x2 > vecClipRect.z )
						{
							u2 = u1 + ( ( vecClipRect.z - x1 ) / ( x2 - x1 ) ) * ( u2 - u1 );
							x2 = vecClipRect.z;
						}
						if ( y2 > vecClipRect.w )
						{
							v2 = v1 + ( ( vecClipRect.w - y1 ) / ( y2 - y1 ) ) * ( v2 - v1 );
							y2 = vecClipRect.w;
						}
						if ( y1 >= y2 )
						{
							x += flCharWidth;
							continue;
						}
					}

					// We are NOT calling PrimRectUV() here because non-inlined causes too much overhead in a debug builds. Inlined here:
					{
						idx_write[ 0 ] = ( DrawIdx_t )( vtx_current_idx );
						idx_write[ 1 ] = ( DrawIdx_t )( vtx_current_idx + 1 );
						idx_write[ 2 ] = ( DrawIdx_t )( vtx_current_idx + 2 );
						idx_write[ 3 ] = ( DrawIdx_t )( vtx_current_idx );
						idx_write[ 4 ] = ( DrawIdx_t )( vtx_current_idx + 2 );
						idx_write[ 5 ] = ( DrawIdx_t )( vtx_current_idx + 3 );
						vtx_write[ 0 ].vecPostion.x = x1;
						vtx_write[ 0 ].vecPostion.y = y1;
						vtx_write[ 0 ].uCol = uCol;
						vtx_write[ 0 ].vecUV.x = u1;
						vtx_write[ 0 ].vecUV.y = v1;
						vtx_write[ 1 ].vecPostion.x = x2;
						vtx_write[ 1 ].vecPostion.y = y1;
						vtx_write[ 1 ].uCol = uCol;
						vtx_write[ 1 ].vecUV.x = u2;
						vtx_write[ 1 ].vecUV.y = v1;
						vtx_write[ 2 ].vecPostion.x = x2;
						vtx_write[ 2 ].vecPostion.y = y2;
						vtx_write[ 2 ].uCol = uCol;
						vtx_write[ 2 ].vecUV.x = u2;
						vtx_write[ 2 ].vecUV.y = v2;
						vtx_write[ 3 ].vecPostion.x = x1;
						vtx_write[ 3 ].vecPostion.y = y2;
						vtx_write[ 3 ].uCol = uCol;
						vtx_write[ 3 ].vecUV.x = u1;
						vtx_write[ 3 ].vecUV.y = v2;
						vtx_write += 4;
						vtx_current_idx += 4;
						idx_write += 6;
					}
				}
			}
		}

		x += flCharWidth;
	}

	// Give back unused vertices (clipped ones, blanks) ~ this is essentially a PrimUnreserve() action.
	pDrawList->vecVtxBuffer.Size = ( int )( vtx_write - pDrawList->vecVtxBuffer.Data ); // Same as calling shrink()
	pDrawList->vecIdxBuffer.Size = ( int )( idx_write - pDrawList->vecIdxBuffer.Data );
	pDrawList->vecCmdBuffer[ pDrawList->vecCmdBuffer.Size - 1 ].nElemCount -= ( idx_expected_size - pDrawList->vecIdxBuffer.Size );
	pDrawList->_pVtxWritePtr = vtx_write;
	pDrawList->_pIdxWritePtr = idx_write;
	pDrawList->_nVtxCurrentIdx = vtx_current_idx;
}

//-----------------------------------------------------------------------------
// [SECTION] Decompression code
//-----------------------------------------------------------------------------
// Compressed with stb_compress() then converted to a C array and encoded as base85.
// Use the program in misc/fonts/binary_to_compressed_c.cpp to create the array from a TTF file.
// The purpose of encoding as base85 instead of "0x00,0x01,..." style is only save on _source code_ size.
// Decompression from stb.h (public domain) by Sean Barrett https://github.com/nothings/stb/blob/master/stb.h
//-----------------------------------------------------------------------------

static std::uint32_t stb_decompress_length( const std::uint8_t* input )
{
	return ( input[ 8 ] << 24 ) + ( input[ 9 ] << 16 ) + ( input[ 10 ] << 8 ) + input[ 11 ];
}

static std::uint8_t* stb__barrier_out_e, * stb__barrier_out_b;
static const std::uint8_t* stb__barrier_in_b;
static std::uint8_t* stb__dout;

static void stb__match( const std::uint8_t* pData, std::uint32_t length )
{
	// INVERSE of memmove... write each byte before copying the next...
	CRT_ASSERTION( stb__dout + length <= stb__barrier_out_e );
	if ( stb__dout + length > stb__barrier_out_e )
	{
		stb__dout += length;
		return;
	}
	if ( pData < stb__barrier_out_b )
	{
		stb__dout = stb__barrier_out_e + 1;
		return;
	}
	while ( length-- )
		*stb__dout++ = *pData++;
}

static void stb__lit( const std::uint8_t* pData, std::uint32_t length )
{
	CRT_ASSERTION( stb__dout + length <= stb__barrier_out_e );
	if ( stb__dout + length > stb__barrier_out_e )
	{
		stb__dout += length;
		return;
	}
	if ( pData < stb__barrier_in_b )
	{
		stb__dout = stb__barrier_out_e + 1;
		return;
	}
	CRT::MemoryCopy( stb__dout, pData, length );
	stb__dout += length;
}

#define stb__in2(x) ((i[x] << 8) + i[(x) + 1])
#define stb__in3(x) ((i[x] << 16) + stb__in2((x) + 1))
#define stb__in4(x) ((i[x] << 24) + stb__in3((x) + 1))

static const std::uint8_t* stb_decompress_token( const std::uint8_t* i )
{
	if ( *i >= 0x20 )
	{ // use fewer if's for cases that expand small
		if ( *i >= 0x80 )
			stb__match( stb__dout - i[ 1 ] - 1, i[ 0 ] - 0x80 + 1 ), i += 2;
		else if ( *i >= 0x40 )
			stb__match( stb__dout - ( stb__in2( 0 ) - 0x4000 + 1 ), i[ 2 ] + 1 ), i += 3;
		else /* *i >= 0x20 */
			stb__lit( i + 1, i[ 0 ] - 0x20 + 1 ), i += 1 + ( i[ 0 ] - 0x20 + 1 );
	}
	else
	{ // more ifs for cases that expand large, since overhead is amortized
		if ( *i >= 0x18 )
			stb__match( stb__dout - ( stb__in3( 0 ) - 0x180000 + 1 ), i[ 3 ] + 1 ), i += 4;
		else if ( *i >= 0x10 )
			stb__match( stb__dout - ( stb__in3( 0 ) - 0x100000 + 1 ), stb__in2( 3 ) + 1 ), i += 5;
		else if ( *i >= 0x08 )
			stb__lit( i + 2, stb__in2( 0 ) - 0x0800 + 1 ), i += 2 + ( stb__in2( 0 ) - 0x0800 + 1 );
		else if ( *i == 0x07 )
			stb__lit( i + 3, stb__in2( 1 ) + 1 ), i += 3 + ( stb__in2( 1 ) + 1 );
		else if ( *i == 0x06 )
			stb__match( stb__dout - ( stb__in3( 1 ) + 1 ), i[ 4 ] + 1 ), i += 5;
		else if ( *i == 0x04 )
			stb__match( stb__dout - ( stb__in3( 1 ) + 1 ), stb__in2( 4 ) + 1 ), i += 6;
	}
	return i;
}

static std::uint32_t stb_adler32( std::uint32_t adler32, std::uint8_t* buffer, std::uint32_t buflen )
{
	const unsigned long ADLER_MOD = 65521;
	unsigned long s1 = adler32 & 0xffff, s2 = adler32 >> 16;
	unsigned long blocklen = buflen % 5552;

	unsigned long i;
	while ( buflen )
	{
		for ( i = 0; i + 7 < blocklen; i += 8 )
		{
			s1 += buffer[ 0 ], s2 += s1;
			s1 += buffer[ 1 ], s2 += s1;
			s1 += buffer[ 2 ], s2 += s1;
			s1 += buffer[ 3 ], s2 += s1;
			s1 += buffer[ 4 ], s2 += s1;
			s1 += buffer[ 5 ], s2 += s1;
			s1 += buffer[ 6 ], s2 += s1;
			s1 += buffer[ 7 ], s2 += s1;

			buffer += 8;
		}

		for ( ; i < blocklen; ++i )
			s1 += *buffer++, s2 += s1;

		s1 %= ADLER_MOD, s2 %= ADLER_MOD;
		buflen -= blocklen;
		blocklen = 5552;
	}
	return ( std::uint32_t )( s2 << 16 ) + ( std::uint32_t )s1;
}

static std::uint32_t stb_decompress( std::uint8_t* output, const std::uint8_t* i, std::uint32_t /*length*/ )
{
	if ( stb__in4( 0 ) != 0x57bC0000 )
		return 0;
	if ( stb__in4( 4 ) != 0 )
		return 0; // error! stream is > 4GB
	const std::uint32_t olen = stb_decompress_length( i );
	stb__barrier_in_b = i;
	stb__barrier_out_e = output + olen;
	stb__barrier_out_b = output;
	i += 16;

	stb__dout = output;
	for ( ;;)
	{
		const std::uint8_t* old_i = i;
		i = stb_decompress_token( i );
		if ( i == old_i )
		{
			if ( *i == 0x05 && i[ 1 ] == 0xfa )
			{
				CRT_ASSERTION( stb__dout == output + olen );
				if ( stb__dout != output + olen )
					return 0;
				if ( stb_adler32( 1, output, olen ) != ( std::uint32_t )stb__in4( 2 ) )
					return 0;
				return olen;
			}
			else
			{
				CRT_ASSERTION( 0 ); /* NOTREACHED */
				return 0;
			}
		}
		CRT_ASSERTION( stb__dout <= output + olen );
		if ( stb__dout > output + olen )
			return 0;
	}
}

#pragma endregion

#pragma once
// used: [stl] is_integral_v
#include <type_traits>
// used: math
#include <cmath>

// convert angle in degrees to radians
#define M_DEG2RAD(DEGREES) ((DEGREES) * (M::_PI / 180.f))
// convert angle in radians to degrees
#define M_RAD2DEG(RADIANS) ((RADIANS) * (180.f / M::_PI))
/// linearly interpolate the value between @a'X0' and @a'X1' by @a'FACTOR'
#define M_LERP(X0, X1, FACTOR) ((X0) + ((X1) - (X0)) * (FACTOR))

#define M_FABS(X) fabsf(X)
#define M_SQRT(X) sqrtf(X)
#define M_FMOD(X, Y) fmodf((X), (Y))
#define M_COS(X) cosf(X)
#define M_SIN(X) sinf(X)
#define M_ACOS(X) acosf(X)
#define M_ATAN2(Y, X) atan2f((Y), (X))
#define M_ATOF(STR) atof(STR)
#define M_FLOOR(X) floorf(X)
#define M_CEIL(X) ceilf(X)
#define M_ROUND(_VAL) ((float)(int)((_VAL) + 0.5f))

static inline float  M_POW( float x, float y ) { return powf( x, y ); }
static inline double M_POW( double x, double y ) { return pow( x, y ); }

/*
 * MATHEMATICS
 * - basic trigonometry, algebraic mathematical functions and constants
 */
namespace M
{
	/* @section: constants */
	// pi value
	inline constexpr float _PI = 3.141592654f;
	// double of pi
	inline constexpr float _2PI = _PI * 2.0f;
	// half of pi
	inline constexpr float _HPI = _PI / 2.0f;
	// quarter of pi
	inline constexpr float _QPI = _HPI / 2.0f;
	// reciprocal of double of pi
	inline constexpr float _1DIV2PI = 0.159154943f;
	// golden ratio
	inline constexpr float _PHI = 1.618033988f;

	/* @section: exponential */
	/// @returns: true if given number is power of two, false otherwise
	template <typename T> requires ( std::is_integral_v<T> )
		[[nodiscard]] __forceinline constexpr bool IsPowerOfTwo( const T value ) noexcept
	{
		return value != 0 && ( value & ( value - 1 ) ) == 0;
	}

	template <typename T> requires ( std::is_integral_v<T> )
		[[nodiscard]] __forceinline constexpr int UpperPowerOfTwo( T v ) noexcept
	{
		v--;
		v |= v >> 1;
		v |= v >> 2;
		v |= v >> 4;
		v |= v >> 8;
		v |= v >> 16;
		v++;
		return v;
	}

}
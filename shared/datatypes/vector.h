#pragma once

// used: [stl] numeric_limits
#include <limits>
// used: [crt] isfinite, fmodf, sqrtf
#include <cmath>

#include "../utilities/crt.h"
#include "../utilities/math.h"

struct Vector2D_t
{
	constexpr Vector2D_t( const float x = 0.0f, const float y = 0.0f ) :
		x( x ), y( y )
	{ }

	constexpr Vector2D_t( const float* arrVector ) :
		x( arrVector[ 0 ] ), y( arrVector[ 1 ] )
	{ }

#pragma region vector_array_operators

	[[nodiscard]] float& operator[]( const int nIndex )
	{
		return reinterpret_cast< float* >( this )[ nIndex ];
	}

	[[nodiscard]] const float& operator[]( const int nIndex ) const
	{
		return reinterpret_cast< const float* >( this )[ nIndex ];
	}

#pragma endregion

#pragma region vector_relational_operators

	bool operator==( const Vector2D_t& vecBase ) const
	{
		return this->IsEqual( vecBase );
	}

	bool operator!=( const Vector2D_t& vecBase ) const
	{
		return !this->IsEqual( vecBase );
	}

#pragma endregion

#pragma region vector_assignment_operators

	constexpr Vector2D_t& operator=( const Vector2D_t& vecBase )
	{
		this->x = vecBase.x;
		this->y = vecBase.y;
		return *this;
	}

#pragma endregion

#pragma region vector_arithmetic_assignment_operators

	constexpr Vector2D_t& operator+=( const Vector2D_t& vecBase )
	{
		this->x += vecBase.x;
		this->y += vecBase.y;
		return *this;
	}

	constexpr Vector2D_t& operator-=( const Vector2D_t& vecBase )
	{
		this->x -= vecBase.x;
		this->y -= vecBase.y;
		return *this;
	}

	constexpr Vector2D_t& operator*=( const Vector2D_t& vecBase )
	{
		this->x *= vecBase.x;
		this->y *= vecBase.y;
		return *this;
	}

	constexpr Vector2D_t& operator/=( const Vector2D_t& vecBase )
	{
		this->x /= vecBase.x;
		this->y /= vecBase.y;
		return *this;
	}

	constexpr Vector2D_t& operator+=( const float flAdd )
	{
		this->x += flAdd;
		this->y += flAdd;
		return *this;
	}

	constexpr Vector2D_t& operator-=( const float flSubtract )
	{
		this->x -= flSubtract;
		this->y -= flSubtract;
		return *this;
	}

	constexpr Vector2D_t& operator*=( const float flMultiply )
	{
		this->x *= flMultiply;
		this->y *= flMultiply;
		return *this;
	}

	constexpr Vector2D_t& operator/=( const float flDivide )
	{
		this->x /= flDivide;
		this->y /= flDivide;
		return *this;
	}

#pragma endregion

#pragma region vector_arithmetic_unary_operators

	constexpr Vector2D_t& operator-( )
	{
		this->x = -this->x;
		this->y = -this->y;
		return *this;
	}

	constexpr Vector2D_t operator-( ) const
	{
		return { -this->x, -this->y };
	}

#pragma endregion

#pragma region vector_arithmetic_ternary_operators

	Vector2D_t operator+( const Vector2D_t& vecAdd ) const
	{
		return { this->x + vecAdd.x, this->y + vecAdd.y };
	}

	Vector2D_t operator-( const Vector2D_t& vecSubtract ) const
	{
		return { this->x - vecSubtract.x, this->y - vecSubtract.y };
	}

	Vector2D_t operator*( const Vector2D_t& vecMultiply ) const
	{
		return { this->x * vecMultiply.x, this->y * vecMultiply.y };
	}

	Vector2D_t operator/( const Vector2D_t& vecDivide ) const
	{
		return { this->x / vecDivide.x, this->y / vecDivide.y };
	}

	Vector2D_t operator+( const float flAdd ) const
	{
		return { this->x + flAdd, this->y + flAdd };
	}

	Vector2D_t operator-( const float flSubtract ) const
	{
		return { this->x - flSubtract, this->y - flSubtract };
	}

	Vector2D_t operator*( const float flMultiply ) const
	{
		return { this->x * flMultiply, this->y * flMultiply };
	}

	Vector2D_t operator/( const float flDivide ) const
	{
		return { this->x / flDivide, this->y / flDivide };
	}

#pragma endregion

	/// @returns: true if each component of the vector equals to another, false otherwise
	[[nodiscard]] bool IsEqual( const Vector2D_t& vecEqual, const float flErrorMargin = std::numeric_limits<float>::epsilon( ) ) const
	{
		return ( std::fabsf( this->x - vecEqual.x ) < flErrorMargin && std::fabsf( this->y - vecEqual.y ) < flErrorMargin );
	}

	float x = 0.0f, y = 0.0f;
};

struct Vector4D_t
{
	constexpr Vector4D_t( const float x = 0.0f, const float y = 0.0f, const float z = 0.0f, const float w = 0.0f ) :
		x( x ), y( y ), z( z ), w( w )
	{ }

	constexpr Vector4D_t( const Vector2D_t& vecPosition, const Vector2D_t& vecSize ) :
		x( vecPosition.x ), y( vecPosition.y ), z( vecSize.x ), w( vecSize.y )
	{ }

	constexpr Vector4D_t( const float* arrVector ) :
		x( arrVector[ 0 ] ), y( arrVector[ 1 ] ), z( arrVector[ 2 ] ), w( arrVector[ 3 ] )
	{ }

#pragma region vector_array_operators

	[[nodiscard]] float& operator[]( const int nIndex )
	{
		return reinterpret_cast< float* >( this )[ nIndex ];
	}

	[[nodiscard]] const float& operator[]( const int nIndex ) const
	{
		return reinterpret_cast< const float* >( this )[ nIndex ];
	}

#pragma endregion

#pragma region vector_relational_operators

	bool operator==( const Vector4D_t& vecBase ) const
	{
		return this->IsEqual( vecBase );
	}

	bool operator!=( const Vector4D_t& vecBase ) const
	{
		return !this->IsEqual( vecBase );
	}

#pragma endregion

#pragma region vector_assignment_operators

	constexpr Vector4D_t& operator=( const Vector4D_t& vecBase )
	{
		this->x = vecBase.x;
		this->y = vecBase.y;
		this->z = vecBase.z;
		this->w = vecBase.w;
		return *this;
	}

	constexpr Vector4D_t& operator=( const Vector2D_t& vecBase2D )
	{
		this->x = vecBase2D.x;
		this->y = vecBase2D.y;
		this->z = 0.0f;
		this->w = 0.0f;
		return *this;
	}

#pragma endregion

#pragma region vector_arithmetic_assignment_operators

	constexpr Vector4D_t& operator+=( const Vector4D_t& vecBase )
	{
		this->x += vecBase.x;
		this->y += vecBase.y;
		this->z += vecBase.z;
		this->w += vecBase.w;
		return *this;
	}

	constexpr Vector4D_t& operator-=( const Vector4D_t& vecBase )
	{
		this->x -= vecBase.x;
		this->y -= vecBase.y;
		this->z -= vecBase.z;
		this->w -= vecBase.w;
		return *this;
	}

	constexpr Vector4D_t& operator*=( const Vector4D_t& vecBase )
	{
		this->x *= vecBase.x;
		this->y *= vecBase.y;
		this->z *= vecBase.z;
		this->w *= vecBase.w;
		return *this;
	}

	constexpr Vector4D_t& operator/=( const Vector4D_t& vecBase )
	{
		this->x /= vecBase.x;
		this->y /= vecBase.y;
		this->z /= vecBase.z;
		this->w /= vecBase.w;
		return *this;
	}

	constexpr Vector4D_t& operator+=( const float flAdd )
	{
		this->x += flAdd;
		this->y += flAdd;
		this->z += flAdd;
		this->w += flAdd;
		return *this;
	}

	constexpr Vector4D_t& operator-=( const float flSubtract )
	{
		this->x -= flSubtract;
		this->y -= flSubtract;
		this->z -= flSubtract;
		this->w -= flSubtract;
		return *this;
	}

	constexpr Vector4D_t& operator*=( const float flMultiply )
	{
		this->x *= flMultiply;
		this->y *= flMultiply;
		this->z *= flMultiply;
		this->w *= flMultiply;
		return *this;
	}

	constexpr Vector4D_t& operator/=( const float flDivide )
	{
		this->x /= flDivide;
		this->y /= flDivide;
		this->z /= flDivide;
		this->w /= flDivide;
		return *this;
	}

#pragma endregion

#pragma region vector_arithmetic_unary_operators

	constexpr Vector4D_t& operator-( )
	{
		this->x = -this->x;
		this->y = -this->y;
		this->z = -this->z;
		this->w = -this->w;
		return *this;
	}

	constexpr Vector4D_t operator-( ) const
	{
		return { -this->x, -this->y, -this->z, -this->w };
	}

#pragma endregion

#pragma region vector_arithmetic_ternary_operators

	Vector4D_t operator+( const Vector4D_t& vecAdd ) const
	{
		return { this->x + vecAdd.x, this->y + vecAdd.y, this->z + vecAdd.z, this->w + vecAdd.w };
	}

	Vector4D_t operator-( const Vector4D_t& vecSubtract ) const
	{
		return { this->x - vecSubtract.x, this->y - vecSubtract.y, this->z - vecSubtract.z, this->w - vecSubtract.w };
	}

	Vector4D_t operator*( const Vector4D_t& vecMultiply ) const
	{
		return { this->x * vecMultiply.x, this->y * vecMultiply.y, this->z * vecMultiply.z, this->w * vecMultiply.w };
	}

	Vector4D_t operator/( const Vector4D_t& vecDivide ) const
	{
		return { this->x / vecDivide.x, this->y / vecDivide.y, this->z / vecDivide.z, this->w / vecDivide.w };
	}

	Vector4D_t operator+( const float flAdd ) const
	{
		return { this->x + flAdd, this->y + flAdd, this->z + flAdd, this->w + flAdd };
	}

	Vector4D_t operator-( const float flSubtract ) const
	{
		return { this->x - flSubtract, this->y - flSubtract, this->z - flSubtract, this->w - flSubtract };
	}

	Vector4D_t operator*( const float flMultiply ) const
	{
		return { this->x * flMultiply, this->y * flMultiply, this->z * flMultiply, this->w * flMultiply };
	}

	Vector4D_t operator/( const float flDivide ) const
	{
		return { this->x / flDivide, this->y / flDivide, this->z / flDivide, this->w / flDivide };
	}

#pragma endregion

	/// @returns: true if each component of the vector equals to another, false otherwise
	[[nodiscard]] bool IsEqual( const Vector4D_t& vecEqual, const float flErrorMargin = std::numeric_limits<float>::epsilon( ) ) const
	{
		return ( std::fabsf( this->x - vecEqual.x ) < flErrorMargin && std::fabsf( this->y - vecEqual.y ) < flErrorMargin && std::fabsf( this->z - vecEqual.z ) < flErrorMargin && std::fabsf( this->w - vecEqual.w ) < flErrorMargin );
	}

	float x = 0.0f, y = 0.0f, z = 0.0f, w = 0.0f;
};

#pragma region vector_math_functions

static inline Vector2D_t VectorMin( const Vector2D_t& lhs, const Vector2D_t& rhs ) { return Vector2D_t( lhs.x < rhs.x ? lhs.x : rhs.x, lhs.y < rhs.y ? lhs.y : rhs.y ); }
static inline Vector2D_t VectorMax( const Vector2D_t& lhs, const Vector2D_t& rhs ) { return Vector2D_t( lhs.x >= rhs.x ? lhs.x : rhs.x, lhs.y >= rhs.y ? lhs.y : rhs.y ); }
static inline Vector2D_t VectorClamp( const Vector2D_t& v, const Vector2D_t& mn, Vector2D_t mx ) { return Vector2D_t( ( v.x < mn.x ) ? mn.x : ( v.x > mx.x ) ? mx.x : v.x, ( v.y < mn.y ) ? mn.y : ( v.y > mx.y ) ? mx.y : v.y ); }
static inline Vector2D_t VectorLerp( const Vector2D_t& a, const Vector2D_t& b, float t ) { return Vector2D_t( a.x + ( b.x - a.x ) * t, a.y + ( b.y - a.y ) * t ); }
static inline Vector2D_t VectorLerp( const Vector2D_t& a, const Vector2D_t& b, const Vector2D_t& t ) { return Vector2D_t( a.x + ( b.x - a.x ) * t.x, a.y + ( b.y - a.y ) * t.y ); }
static inline Vector4D_t VectorLerp( const Vector4D_t& a, const Vector4D_t& b, float t ) { return Vector4D_t( a.x + ( b.x - a.x ) * t, a.y + ( b.y - a.y ) * t, a.z + ( b.z - a.z ) * t, a.w + ( b.w - a.w ) * t ); }
static inline float   VectorLengthSqr( const Vector2D_t& lhs ) { return lhs.x * lhs.x + lhs.y * lhs.y; }
static inline float   VectorLengthSqr( const Vector4D_t& lhs ) { return lhs.x * lhs.x + lhs.y * lhs.y + lhs.z * lhs.z + lhs.w * lhs.w; }
static inline float   VectorInvLength( const Vector2D_t& lhs, float fail_value ) { float d = lhs.x * lhs.x + lhs.y * lhs.y; if ( d > 0.0f ) return 1.0f / M_SQRT( d ); return fail_value; }
static inline float   VectorLength( const Vector2D_t& lhs, float fail_value ) { float d = ( lhs.x * lhs.x ) + ( lhs.y * lhs.y ); if ( d > 0.0f ) return M_SQRT( d ); return fail_value; }
static inline Vector2D_t VectorFloor( const Vector2D_t& v ) { return Vector2D_t( ( float )( int )( v.x ), ( float )( int )( v.y ) ); }
static inline float  VectorDot( const Vector2D_t& a, const Vector2D_t& b ) { return a.x * b.x + a.y * b.y; }
static inline Vector2D_t VectorRotate( const Vector2D_t& v, float cos_a, float sin_a ) { return Vector2D_t( v.x * cos_a - v.y * sin_a, v.x * sin_a + v.y * cos_a ); }
static inline Vector2D_t VectorMul( const Vector2D_t& lhs, const Vector2D_t& rhs ) { return Vector2D_t( lhs.x * rhs.x, lhs.y * rhs.y ); }

#pragma endregion
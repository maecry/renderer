#pragma once

// used: printf
#include <cstdio>
// used: [stl] source_location
#include <source_location>	

__forceinline void assert_failed( const char* szExpression, std::source_location l = std::source_location::current( ) )
{
	printf( "assertion failed: %s\nFile: %s\nLine: %d\nFunction: %s\n", szExpression, l.file_name( ), l.line( ), l.function_name( ) );
#ifdef _DEBUG
#if defined(_MSC_VER)
	__debugbreak( );
#elif defined(__GNUC__)
	__builtin_trap( );
#elif defined(__clang__)
	__builtin_debugtrap( );
#endif
#else
	std::abort( );
#endif
}

#define _ASSERT( _Expression ) static_cast<void>( (!!(_Expression)) || (assert_failed( #_Expression ), 0) )
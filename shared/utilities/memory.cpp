#include "memory.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>

// used: memoryset
#include "crt.h"

#pragma region memory_allocation

struct DebugAllocInfo_t
{
	int iTotalAllocCount = 0;
	int iTotalFreeCount = 0;

	std::size_t nTotalAllocSize = 0;

	DebugAllocInfo_t( )
	{
		CRT::MemorySet( this, 0, sizeof( *this ) );
	}
} g_DebugAllocInfo;

static void DebugAlloc( void* pMemory, const size_t nSize )
{
	CRT_UNUNSED( pMemory );
	if ( nSize != static_cast< std::size_t >( -1 ) )
	{
		++g_DebugAllocInfo.iTotalAllocCount;
		g_DebugAllocInfo.nTotalAllocSize += nSize;
	}
	else
		++g_DebugAllocInfo.iTotalFreeCount;

}

void* MEM::HeapAlloc( const std::size_t nSize )
{
	const HANDLE hHeap = ::GetProcessHeap( );

#ifdef _DEBUG
	DebugAlloc( nullptr, nSize );
#endif

	return ::HeapAlloc( hHeap, 0UL, nSize );
}

void MEM::HeapFree( void* pMemory )
{
#ifdef _DEBUG
	DebugAlloc( pMemory, static_cast< std::size_t >( -1 ) );
#endif
	if ( pMemory != nullptr )
	{
		const HANDLE hHeap = ::GetProcessHeap( );
		::HeapFree( hHeap, 0UL, pMemory );
	}
}

void* MEM::HeapRealloc( void* pMemory, const std::size_t nNewSize )
{
	if ( pMemory == nullptr )
		return HeapAlloc( nNewSize );

	if ( nNewSize == 0UL )
	{
		HeapFree( pMemory );
		return nullptr;
	}

	const HANDLE hHeap = ::GetProcessHeap( );
	return ::HeapReAlloc( hHeap, 0UL, pMemory, nNewSize );
}

#pragma endregion
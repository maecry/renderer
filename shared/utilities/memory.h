#pragma once

// used: _alloca
#include <malloc.h>
// used: [stl] type_info
#include <typeinfo>

#pragma region memory_definitions
#pragma warning(push)
#pragma warning(disable : 6255) // '_alloca' indicates failure by raising a stack overflow exception. consider using '_malloca' instead
#define MEM_STACKALLOC(_TYPE, _SIZE) static_cast<_TYPE>(_alloca(_SIZE))
#pragma warning(pop)
#pragma endregion


/*
 * MEMORY
 * - memory management and manipulation
 */
namespace MEM
{
	/* @section: allocation */
	// allocate a block of memory from a heap
	[[nodiscard]] void* HeapAlloc( const std::size_t nSize );
	// free a memory block allocated from a heap
	void HeapFree( void* pMemory );
	// reallocate a block of memory from a heap
	// @note: we're expect this to allocate instead when passed null, and free if size is null
	void* HeapRealloc( void* pMemory, const std::size_t nNewSize );
}

#pragma region memory_overload_operators
// clang-format off

struct MemNewDummy_t { };
inline void* operator new( size_t, MemNewDummy_t, void* ptr ) { return ptr; }
inline void  operator delete( void*, MemNewDummy_t, void* ) { } // This is only required so we can use the symmetrical new()
#define MEM_HEAPALLOC(_TYPE, _SIZE)               static_cast<_TYPE>(MEM::HeapAlloc(_SIZE))
#define MEM_HEAPFREE(_PTR)                       MEM::HeapFree(_PTR)
#define MEM_PLACEMENT_NEW(_PTR)              new(MemNewDummy_t(), _PTR)
#define MEM_NEW(_TYPE)                       new(MemNewDummy_t(), MEM::HeapAlloc(sizeof(_TYPE))) _TYPE
template<typename T> void MEM_DELETE( T* p ) { if ( p != nullptr ) { p->~T( ); MEM::HeapFree( p ); } }

// clang-format on
#pragma endregion

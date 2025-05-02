#pragma once
// used: [stl] uint32_t
#include <cstdint>

using CRC32_t = std::uint32_t;

namespace CRC
{
	CRC32_t HashData( const void* pData, size_t uSize = 0U, CRC32_t uSeed = 0U ) noexcept;
	CRC32_t HashString( const char* szString, size_t uSize = 0U, CRC32_t uSeed = 0U ) noexcept;
}
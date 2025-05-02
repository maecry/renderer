#pragma once

// used: [win]
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>

// used: [stl] std::uint64_t
#include <cstdint>

// helpers functions
namespace FILEHELPERS
{
	/* @section: get */
	/// load file to memory
	/// @param[in] szFileName - path to file
	/// @param[out] pnOutFileSize - size of loaded file
	/// @param[in] iPaddingBytes - padding bytes to add to the end of the file
	/// @return pointer to allocated memory with file data
	void* LoadFileToMemory( const char* szFileName, size_t* pnOutFileSize, int iPaddingBytes );

	/// @param[out] wszDestination output for working path where files will be saved (default: "%userprofile%\documents\.crown")
	/// @returns: true if successfully got the path, false otherwise
	bool GetWorkingPath( wchar_t* wszDestination );

	/// @param[in] wszPath - path to create
	/// @returns: true if successfully created the path, false otherwise
	bool CreatePath( const wchar_t* wszPath );
}

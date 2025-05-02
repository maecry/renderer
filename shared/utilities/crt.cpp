#include "crt.h"

// used: [ext] stb_sprintf
#define STB_SPRINTF_IMPLEMENTATION
#include "../dependencies/stb/stb_sprintf.h"

/// write formatted data to a string, alternative of 'sprintf'
/// @returns: the number of characters written to the formatted data string, not including the terminating null character. a return value of -1 indicates that an encoding error has occurred
CRT_FORMAT_STRING_ATTRIBUTE( printf, 2, 3 )
int CRT::StringPrint( char* szBuffer, const char* const szFormat, ... )
{
	va_list args;
	va_start( args, szFormat );

	const int iResult = stbsp_vsprintf( szBuffer, szFormat, args );

	va_end( args );

	return iResult;
}

/// write formatted data to a string up to the specified count of characters, alternative of 'snprintf'
/// @remarks: format and store @a'nCount' or fewer characters in @a'szBuffer'. always store a terminating null character, truncating the output if necessary. if copying occurs between strings that overlap, the behavior is undefined
/// @returns: if the buffer size specified by @a'nCount' isn't sufficiently large to contain the output specified by @a'szFormat', the return value is the number of characters that would be written, not including the terminating null character, if @a'nCount' were sufficiently large. if the return value is greater than @a'nCount' - 1, the output has been truncated. a return value of -1 indicates that an encoding error has occurred
CRT_FORMAT_STRING_ATTRIBUTE( printf, 3, 4 )
int CRT::StringPrintN( char* szBuffer, std::size_t nCount, const char* const szFormat, ... )
{
	va_list args;
	va_start( args, szFormat );

	const int iResult = stbsp_vsnprintf( szBuffer, static_cast< int >( nCount ), szFormat, args );

	va_end( args );

	return iResult;

}
